# AetherIME Architecture (MVP)

## Runtime Components

### 1) Fcitx5 Addon (`fcitx5/`)

- `InputMethodEngineV2` implementation (`AetherImeEngine`)
- Per-context session state (`AetherImeState`)
- Handles key events, preedit, candidates, commit flow
- Calls daemon through `GhostSession` + Unix socket

### 2) AI Daemon (`daemon/`)

- Unix socket server (default `/tmp/aetherime.sock`)
- Request types: `ping`, `predict`
- Timeout guard, response cache, backend fallback

### 3) Prediction Backends

- `heuristic`: rule-based fallback backend
- `ollama`: local HTTP backend (`/api/chat`)
- `llamacpp`: local GGUF backend via `llama-cli`

### 4) Pinyin Lexicon Backend

- Primary: `libime` (`sc.dict` + `zh_CN.lm`)
- Fallback: small built-in lexicon

## Data Flow

1. User types in app input field.
2. Fcitx addon updates preedit/candidates.
3. Addon sends `predict` JSON to daemon.
4. Daemon routes request to selected backend.
5. Addon renders ghost text and accepts with `Tab`.

## Key Event Pipeline (Addon Side)

1. Enter `AetherImeEngine::keyEvent`.
2. Resolve `AetherImeState` for current input context.
3. Handle mode hotkeys.
4. Handle candidate navigation and selection.
5. Handle edit keys (`Backspace`, `Esc`, `Space`, `Enter`).
6. Refresh preedit + ghost text + candidate list.

## IPC Schema Example

Request:

```json
{"id":"r1","type":"predict","prefix":"hello","suffix":"","language":"en","mode":"next","max_tokens":8,"latency_budget_ms":5000}
```

Response:

```json
{"id":"r1","type":"predict","ghost_text":" world","candidates":[" world"],"confidence":0.71,"source":"local_next","elapsed_ms":120}
```

## FIM Strategy

- Supports `mode = "fim"`
- Uses surrounding text when available (`prefix` + `suffix`)
- Falls back to `next` when `suffix` is empty

## Failure Strategy

- Invalid JSON => `type=error`, `code=invalid_request`
- Timeout => `type=error`, `code=timeout`
- Primary backend failure => fallback to `heuristic`
