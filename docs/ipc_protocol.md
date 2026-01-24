# IPC 协议（Phase 1：TCP + JSONL）

> 传输：`127.0.0.1:48080`（可用环境变量覆盖，见 `core_engine/README` / `tsf_shell` 配置）
>
> 编码：UTF-8
>
> 分帧：**每行一个 JSON**（JSON Lines / JSONL），以 `\n` 结束

## 消息类型

### 1) Suggest 请求（tsf_shell -> core_engine）

```json
{
  "type": "suggest",
  "request_id": "42",
  "context": "Hello wor",
  "cursor": 9,
  "language_hint": "auto",
  "max_len": 32,
  "client": {
    "app": "notepad.exe",
    "pid": 1234
  }
}
```

语义：

- `request_id`：字符串（推荐单调递增或 UUID）。用于 **去抖、取消、晚到丢弃**。
- `context`：窗口文本（通常是光标前后拼出来的片段，Phase 1 可先只发光标前）。
- `cursor`：光标在 `context` 内的 index（0..len）。
- `language_hint`：`"en" | "zh" | "mixed" | "auto"`（Phase 1 先按 `"auto"`/`"en"` 处理）。
- `max_len`：建议输出最大长度（含可能的前导空格）。
- `client`：可选，便于做按应用策略/统计。

### 2) Cancel 请求（tsf_shell -> core_engine）

```json
{
  "type": "cancel",
  "request_id": "42"
}
```

语义：尽力取消；即使引擎来不及取消，tsf_shell 也会根据 `request_id` 丢弃晚到结果。

### 3) Suggest 响应（core_engine -> tsf_shell）

```json
{
  "type": "suggestion",
  "request_id": "42",
  "suggestion": "ld",
  "confidence": 0.87,
  "replace_range": [9, 9]
}
```

语义：

- `suggestion`：要显示的 ghost 文本（可包含前导空格，例如中英混输）。
- `confidence`：0..1，用于 TSF 端门控（例如 `<0.5` 不展示）。
- `replace_range`：建议替换的范围，**以 `context` 为坐标** 的 `[start, end)`：
  - Phase 1：通常是 `[cursor, cursor]`（插入）。
  - Phase 2：可用于 “替换最后一个 token 的后缀/纠错”。

## 错误与扩展

建议约定：

- 引擎不可用：tsf_shell 直接不显示 ghost（不影响输入）
- 解析失败：忽略该行；日志记一次计数
- 扩展字段：允许新增字段，双方应忽略未知字段

