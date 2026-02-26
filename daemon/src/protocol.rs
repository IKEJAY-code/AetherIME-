use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DaemonRequest {
    #[serde(default)]
    pub id: String,
    #[serde(flatten)]
    pub body: RequestBody,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum RequestBody {
    Predict(PredictRequest),
    Ping,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DaemonResponse {
    #[serde(default)]
    pub id: String,
    #[serde(flatten)]
    pub body: ResponseBody,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum ResponseBody {
    Predict(PredictResponse),
    Pong,
    Error(ErrorResponse),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ErrorResponse {
    pub code: ErrorCode,
    pub message: String,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum ErrorCode {
    InvalidRequest,
    Timeout,
    Internal,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PredictRequest {
    pub prefix: String,
    #[serde(default)]
    pub suffix: String,
    #[serde(default)]
    pub language: Language,
    #[serde(default)]
    pub mode: PredictMode,
    #[serde(default = "default_max_tokens")]
    pub max_tokens: u32,
    #[serde(default = "default_latency_budget_ms")]
    pub latency_budget_ms: u64,
}

impl PredictRequest {
    pub fn normalized(mut self) -> Self {
        if self.max_tokens == 0 {
            self.max_tokens = default_max_tokens();
        }
        if self.latency_budget_ms == 0 {
            self.latency_budget_ms = default_latency_budget_ms();
        }
        self
    }
}

fn default_max_tokens() -> u32 {
    12
}

fn default_latency_budget_ms() -> u64 {
    90
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, Default, PartialEq, Eq, Hash)]
#[serde(rename_all = "lowercase")]
pub enum Language {
    #[default]
    Zh,
    En,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, Default, PartialEq, Eq, Hash)]
#[serde(rename_all = "lowercase")]
pub enum PredictMode {
    Next,
    #[default]
    Fim,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PredictResponse {
    pub ghost_text: String,
    pub candidates: Vec<String>,
    pub confidence: f32,
    pub source: PredictionSource,
    pub elapsed_ms: u64,
}

impl PredictResponse {
    pub fn empty(source: PredictionSource, elapsed_ms: u64) -> Self {
        Self {
            ghost_text: String::new(),
            candidates: Vec::new(),
            confidence: 0.0,
            source,
            elapsed_ms,
        }
    }
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum PredictionSource {
    LocalFim,
    LocalNext,
    Cloud,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_predict_request() {
        let raw = r#"{"id":"abc","type":"predict","prefix":"你好","suffix":"","language":"zh","mode":"next","max_tokens":6,"latency_budget_ms":80}"#;
        let request: DaemonRequest = serde_json::from_str(raw).unwrap();
        assert_eq!(request.id, "abc");
        match request.body {
            RequestBody::Predict(payload) => {
                assert_eq!(payload.prefix, "你好");
                assert_eq!(payload.mode, PredictMode::Next);
                assert_eq!(payload.language, Language::Zh);
            }
            _ => panic!("expected predict request"),
        }
    }
}
