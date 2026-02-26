use anyhow::{anyhow, Context, Result};
use async_trait::async_trait;
use reqwest::Client;
use serde::{Deserialize, Serialize};

use crate::config::ModelConfig;
use crate::predictor::{PredictionDraft, PredictorEngine};
use crate::protocol::{Language, PredictMode, PredictRequest, PredictionSource};

pub struct OllamaPredictor {
    base_url: String,
    model: String,
    temperature: f32,
    top_p: f32,
    client: Client,
}

impl OllamaPredictor {
    pub fn new(config: ModelConfig) -> Result<Self> {
        if config.ollama_model.trim().is_empty() {
            return Err(anyhow!(
                "model.backend is ollama but model.ollama_model is empty"
            ));
        }
        if config.ollama_host.trim().is_empty() {
            return Err(anyhow!(
                "model.backend is ollama but model.ollama_host is empty"
            ));
        }

        Ok(Self {
            base_url: config.ollama_host.trim_end_matches('/').to_string(),
            model: config.ollama_model,
            temperature: config.temperature,
            top_p: config.top_p,
            client: Client::builder()
                .build()
                .context("failed to build HTTP client")?,
        })
    }

    fn build_prompt(&self, request: &PredictRequest, mode: PredictMode) -> String {
        let language = match request.language {
            Language::Zh => "中文",
            Language::En => "English",
        };
        match mode {
            PredictMode::Fim if !request.suffix.trim().is_empty() => format!(
                "你是输入法幽灵补全引擎。仅输出需要插入中间的文本，不要解释，不要加引号。\n语言: {language}\n前文:\n{}\n后文:\n{}\n中间补全:",
                request.prefix, request.suffix
            ),
            _ => format!(
                "你是输入法幽灵补全引擎。仅输出接在末尾的连续文本，不要解释，不要加引号。\n语言: {language}\n当前文本:\n{}\n续写:",
                request.prefix
            ),
        }
    }

    async fn run_ollama(&self, request: &PredictRequest, mode: PredictMode) -> Result<String> {
        let endpoint = format!("{}/api/chat", self.base_url);
        let prompt = self.build_prompt(request, mode);
        let payload = OllamaChatRequest {
            model: self.model.clone(),
            stream: false,
            messages: vec![
                OllamaMessage {
                    role: "system".to_string(),
                    content: "你是输入法补全模型。输出必须是纯补全文本。".to_string(),
                },
                OllamaMessage {
                    role: "user".to_string(),
                    content: prompt,
                },
            ],
            options: OllamaOptions {
                temperature: self.temperature,
                top_p: self.top_p,
                num_predict: request.max_tokens.max(1),
            },
        };

        let response = self
            .client
            .post(endpoint)
            .json(&payload)
            .send()
            .await
            .context("failed to call ollama API")?;

        let status = response.status();
        let body = response
            .text()
            .await
            .context("failed to read ollama response body")?;

        if !status.is_success() {
            return Err(anyhow!("ollama API failed ({status}): {body}"));
        }

        let parsed: OllamaChatResponse =
            serde_json::from_str(&body).context("invalid ollama response format")?;
        Ok(parsed.message.content)
    }
}

fn sanitize_output(raw: &str, request: &PredictRequest) -> String {
    let mut text = raw.trim().to_string();
    text = text.trim_matches('`').trim_matches('"').trim().to_string();

    if text.starts_with(&request.prefix) {
        text = text[request.prefix.len()..].to_string();
    }

    if !request.suffix.is_empty() {
        if let Some(position) = text.find(&request.suffix) {
            text.truncate(position);
        }
    }

    if let Some(position) = text.find('\n') {
        text.truncate(position);
    }

    text.trim().to_string()
}

#[async_trait]
impl PredictorEngine for OllamaPredictor {
    async fn predict(
        &self,
        request: &PredictRequest,
        mode: PredictMode,
    ) -> Result<PredictionDraft> {
        let raw = self.run_ollama(request, mode).await?;
        let ghost_text = sanitize_output(&raw, request);
        if ghost_text.is_empty() {
            return Err(anyhow!("ollama returned empty prediction"));
        }

        Ok(PredictionDraft {
            candidates: vec![ghost_text.clone()],
            ghost_text,
            confidence: 0.71,
            source: match mode {
                PredictMode::Fim => PredictionSource::LocalFim,
                PredictMode::Next => PredictionSource::LocalNext,
            },
        })
    }
}

#[derive(Debug, Serialize)]
struct OllamaChatRequest {
    model: String,
    stream: bool,
    messages: Vec<OllamaMessage>,
    options: OllamaOptions,
}

#[derive(Debug, Serialize)]
struct OllamaMessage {
    role: String,
    content: String,
}

#[derive(Debug, Serialize)]
struct OllamaOptions {
    temperature: f32,
    top_p: f32,
    num_predict: u32,
}

#[derive(Debug, Deserialize)]
struct OllamaChatResponse {
    message: OllamaMessageResponse,
}

#[derive(Debug, Deserialize)]
struct OllamaMessageResponse {
    content: String,
}

#[cfg(test)]
mod tests {
    use super::sanitize_output;
    use crate::protocol::{Language, PredictMode, PredictRequest};

    fn make_request(prefix: &str, suffix: &str) -> PredictRequest {
        PredictRequest {
            prefix: prefix.to_string(),
            suffix: suffix.to_string(),
            language: Language::Zh,
            mode: PredictMode::Fim,
            max_tokens: 12,
            latency_budget_ms: 90,
        }
    }

    #[test]
    fn trims_fim_reply() {
        let request = make_request("我今天", "很好");
        let output = sanitize_output("心情很好", &request);
        assert_eq!(output, "心情");
    }

    #[test]
    fn removes_prefix_echo() {
        let request = make_request("你好", "");
        let output = sanitize_output("你好，今天过得怎么样", &request);
        assert_eq!(output, "，今天过得怎么样");
    }
}
