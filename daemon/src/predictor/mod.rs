mod heuristic;
mod llamacpp;
mod ollama;

use std::collections::{HashMap, VecDeque};
use std::sync::Arc;
use std::time::Instant;

use anyhow::Result;
use async_trait::async_trait;
pub use heuristic::HeuristicPredictor;
use tokio::sync::RwLock;
use tracing::warn;

use crate::config::{DefaultMode, ModelBackend, ModelConfig, PredictConfig};
use crate::protocol::{Language, PredictMode, PredictRequest, PredictResponse, PredictionSource};

#[async_trait]
pub trait PredictorEngine: Send + Sync {
    async fn predict(&self, request: &PredictRequest, mode: PredictMode)
        -> Result<PredictionDraft>;
}

#[derive(Debug, Clone)]
pub struct PredictionDraft {
    pub ghost_text: String,
    pub candidates: Vec<String>,
    pub confidence: f32,
    pub source: PredictionSource,
}

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
struct CacheKey {
    prefix: String,
    suffix: String,
    language: Language,
    mode: PredictMode,
    max_tokens: u32,
}

#[derive(Debug)]
struct PredictCache {
    capacity: usize,
    map: HashMap<CacheKey, PredictResponse>,
    order: VecDeque<CacheKey>,
}

impl PredictCache {
    fn new(capacity: usize) -> Self {
        Self {
            capacity,
            map: HashMap::new(),
            order: VecDeque::new(),
        }
    }

    fn get(&self, key: &CacheKey) -> Option<PredictResponse> {
        self.map.get(key).cloned()
    }

    fn insert(&mut self, key: CacheKey, value: PredictResponse) {
        if self.capacity == 0 {
            return;
        }
        if self.map.contains_key(&key) {
            self.map.insert(key, value);
            return;
        }
        if self.map.len() == self.capacity {
            if let Some(front) = self.order.pop_front() {
                self.map.remove(&front);
            }
        }
        self.order.push_back(key.clone());
        self.map.insert(key, value);
    }
}

pub struct PredictorRouter {
    primary: Arc<dyn PredictorEngine>,
    fallback: Arc<HeuristicPredictor>,
    default_mode: PredictMode,
    enabled: bool,
    cache: RwLock<PredictCache>,
}

impl PredictorRouter {
    pub fn new(model: ModelConfig, predict: PredictConfig) -> Self {
        let fallback = Arc::new(HeuristicPredictor::new());
        let primary: Arc<dyn PredictorEngine> = match model.backend {
            ModelBackend::Heuristic => fallback.clone(),
            ModelBackend::Llamacpp => match llamacpp::LlamaCppPredictor::new(model.clone()) {
                Ok(predictor) => Arc::new(predictor),
                Err(error) => {
                    warn!("failed to init llama.cpp backend: {error:#}");
                    fallback.clone()
                }
            },
            ModelBackend::Ollama => match ollama::OllamaPredictor::new(model.clone()) {
                Ok(predictor) => Arc::new(predictor),
                Err(error) => {
                    warn!("failed to init ollama backend: {error:#}");
                    fallback.clone()
                }
            },
        };
        let default_mode = match model.mode {
            DefaultMode::Next => PredictMode::Next,
            DefaultMode::Fim => PredictMode::Fim,
        };

        Self {
            primary,
            fallback,
            default_mode,
            enabled: predict.enable,
            cache: RwLock::new(PredictCache::new(predict.cache_capacity)),
        }
    }

    pub async fn predict(&self, request: PredictRequest) -> PredictResponse {
        if !self.enabled {
            return PredictResponse::empty(PredictionSource::LocalNext, 0);
        }
        let request = request.normalized();
        if request.prefix.trim().is_empty() {
            return PredictResponse::empty(PredictionSource::LocalNext, 0);
        }

        let effective_mode =
            if matches!(request.mode, PredictMode::Fim) && request.suffix.trim().is_empty() {
                match self.default_mode {
                    PredictMode::Fim => PredictMode::Next,
                    PredictMode::Next => PredictMode::Next,
                }
            } else {
                request.mode
            };

        let cache_key = CacheKey {
            prefix: request.prefix.clone(),
            suffix: request.suffix.clone(),
            language: request.language,
            mode: effective_mode,
            max_tokens: request.max_tokens,
        };

        if let Some(cached) = self.cache.read().await.get(&cache_key) {
            return cached;
        }

        let started = Instant::now();
        let draft = match self.primary.predict(&request, effective_mode).await {
            Ok(value) => value,
            Err(error) => {
                warn!("primary predictor failed, fallback to heuristic: {error:#}");
                self.fallback
                    .predict(&request, effective_mode)
                    .await
                    .unwrap_or(PredictionDraft {
                        ghost_text: String::new(),
                        candidates: Vec::new(),
                        confidence: 0.0,
                        source: match effective_mode {
                            PredictMode::Fim => PredictionSource::LocalFim,
                            PredictMode::Next => PredictionSource::LocalNext,
                        },
                    })
            }
        };
        let elapsed_ms = started.elapsed().as_millis() as u64;

        let response = PredictResponse {
            ghost_text: draft.ghost_text,
            candidates: draft.candidates,
            confidence: draft.confidence,
            source: draft.source,
            elapsed_ms,
        };
        self.cache.write().await.insert(cache_key, response.clone());
        response
    }
}
