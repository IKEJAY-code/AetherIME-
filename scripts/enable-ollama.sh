#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CONFIG_FILE="${ROOT_DIR}/config/aetherime.toml"
MODEL="${1:-qwen2.5:3b}"

if ! command -v ollama >/dev/null 2>&1; then
  echo "Ollama was not found. Install it first: https://ollama.com/download/linux"
  exit 1
fi

if [[ ! -f "${CONFIG_FILE}" ]]; then
  echo "Config file not found: ${CONFIG_FILE}"
  exit 1
fi

echo "==> Pulling Ollama model: ${MODEL}"
ollama pull "${MODEL}"

echo "==> Updating AetherIME config: ${CONFIG_FILE}"
sed -i -E 's/^request_timeout_ms = [0-9]+/request_timeout_ms = 5000/' "${CONFIG_FILE}"
sed -i -E 's/^backend = ".+"/backend = "ollama"/' "${CONFIG_FILE}"
sed -i -E 's/^mode = ".+"/mode = "fim"/' "${CONFIG_FILE}"
sed -i -E 's|^ollama_host = ".*"|ollama_host = "http://127.0.0.1:11434"|' "${CONFIG_FILE}"
sed -i -E "s|^ollama_model = \".*\"|ollama_model = \"${MODEL}\"|" "${CONFIG_FILE}"

echo
echo "✅ Switched to Ollama backend."
echo "Start daemon with:"
echo "  AETHERIME_CONFIG=${CONFIG_FILE} cargo run -p aetherime-daemon"
