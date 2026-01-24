use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ClientInfo {
    pub app: Option<String>,
    pub pid: Option<u32>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum ClientMessage {
    #[serde(rename = "suggest")]
    Suggest {
        request_id: String,
        context: String,
        cursor: usize,
        #[serde(default)]
        language_hint: String,
        max_len: usize,
        #[serde(default)]
        client: Option<ClientInfo>,
    },
    #[serde(rename = "cancel")]
    Cancel { request_id: String },
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum EngineMessage {
    #[serde(rename = "suggestion")]
    Suggestion {
        request_id: String,
        suggestion: String,
        confidence: f32,
        replace_range: [usize; 2],
    },
}

