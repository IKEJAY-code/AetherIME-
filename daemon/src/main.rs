mod config;
mod predictor;
mod protocol;
mod server;

use anyhow::Result;
use config::DaemonConfig;
use predictor::PredictorRouter;
use server::PredictionServer;
use tracing::info;
use tracing_subscriber::EnvFilter;

#[tokio::main]
async fn main() -> Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter(EnvFilter::from_default_env().add_directive("info".parse()?))
        .init();

    let config = DaemonConfig::load()?;
    info!(
        socket = %config.server.socket_path.display(),
        backend = ?config.model.backend,
        default_mode = ?config.model.mode,
        predict_enabled = config.predict.enable,
        trigger_delay_ms = config.predict.trigger_delay_ms,
        default_max_tokens = config.predict.max_tokens,
        local_only = config.privacy.local_only,
        cloud_enabled = !config.privacy.cloud_endpoint.is_empty(),
        theme = %config.ui.theme,
        accept_hotkey = %config.hotkey.accept,
        toggle_hotkey = %config.hotkey.toggle_predict,
        "loaded aetherime config"
    );
    let predictor = PredictorRouter::new(config.model.clone(), config.predict.clone());
    let server = PredictionServer::new(config.server.clone(), predictor);
    server.run().await
}
