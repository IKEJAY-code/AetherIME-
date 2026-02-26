# IPC Contract (v1)

Transport: Unix Domain Socket, newline-delimited JSON.

Default socket path: `/tmp/aetherime.sock`.

## Request types

### `ping`

```json
{"id":"health-1","type":"ping"}
```

### `predict`

```json
{
  "id": "req-1",
  "type": "predict",
  "prefix": "我们",
  "suffix": "",
  "language": "zh",
  "mode": "next",
  "max_tokens": 12,
  "latency_budget_ms": 90
}
```

Fields:

- `prefix` (required)
- `suffix` (optional, for FIM)
- `language`: `zh` | `en`
- `mode`: `next` | `fim`

## Response types

### `pong`

```json
{"id":"health-1","type":"pong"}
```

### `predict`

```json
{
  "id": "req-1",
  "type": "predict",
  "ghost_text": "可以先",
  "candidates": ["可以先", "继续", "补充一下"],
  "confidence": 0.42,
  "source": "local_next",
  "elapsed_ms": 3
}
```

### `error`

```json
{
  "id": "req-1",
  "type": "error",
  "code": "timeout",
  "message": "prediction exceeded 120ms"
}
```
