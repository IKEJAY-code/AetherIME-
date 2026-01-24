# ipc_protocol.jsonl.md

本文件用于给工程师/工具提供 **IPC JSONL** 的稳定定义（与 `docs/ipc_protocol.md` 内容一致）。

## suggest

字段：

- `type`: `"suggest"`
- `request_id`: string
- `context`: string (UTF-8)
- `cursor`: number (int)
- `language_hint`: string
- `max_len`: number (int)
- `client`: object (optional)
  - `app`: string
  - `pid`: number (int)

## cancel

- `type`: `"cancel"`
- `request_id`: string

## suggestion

- `type`: `"suggestion"`
- `request_id`: string
- `suggestion`: string
- `confidence`: number (float 0..1)
- `replace_range`: `[number, number]` (int, [start,end) in context)

