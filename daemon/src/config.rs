use std::env;
use std::fs;
use std::path::{Path, PathBuf};

use anyhow::{Context, Result};
use serde::Deserialize;

#[derive(Debug, Clone, Deserialize, Default)]
pub struct DaemonConfig {
    #[serde(default)]
    pub server: ServerConfig,
    #[serde(default)]
    pub predict: PredictConfig,
    #[serde(default)]
    pub model: ModelConfig,
    #[serde(default)]
    pub privacy: PrivacyConfig,
    #[serde(default)]
    pub ui: UiConfig,
    #[serde(default)]
    pub hotkey: HotkeyConfig,
}

impl DaemonConfig {
    pub fn load() -> Result<Self> {
        let config_path = resolve_config_path();
        if config_path.exists() {
            let raw = fs::read_to_string(&config_path)
                .with_context(|| format!("failed to read config file {}", config_path.display()))?;
            let parsed: DaemonConfig = toml::from_str(&raw)
                .with_context(|| format!("failed to parse TOML from {}", config_path.display()))?;
            return Ok(parsed);
        }

        Ok(DaemonConfig::default())
    }
}

fn resolve_config_path() -> PathBuf {
    if let Ok(path) = env::var("AETHERIME_CONFIG") {
        return Path::new(&path).to_path_buf();
    }

    if let Some(base) = dirs::config_dir() {
        return base.join("aetherime").join("config.toml");
    }

    Path::new("/tmp/aetherime.toml").to_path_buf()
}

#[derive(Debug, Clone, Deserialize)]
pub struct ServerConfig {
    #[serde(default = "default_socket_path")]
    pub socket_path: PathBuf,
    #[serde(default = "default_request_timeout_ms")]
    pub request_timeout_ms: u64,
}

impl Default for ServerConfig {
    fn default() -> Self {
        Self {
            socket_path: default_socket_path(),
            request_timeout_ms: default_request_timeout_ms(),
        }
    }
}

fn default_socket_path() -> PathBuf {
    Path::new("/tmp/aetherime.sock").to_path_buf()
}

fn default_request_timeout_ms() -> u64 {
    120
}

#[derive(Debug, Clone, Deserialize)]
pub struct PredictConfig {
    #[serde(default = "default_predict_enabled")]
    pub enable: bool,
    #[serde(default = "default_trigger_delay_ms")]
    pub trigger_delay_ms: u64,
    #[serde(default = "default_max_tokens")]
    pub max_tokens: u32,
    #[serde(default = "default_cache_capacity")]
    pub cache_capacity: usize,
}

impl Default for PredictConfig {
    fn default() -> Self {
        Self {
            enable: default_predict_enabled(),
            trigger_delay_ms: default_trigger_delay_ms(),
            max_tokens: default_max_tokens(),
            cache_capacity: default_cache_capacity(),
        }
    }
}

fn default_predict_enabled() -> bool {
    true
}

fn default_trigger_delay_ms() -> u64 {
    35
}

fn default_max_tokens() -> u32 {
    12
}

fn default_cache_capacity() -> usize {
    512
}

#[derive(Debug, Clone, Deserialize)]
pub struct ModelConfig {
    #[serde(default = "default_backend")]
    pub backend: ModelBackend,
    #[serde(default = "default_mode")]
    pub mode: DefaultMode,
    #[serde(default)]
    pub model_path: String,
    #[serde(default = "default_ollama_host")]
    pub ollama_host: String,
    #[serde(default)]
    pub ollama_model: String,
    #[serde(default = "default_ctx_len")]
    pub ctx_len: u32,
    #[serde(default = "default_temperature")]
    pub temperature: f32,
    #[serde(default = "default_top_p")]
    pub top_p: f32,
    #[serde(default = "default_llama_cli_path")]
    pub llama_cli_path: String,
}

impl Default for ModelConfig {
    fn default() -> Self {
        Self {
            backend: default_backend(),
            mode: default_mode(),
            model_path: String::new(),
            ollama_host: default_ollama_host(),
            ollama_model: String::new(),
            ctx_len: default_ctx_len(),
            temperature: default_temperature(),
            top_p: default_top_p(),
            llama_cli_path: default_llama_cli_path(),
        }
    }
}

#[derive(Debug, Clone, Copy, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum ModelBackend {
    Heuristic,
    Llamacpp,
    Ollama,
}

fn default_backend() -> ModelBackend {
    ModelBackend::Heuristic
}

#[derive(Debug, Clone, Copy, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum DefaultMode {
    Next,
    Fim,
}

fn default_mode() -> DefaultMode {
    DefaultMode::Fim
}

fn default_ctx_len() -> u32 {
    1024
}

fn default_ollama_host() -> String {
    "http://127.0.0.1:11434".to_string()
}

fn default_temperature() -> f32 {
    0.2
}

fn default_top_p() -> f32 {
    0.9
}

fn default_llama_cli_path() -> String {
    "llama-cli".to_string()
}

#[derive(Debug, Clone, Deserialize)]
pub struct PrivacyConfig {
    #[serde(default = "default_local_only")]
    pub local_only: bool,
    #[serde(default)]
    pub cloud_endpoint: String,
}

impl Default for PrivacyConfig {
    fn default() -> Self {
        Self {
            local_only: default_local_only(),
            cloud_endpoint: String::new(),
        }
    }
}

fn default_local_only() -> bool {
    true
}

#[derive(Debug, Clone, Deserialize)]
pub struct UiConfig {
    #[serde(default = "default_theme")]
    pub theme: String,
}

impl Default for UiConfig {
    fn default() -> Self {
        Self {
            theme: default_theme(),
        }
    }
}

fn default_theme() -> String {
    "deep-ocean".to_string()
}

#[derive(Debug, Clone, Deserialize)]
pub struct HotkeyConfig {
    #[serde(default = "default_accept_hotkey")]
    pub accept: String,
    #[serde(default = "default_toggle_predict_hotkey")]
    pub toggle_predict: String,
}

impl Default for HotkeyConfig {
    fn default() -> Self {
        Self {
            accept: default_accept_hotkey(),
            toggle_predict: default_toggle_predict_hotkey(),
        }
    }
}

fn default_accept_hotkey() -> String {
    "Tab".to_string()
}

fn default_toggle_predict_hotkey() -> String {
    "Ctrl+;".to_string()
}
