#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
SERVICE_NAME="aetherime-daemon.service"
SERVICE_DIR="${HOME}/.config/systemd/user"
SERVICE_FILE="${SERVICE_DIR}/${SERVICE_NAME}"

CONFIG_PATH="${1:-${ROOT_DIR}/config/aetherime.toml}"
if [[ "${CONFIG_PATH}" != /* ]]; then
  CONFIG_PATH="$(realpath "${PWD}/${CONFIG_PATH}")"
fi

if [[ ! -f "${CONFIG_PATH}" ]]; then
  echo "配置文件不存在: ${CONFIG_PATH}"
  exit 1
fi

if ! command -v systemctl >/dev/null 2>&1; then
  echo "系统缺少 systemctl，无法安装 user service。"
  exit 1
fi

echo "==> [1/4] 构建 release daemon"
cd "${ROOT_DIR}"
cargo build --release -p aetherime-daemon
BIN_PATH="${ROOT_DIR}/target/release/aetherime-daemon"

if [[ ! -x "${BIN_PATH}" ]]; then
  echo "未找到可执行文件: ${BIN_PATH}"
  exit 1
fi

echo "==> [2/4] 写入 systemd user service"
mkdir -p "${SERVICE_DIR}"
cat >"${SERVICE_FILE}" <<EOF
[Unit]
Description=AetherIME AI Input Method Daemon
After=default.target

[Service]
Type=simple
WorkingDirectory=${ROOT_DIR}
Environment=AETHERIME_CONFIG=${CONFIG_PATH}
ExecStart=${BIN_PATH}
Restart=on-failure
RestartSec=1

[Install]
WantedBy=default.target
EOF

echo "==> [3/4] 启用并启动 user service"
systemctl --user daemon-reload
systemctl --user enable --now "${SERVICE_NAME}"

echo "==> [4/4] 服务状态"
systemctl --user --no-pager --full status "${SERVICE_NAME}" || true

cat <<EOF

✅ 已安装并启动 ${SERVICE_NAME}

常用命令：
- 查看日志：journalctl --user -u ${SERVICE_NAME} -f
- 重启服务：systemctl --user restart ${SERVICE_NAME}
- 停止服务：systemctl --user stop ${SERVICE_NAME}
- 卸载服务：./scripts/uninstall-user-daemon-service.sh
EOF
