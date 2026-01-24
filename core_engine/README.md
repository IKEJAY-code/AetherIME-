# core_engine

Rust 进程，负责补全和业务规则。默认在 `127.0.0.1:48080` 监听 JSONL/TCP。

## 运行

- Debug：`cargo run`
- Release（推荐给 TSF 配合使用）：`cargo run --release`

无法安装 Rust 时，可用 `cargo build`/`cargo check` 验证编译，二进制位于 `target/debug/core_engine` 或 `target/release/core_engine`。

## 配置

- `SHURUFA_ENGINE_PORT`：覆盖监听端口（默认 `48080`）。

## 手工测试（无 TSF）

1. 在终端运行引擎：`cargo run`
2. 另开一个终端，用 `nc` 发送一行 JSON：
   ```
   echo '{"type":"suggest","request_id":"1","context":"hello wor","cursor":9,"language_hint":"auto","max_len":16}' | nc 127.0.0.1 48080
   ```
3. 预期返回：
   ```
   {"type":"suggestion","request_id":"1","suggestion":"ld","confidence":...,"replace_range":[9,9]}
   ```
