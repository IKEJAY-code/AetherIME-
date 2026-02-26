use std::path::Path;
use std::sync::Arc;

use anyhow::{Context, Result};
use tokio::fs;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::{UnixListener, UnixStream};
use tokio::time::{timeout, Duration};
use tracing::{error, info, warn};

use crate::config::ServerConfig;
use crate::predictor::PredictorRouter;
use crate::protocol::{
    DaemonRequest, DaemonResponse, ErrorCode, ErrorResponse, RequestBody, ResponseBody,
};

pub struct PredictionServer {
    config: ServerConfig,
    predictor: Arc<PredictorRouter>,
}

impl PredictionServer {
    pub fn new(config: ServerConfig, predictor: PredictorRouter) -> Self {
        Self {
            config,
            predictor: Arc::new(predictor),
        }
    }

    pub async fn run(&self) -> Result<()> {
        self.prepare_socket_path().await?;
        if self.config.socket_path.exists() {
            fs::remove_file(&self.config.socket_path)
                .await
                .with_context(|| {
                    format!(
                        "failed to cleanup stale socket {}",
                        self.config.socket_path.display()
                    )
                })?;
        }

        let listener = UnixListener::bind(&self.config.socket_path).with_context(|| {
            format!(
                "failed to bind unix socket at {}",
                self.config.socket_path.display()
            )
        })?;
        info!(
            "aetherime daemon listening on {}",
            self.config.socket_path.display()
        );

        loop {
            let (stream, _) = listener.accept().await?;
            let predictor = self.predictor.clone();
            let timeout_ms = self.config.request_timeout_ms;
            tokio::spawn(async move {
                if let Err(error) = handle_connection(stream, predictor, timeout_ms).await {
                    warn!("connection closed with error: {error:#}");
                }
            });
        }
    }

    async fn prepare_socket_path(&self) -> Result<()> {
        if let Some(parent) = Path::new(&self.config.socket_path).parent() {
            fs::create_dir_all(parent).await.with_context(|| {
                format!("failed to create socket directory {}", parent.display())
            })?;
        }
        Ok(())
    }
}

async fn handle_connection(
    stream: UnixStream,
    predictor: Arc<PredictorRouter>,
    timeout_ms: u64,
) -> Result<()> {
    let (reader, mut writer) = stream.into_split();
    let mut lines = BufReader::new(reader).lines();

    while let Some(line) = lines.next_line().await? {
        if line.trim().is_empty() {
            continue;
        }
        let response = process_line(line, predictor.clone(), timeout_ms).await;
        let payload = serde_json::to_string(&response)?;
        writer.write_all(payload.as_bytes()).await?;
        writer.write_all(b"\n").await?;
    }
    Ok(())
}

async fn process_line(
    line: String,
    predictor: Arc<PredictorRouter>,
    timeout_ms: u64,
) -> DaemonResponse {
    match serde_json::from_str::<DaemonRequest>(&line) {
        Ok(request) => handle_request(request, predictor, timeout_ms).await,
        Err(error) => {
            error!("invalid request JSON: {error}");
            DaemonResponse {
                id: String::new(),
                body: ResponseBody::Error(ErrorResponse {
                    code: ErrorCode::InvalidRequest,
                    message: format!("invalid JSON payload: {error}"),
                }),
            }
        }
    }
}

async fn handle_request(
    request: DaemonRequest,
    predictor: Arc<PredictorRouter>,
    timeout_ms: u64,
) -> DaemonResponse {
    let id = request.id;
    match request.body {
        RequestBody::Ping => DaemonResponse {
            id,
            body: ResponseBody::Pong,
        },
        RequestBody::Predict(predict_request) => {
            let effective_timeout_ms = timeout_ms.max(predict_request.latency_budget_ms).max(1);
            match timeout(
                Duration::from_millis(effective_timeout_ms),
                predictor.predict(predict_request),
            )
            .await
            {
                Ok(prediction) => DaemonResponse {
                    id,
                    body: ResponseBody::Predict(prediction),
                },
                Err(_) => DaemonResponse {
                    id,
                    body: ResponseBody::Error(ErrorResponse {
                        code: ErrorCode::Timeout,
                        message: format!("prediction exceeded {}ms", effective_timeout_ms),
                    }),
                },
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::config::{ModelConfig, PredictConfig};
    use crate::protocol::{Language, PredictMode, PredictRequest};

    #[tokio::test]
    async fn handles_ping() {
        let predictor = Arc::new(PredictorRouter::new(
            ModelConfig::default(),
            PredictConfig::default(),
        ));
        let request = DaemonRequest {
            id: "1".to_string(),
            body: RequestBody::Ping,
        };

        let response = handle_request(request, predictor, 100).await;
        assert!(matches!(response.body, ResponseBody::Pong));
        assert_eq!(response.id, "1");
    }

    #[tokio::test]
    async fn handles_predict() {
        let predictor = Arc::new(PredictorRouter::new(
            ModelConfig::default(),
            PredictConfig::default(),
        ));
        let request = DaemonRequest {
            id: "2".to_string(),
            body: RequestBody::Predict(PredictRequest {
                prefix: "你好".to_string(),
                suffix: String::new(),
                language: Language::Zh,
                mode: PredictMode::Next,
                max_tokens: 8,
                latency_budget_ms: 50,
            }),
        };

        let response = handle_request(request, predictor, 100).await;
        match response.body {
            ResponseBody::Predict(prediction) => {
                assert!(!prediction.ghost_text.is_empty());
            }
            other => panic!("unexpected response: {other:?}"),
        }
    }
}
