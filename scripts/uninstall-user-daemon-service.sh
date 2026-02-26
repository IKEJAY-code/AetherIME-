#!/usr/bin/env bash
set -euo pipefail

SERVICE_NAME="aetherime-daemon.service"
SERVICE_FILE="${HOME}/.config/systemd/user/${SERVICE_NAME}"

if ! command -v systemctl >/dev/null 2>&1; then
  echo "系统缺少 systemctl，无法卸载 user service。"
  exit 1
fi

echo "==> [1/3] 停止并禁用服务"
systemctl --user disable --now "${SERVICE_NAME}" >/dev/null 2>&1 || true

echo "==> [2/3] 删除 service 文件"
rm -f "${SERVICE_FILE}"

echo "==> [3/3] 重载 user systemd"
systemctl --user daemon-reload
systemctl --user reset-failed >/dev/null 2>&1 || true

echo "✅ 已卸载 ${SERVICE_NAME}"
