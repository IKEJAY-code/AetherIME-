#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

export AETHERIME_CONFIG="${AETHERIME_CONFIG:-${ROOT_DIR}/config/aetherime.toml}"
cd "${ROOT_DIR}"
exec cargo run -p aetherime-daemon
