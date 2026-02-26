# AetherIME

AetherIME is a Linux AI input method prototype that combines:

- Fcitx5 pinyin input (candidate list + selection flow)
- Ghost text completion (accept with `Tab`)
- Local-first prediction backends (`heuristic`, `ollama`, `llama.cpp`)
- FIM-friendly request format (`prefix` + `suffix`)

## Highlights

- Fcitx5 addon with live preedit/candidates/status
- Rust daemon over Unix socket with timeout + cache + fallback
- LibIME-based full pinyin lexicon (`sc.dict + zh_CN.lm`)
- One-command Ubuntu bootstrap
- `systemd --user` daemon auto-start script

## Quick Start (Ubuntu)

### 1) Install addon and dependencies

```bash
./scripts/bootstrap-ubuntu.sh
```

Log out and log back in, then enable `AetherIME` in `fcitx5-configtool`.

### 2) Enable local AI backend (Ollama)

```bash
./scripts/enable-ollama.sh
```

Optional model override:

```bash
./scripts/enable-ollama.sh qwen2.5:7b
```

### 3) Run daemon (foreground)

```bash
AETHERIME_CONFIG=./config/aetherime.toml cargo run -p aetherime-daemon
```

> Ghost text requires the daemon. Without daemon, pinyin candidates still work.

## Run as `systemd --user` service

Install and auto-start:

```bash
./scripts/install-user-daemon-service.sh
```

With custom config path:

```bash
./scripts/install-user-daemon-service.sh ./config/aetherime.toml
```

Useful commands:

```bash
systemctl --user status aetherime-daemon.service
systemctl --user restart aetherime-daemon.service
journalctl --user -u aetherime-daemon.service -f
```

Uninstall service:

```bash
./scripts/uninstall-user-daemon-service.sh
```

## Keybindings

- `Ctrl+Space`: switch CN/EN mode
- `Ctrl+;`: toggle AI on/off
- `1..0`: select candidate
- `Space`: commit top candidate (or raw preedit)
- `Tab`: accept ghost text
- `Esc`: clear current composing state

## Configuration

Main file: `config/aetherime.toml`

- `server.request_timeout_ms`: daemon timeout (default `5000` for CPU-friendly inference)
- `model.backend`: `heuristic` / `ollama` / `llamacpp`
- `model.mode`: `next` / `fim`
- `model.ollama_host`: default `http://127.0.0.1:11434`
- `model.ollama_model`: e.g. `qwen2.5:3b`

Environment variables:

- `AETHERIME_CONFIG`: daemon config path
- `AETHERIME_SOCKET`: addon socket path (default `/tmp/aetherime.sock`)
- `AETHERIME_LIBIME_DICT`: override `sc.dict` path
- `AETHERIME_LIBIME_LM`: override `zh_CN.lm` path

## Smoke Test

Ping:

```bash
printf '%s\n' '{"id":"ping1","type":"ping"}' | nc -N -U /tmp/aetherime.sock
```

Predict:

```bash
printf '%s\n' '{"id":"p1","type":"predict","prefix":"hello","suffix":"","language":"en","mode":"next","max_tokens":8,"latency_budget_ms":5000}' | nc -N -U /tmp/aetherime.sock
```

## Troubleshooting

### `AetherIME not available` in Fcitx5 config

Re-run bootstrap to reinstall plugin files under `/usr`:

```bash
./scripts/bootstrap-ubuntu.sh
```

### No pinyin candidates for common syllables (`chi`, `shi`, ...)

LibIME is not enabled (`PY:fallback`). Install missing packages, then rebuild:

```bash
sudo apt-get install -y libboost-dev libimecore-dev libimepinyin-dev libime-data libime-data-language-model
./scripts/bootstrap-ubuntu.sh
```

### Pinyin works but no ghost text

- Ensure daemon is running: `systemctl --user status aetherime-daemon.service`
- Ensure AI is on (`AI:on`, toggle with `Ctrl+;`)
- Check logs: `journalctl --user -u aetherime-daemon.service -f`

### `nvidia-smi: Driver/library version mismatch`

Reboot first. If still broken:

```bash
sudo apt-get install --reinstall -y nvidia-driver-580-open nvidia-utils-580 libnvidia-compute-580 linux-modules-nvidia-580-open-generic-hwe-24.04
sudo reboot
```

## Project Layout

- `daemon/`: Rust prediction service
- `fcitx5/`: Fcitx5 addon
- `config/`: default configs
- `scripts/`: install and ops scripts
- `docs/`: architecture, protocol, development notes

## Documentation

- `docs/DEVELOPMENT.md`
- `docs/ARCHITECTURE.md`
- `docs/IPC.md`

## Acknowledgements

AetherIME builds on excellent open-source projects:

- [Fcitx5](https://github.com/fcitx/fcitx5): Linux input method framework and addon SDK
- [LibIME](https://github.com/fcitx/libime): Pinyin engine and language model support
- [Ollama](https://github.com/ollama/ollama): local model serving backend
- [llama.cpp](https://github.com/ggml-org/llama.cpp): local inference backend integration target
- [Qwen](https://github.com/QwenLM/Qwen): model family used in local completion examples

Thanks to the maintainers and contributors of these projects.
Please check each upstream repository for license details and attribution requirements.

## License

Apache-2.0 (`LICENSE`)
