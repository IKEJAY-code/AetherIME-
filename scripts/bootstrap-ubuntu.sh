#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -eq 0 ]]; then
  echo "请不要直接用 root 运行。用普通用户执行本脚本，它会按需调用 sudo。"
  exit 1
fi

if ! command -v apt-get >/dev/null 2>&1; then
  echo "当前系统不是 Ubuntu/Debian（缺少 apt-get），请改用手动安装。"
  exit 1
fi

if ! command -v sudo >/dev/null 2>&1; then
  echo "系统缺少 sudo，请先安装 sudo。"
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-fcitx5-ubuntu"
GENERATOR="Ninja"
INSTALL_PREFIX="/usr"

APT_PACKAGES=(
  build-essential
  cmake
  ninja-build
  pkg-config
  libboost-dev
  gettext
  im-config
  fcitx5
  fcitx5-config-qt
  fcitx5-frontend-gtk3
  fcitx5-frontend-gtk4
  fcitx5-frontend-qt5
  fcitx5-frontend-qt6
  fcitx5-frontend-all
  fcitx5-chinese-addons
  libime-data
  libime-data-language-model
  libimecore-dev
  libimepinyin-dev
  libfcitx5core-dev
  libfcitx5utils-dev
  libfcitx5config-dev
  fcitx5-modules-dev
)

is_pkg_installed() {
  local pkg="$1"
  dpkg-query -W -f='${Status}\n' "${pkg}" 2>/dev/null | grep -q '^install ok installed$'
}

echo "==> [1/7] 安装 Ubuntu 依赖包"
sudo apt-get update
APT_FAILED=0
if ! sudo apt-get install -y "${APT_PACKAGES[@]}"; then
  APT_FAILED=1
fi

MISSING=()
for pkg in "${APT_PACKAGES[@]}"; do
  if ! is_pkg_installed "${pkg}"; then
    MISSING+=("${pkg}")
  fi
done

if (( ${#MISSING[@]} > 0 )); then
  echo "❌ 依赖包安装不完整，缺少: ${MISSING[*]}"
  echo "请先修复系统包状态，再重试："
  echo "  sudo dpkg --configure -a"
  echo "  sudo apt-get -f install"
  exit 1
fi

if (( APT_FAILED == 1 )); then
  echo "⚠️ apt 返回了错误，但 AetherIME 所需依赖已安装，继续执行后续步骤。"
fi

echo "==> [2/7] 设置系统输入法框架为 fcitx5"
if command -v im-config >/dev/null 2>&1; then
  im-config -n fcitx5 || true
fi

echo "==> [3/7] 写入环境变量 (~/.config/environment.d/99-fcitx5.conf)"
mkdir -p "${HOME}/.config/environment.d"
cat >"${HOME}/.config/environment.d/99-fcitx5.conf" <<'EOF'
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
SDL_IM_MODULE=fcitx
EOF

echo "==> [4/7] 编译 AetherIME Fcitx5 插件"
if [[ -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
  CURRENT_GENERATOR="$(sed -n 's/^CMAKE_GENERATOR:INTERNAL=//p' "${BUILD_DIR}/CMakeCache.txt" | head -n1 || true)"
  if [[ -n "${CURRENT_GENERATOR}" && "${CURRENT_GENERATOR}" != "${GENERATOR}" ]]; then
    echo "检测到 ${BUILD_DIR} 使用了不同生成器(${CURRENT_GENERATOR})，清理后重建。"
    rm -rf "${BUILD_DIR}"
  fi
fi

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G "${GENERATOR}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"
cmake --build "${BUILD_DIR}" -j"$(nproc)"

if ! grep -Rqs "AETHERIME_HAS_LIBIME=1" "${BUILD_DIR}"; then
  echo "⚠️ 当前构建未启用 libime 完整拼音，正在使用 fallback 小词表。"
  echo "请确认已安装: libboost-dev libimecore-dev libimepinyin-dev libime-data libime-data-language-model"
fi

echo "==> [5/7] 清理旧的 /usr/local 安装残留"
sudo rm -f \
  /usr/local/share/fcitx5/addon/aetherime.conf \
  /usr/local/share/fcitx5/inputmethod/aetherime.conf \
  /usr/local/lib/fcitx5/libaetherime.so \
  /usr/local/lib64/fcitx5/libaetherime.so \
  /usr/local/lib/x86_64-linux-gnu/fcitx5/libaetherime.so \
  /usr/local/lib/aarch64-linux-gnu/fcitx5/libaetherime.so

echo "==> [6/7] 安装到系统路径（需要 sudo）"
sudo cmake --install "${BUILD_DIR}"

echo "==> [7/7] 尝试重启 fcitx5（当前会话可用时）"
pkill -x fcitx5 >/dev/null 2>&1 || true
if [[ -n "${DISPLAY:-}" || -n "${WAYLAND_DISPLAY:-}" ]]; then
  nohup fcitx5 -d >/dev/null 2>&1 || true
fi

PLUGIN_PATH="$(find /usr/lib /usr/lib64 -type f -name libaetherime.so 2>/dev/null | head -n1 || true)"
if [[ -n "${PLUGIN_PATH}" ]]; then
  if ldd "${PLUGIN_PATH}" | grep -q "not found"; then
    echo "⚠️ 检测到插件依赖缺失，请执行: ldd ${PLUGIN_PATH}"
  fi
fi

cat <<'EOF'

✅ 已完成一键安装。

接下来请做两步：
1) 注销并重新登录（确保环境变量生效）
2) 打开 `fcitx5-configtool` -> 添加输入法 -> 搜索并启用 "AetherIME"

可选检查：
- `fcitx5-diagnose | less`
- `ls /usr/lib*/fcitx5/libaetherime.so`
- `ls /usr/share/fcitx5/inputmethod/aetherime.conf`

如果 daemon 还没启动：
- `AETHERIME_CONFIG=./config/aetherime.toml cargo run -p aetherime-daemon`
EOF
