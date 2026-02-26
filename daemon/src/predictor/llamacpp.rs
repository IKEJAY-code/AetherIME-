use std::process::Stdio;

use anyhow::{anyhow, Context, Result};
use async_trait::async_trait;
use tokio::process::Command;
use tokio::time::{timeout, Duration};

use crate::config::ModelConfig;
use crate::predictor::{PredictionDraft, PredictorEngine};
use crate::protocol::{PredictMode, PredictRequest, PredictionSource};

pub struct LlamaCppPredictor {
    model_path: String,
    cli_path: String,
    ctx_len: u32,
    temperature: f32,
    top_p: f32,
}

impl LlamaCppPredictor {
    pub fn new(config: ModelConfig) -> Result<Self> {
        if config.model_path.trim().is_empty() {
            return Err(anyhow!(
                "model.backend is llamacpp but model.model_path is empty"
            ));
        }
        Ok(Self {
            model_path: config.model_path,
            cli_path: config.llama_cli_path,
            ctx_len: config.ctx_len,
            temperature: config.temperature,
            top_p: config.top_p,
        })
    }

    fn build_prompt(&self, request: &PredictRequest, mode: PredictMode) -> String {
        match mode {
            PredictMode::Fim => format!(
                "<fim_prefix>{}<fim_suffix>{}<fim_middle>",
                request.prefix, request.suffix
            ),
            PredictMode::Next => request.prefix.clone(),
        }
    }

    async fn run_llama_cli(&self, request: &PredictRequest, mode: PredictMode) -> Result<String> {
        let prompt = self.build_prompt(request, mode);
        let mut command = Command::new(&self.cli_path);
        command
            .arg("-m")
            .arg(&self.model_path)
            .arg("-n")
            .arg(request.max_tokens.to_string())
            .arg("-c")
            .arg(self.ctx_len.to_string())
            .arg("--temp")
            .arg(self.temperature.to_string())
            .arg("--top-p")
            .arg(self.top_p.to_string())
            .arg("-p")
            .arg(prompt)
            .arg("--no-display-prompt")
            .stdout(Stdio::piped())
            .stderr(Stdio::piped());

        let output = timeout(
            Duration::from_millis(request.latency_budget_ms),
            command.output(),
        )
        .await
        .context("llama.cpp command timed out")?
        .context("failed to execute llama.cpp")?;

        if !output.status.success() {
            let stderr = String::from_utf8_lossy(&output.stderr).trim().to_string();
            return Err(anyhow!(
                "llama.cpp exited with {}: {}",
                output.status,
                stderr
            ));
        }

        let stdout = String::from_utf8(output.stdout).context("llama.cpp stdout is not UTF-8")?;
        Ok(stdout.trim().to_string())
    }
}

#[async_trait]
impl PredictorEngine for LlamaCppPredictor {
    async fn predict(
        &self,
        request: &PredictRequest,
        mode: PredictMode,
    ) -> Result<PredictionDraft> {
        let raw = self.run_llama_cli(request, mode).await?;
        let ghost_text = raw.lines().next().unwrap_or("").trim().to_string();

        if ghost_text.is_empty() {
            return Err(anyhow!("llama.cpp returned empty prediction"));
        }

        Ok(PredictionDraft {
            candidates: vec![ghost_text.clone()],
            ghost_text,
            confidence: 0.66,
            source: match mode {
                PredictMode::Fim => PredictionSource::LocalFim,
                PredictMode::Next => PredictionSource::LocalNext,
            },
        })
    }
}
