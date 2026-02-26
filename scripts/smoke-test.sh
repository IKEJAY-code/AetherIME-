#!/usr/bin/env bash
set -euo pipefail

SOCKET_PATH="${1:-/tmp/aetherime.sock}"

if ! command -v socat >/dev/null 2>&1; then
  echo "socat is required for smoke test"
  exit 1
fi

echo "-> ping"
printf '%s\n' '{"id":"1","type":"ping"}' | socat - UNIX-CONNECT:"${SOCKET_PATH}"

echo "-> predict"
printf '%s\n' '{"id":"2","type":"predict","prefix":"你好","suffix":"","language":"zh","mode":"next","max_tokens":8,"latency_budget_ms":90}' \
  | socat - UNIX-CONNECT:"${SOCKET_PATH}"
