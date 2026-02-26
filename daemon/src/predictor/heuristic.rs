use std::collections::HashMap;

use anyhow::Result;
use async_trait::async_trait;

use crate::predictor::{PredictionDraft, PredictorEngine};
use crate::protocol::{Language, PredictMode, PredictRequest, PredictionSource};

pub struct HeuristicPredictor {
    zh_next: HashMap<&'static str, &'static str>,
    en_next: HashMap<&'static str, &'static str>,
}

impl HeuristicPredictor {
    pub fn new() -> Self {
        let zh_next = HashMap::from([
            ("你好", "，很高兴见到你"),
            ("今天", "天气不错"),
            ("我想", "要"),
            ("我们", "可以先"),
            ("谢谢", "你的帮助"),
            ("请问", "现在方便吗"),
        ]);
        let en_next = HashMap::from([
            ("hello", ", how are you"),
            ("thank", " you"),
            ("let's", " start with"),
            ("could", " you please"),
            ("I need", " to"),
            ("please", " help me"),
        ]);

        Self { zh_next, en_next }
    }

    fn predict_next(&self, request: &PredictRequest) -> PredictionDraft {
        let lower_prefix = request.prefix.to_lowercase();
        let ghost_text = match request.language {
            Language::Zh => self
                .zh_next
                .iter()
                .find_map(|(key, value)| request.prefix.trim_end().ends_with(key).then(|| *value))
                .or_else(|| {
                    if request.prefix.ends_with('我') {
                        Some("们")
                    } else if request.prefix.ends_with("想") {
                        Some("要")
                    } else {
                        None
                    }
                })
                .unwrap_or(""),
            Language::En => self
                .en_next
                .iter()
                .find_map(|(key, value)| lower_prefix.ends_with(key).then(|| *value))
                .unwrap_or(""),
        }
        .to_string();

        let mut candidates = Vec::new();
        if !ghost_text.is_empty() {
            candidates.push(ghost_text.clone());
            if request.language == Language::Zh {
                candidates.push("继续".to_string());
                candidates.push("补充一下".to_string());
            } else {
                candidates.push(" and".to_string());
                candidates.push(" with details".to_string());
            }
        }

        PredictionDraft {
            ghost_text,
            candidates,
            confidence: 0.42,
            source: PredictionSource::LocalNext,
        }
    }

    fn predict_fim(&self, request: &PredictRequest) -> PredictionDraft {
        let ghost_text = if request.language == Language::Zh {
            match (request.prefix.as_str(), request.suffix.as_str()) {
                (prefix, suffix) if prefix.ends_with("我") && suffix.starts_with("吃饭") => {
                    "们一起去"
                }
                (prefix, suffix) if prefix.ends_with("今天") && suffix.starts_with("很好") => {
                    "心情"
                }
                (prefix, suffix) if prefix.ends_with("这个") && suffix.starts_with("问题") => {
                    "技术"
                }
                _ => "先",
            }
            .to_string()
        } else {
            match (request.prefix.to_lowercase(), request.suffix.to_lowercase()) {
                (prefix, suffix) if prefix.ends_with("we") && suffix.starts_with("build") => {
                    " can".to_string()
                }
                (prefix, suffix) if prefix.ends_with("please") && suffix.starts_with("review") => {
                    " quickly".to_string()
                }
                _ => " ".to_string(),
            }
        };

        let candidates = if ghost_text.trim().is_empty() {
            Vec::new()
        } else {
            vec![ghost_text.clone()]
        };

        PredictionDraft {
            ghost_text,
            candidates,
            confidence: 0.38,
            source: PredictionSource::LocalFim,
        }
    }
}

#[async_trait]
impl PredictorEngine for HeuristicPredictor {
    async fn predict(
        &self,
        request: &PredictRequest,
        mode: PredictMode,
    ) -> Result<PredictionDraft> {
        let draft = match mode {
            PredictMode::Next => self.predict_next(request),
            PredictMode::Fim => self.predict_fim(request),
        };
        Ok(draft)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::protocol::{Language, PredictMode, PredictRequest};

    #[tokio::test]
    async fn predicts_zh_next_phrase() {
        let predictor = HeuristicPredictor::new();
        let request = PredictRequest {
            prefix: "你好".to_string(),
            suffix: String::new(),
            language: Language::Zh,
            mode: PredictMode::Next,
            max_tokens: 12,
            latency_budget_ms: 90,
        };

        let result = predictor
            .predict(&request, PredictMode::Next)
            .await
            .unwrap();
        assert!(!result.ghost_text.is_empty());
        assert_eq!(result.source, PredictionSource::LocalNext);
    }
}
