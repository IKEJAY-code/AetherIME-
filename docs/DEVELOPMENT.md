# Development Guide

## Prerequisites

- Rust stable toolchain
- CMake + C++17 compiler
- Boost headers (`libboost-dev`)
- Fcitx5 development packages (`fcitx5-core`, `fcitx5-utils`)
- LibIME packages (`libime-data`, `libime-data-language-model`, `libimecore-dev`, `libimepinyin-dev`)

## Build Daemon

```bash
cargo build -p aetherime-daemon
```

## Build Fcitx5 Addon

```bash
cmake -S . -B build
cmake --build build -j
```

Generated install artifacts:

- addon descriptor: `share/fcitx5/addon/aetherime.conf`
- input method descriptor: `share/fcitx5/inputmethod/aetherime.conf`
- plugin library: `lib/fcitx5/libaetherime.so` (libdir varies by distro)

## Ubuntu Bootstrap

```bash
./scripts/bootstrap-ubuntu.sh
```

This installs dependencies, builds and installs the addon, and writes IM environment variables.

## Enable Ollama Backend

```bash
./scripts/enable-ollama.sh
```

## Run Daemon (Foreground)

```bash
AETHERIME_CONFIG=./config/aetherime.toml cargo run -p aetherime-daemon
```

## Run Daemon as User Service

```bash
./scripts/install-user-daemon-service.sh
systemctl --user status aetherime-daemon.service
```

## IPC Smoke Test

```bash
printf '%s\n' '{"id":"1","type":"ping"}' | socat - UNIX-CONNECT:/tmp/aetherime.sock
printf '%s\n' '{"id":"2","type":"predict","prefix":"hello","suffix":"","language":"en","mode":"next","max_tokens":8,"latency_budget_ms":5000}' | socat - UNIX-CONNECT:/tmp/aetherime.sock
```

## Important Config Knobs

- `server.request_timeout_ms`: global daemon timeout budget
- `predict.cache_capacity`: response cache size
- `model.backend`: `heuristic` / `llamacpp` / `ollama`
- `model.model_path`: GGUF path (required for `llamacpp`)
- `model.ollama_host`: Ollama endpoint (`http://127.0.0.1:11434`)
- `model.ollama_model`: e.g. `qwen2.5:3b`

## Runtime Environment Variables

- `AETHERIME_CONFIG`: daemon config file path
- `AETHERIME_SOCKET`: Unix socket path (default `/tmp/aetherime.sock`)
- `AETHERIME_LIBIME_DICT`: override LibIME dictionary path (`sc.dict`)
- `AETHERIME_LIBIME_LM`: override LibIME language model path (`zh_CN.lm`)
