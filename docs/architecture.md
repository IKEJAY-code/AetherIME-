# 架构概览

## Monorepo 目录结构（建议）

```
/
  README.md
  docs/
    architecture.md
    ipc_protocol.md
    tsf_shell.md
  proto/
    ipc_protocol.jsonl.md
  scripts/
    dev.ps1
    start_engine.ps1
    stop_engine.ps1
    register_tip.ps1
    unregister_tip.ps1
  core_engine/                 # Rust 进程：补全与规则
    Cargo.toml
    src/
      main.rs                  # TCP JSONL server
      protocol.rs              # IPC 结构与解析
      baseline/
        mod.rs                 # BaselineSuggestor
        trie.rs                # Trie 前缀检索
        mixing.rs              # 中英混输 token/空格规则
        scoring.rs             # 打分/门控/长度控制
      personalization.rs       # 频次与个性化（Phase 2+ 可接 SQLite）
      util.rs
  tsf_shell/                   # TSF TIP DLL：上下文/ghost/键处理/安全
    CMakeLists.txt
    src/
      DllMain.cpp
      ClassFactory.*           # COM class factory
      Guids.cpp
      TextService.*            # ITfTextInputProcessor + sinks + ghost
      DisplayAttribute.*       # ITfDisplayAttributeInfo(+枚举器)
      IpcClient.*              # IPC client（后台线程，永不阻塞 TSF 线程）
      Registry.*               # DllRegisterServer/DllUnregisterServer helpers
      Log.*                    # Debug log
    include/
      shurufa/Guids.h
```

## 模块职责

### tsf_shell（C++ / TSF）

- **输入事件**：`ITfKeyEventSink` 拦截/放行 Tab/Esc；其它按键尽量不吃（让应用保持原生行为）。
- **上下文**：优先从 `ITfContext` surrounding text 读取；失败时使用 ring buffer 兜底（Phase 2 进一步增强）。
- **Ghost 展示**：用 TSF composition + display attribute（灰色、无下划线）模拟 Cursor 风格 ghost。
- **接受与提交**：`Tab` 将 ghost composition 终止为已提交文本；`Esc` 清除。
- **性能与隔离**：TSF 主线程只做轻量操作；IPC、解析、缓存等放后台线程；超时与晚到结果丢弃。
- **安全策略**：密码/敏感输入 scope 禁用；默认不落盘原始文本；日志只记统计。

### core_engine（Rust 独立进程）

- **补全算法 baseline**（Phase 1 起跑）：
  - 英文 last-token 前缀补全（Trie/词表）
  - 简单打分（频次/上下文匹配）与门控（置信度、长度、黑名单）
  - 中英混输规则：ASCII token 连续判定；必要时自动插入空格
- **缓存**：按 `(context_hash, cursor, max_len, hint)` 做 LRU；避免重复计算（Phase 2）。
- **取消**：支持 cancel；同时允许客户端丢弃晚到结果（request_id gating）。
- **可扩展**：后续替换 baseline 为小模型/本地推理，协议不变。

## 性能与可靠性要点

- TSF 线程：**绝不等待 IPC**；只做：
  - 事件判定、状态机更新
  - 读少量上下文（<= 256 chars）与显示/清除 ghost
- core_engine：baseline 目标 **P95 < 50ms**；超时响应直接丢弃。
- 结果一致性：
  - `request_id` 单调递增；TSF 只接受最新 request 的结果
  - 如果 caret/selection/上下文版本变化，晚到结果丢弃

