# Shurufa — Cursor 风格 Ghost Text 自动补全输入法（Windows / TSF）

本仓库是一个 **monorepo**，目标是在 Windows 上实现类似 Cursor 的 **inline ghost text** 自动补全体验：

- `tsf_shell/`：Windows TSF Text Service（C++/MSVC, DLL）。负责键盘事件、上下文获取、ghost 显示、Tab 接受、commit 文本、安全策略、性能隔离。
- `core_engine/`：独立进程（Rust）。负责补全算法（baseline 先做快且准）、中英混输规则、缓存/门控；后续可扩展小模型推理。
- 两者通过本机 IPC（Phase 1 先用 `localhost TCP`，Phase 2+ 可切 Named Pipe）通信，支持取消与去抖。

## 快速开始（开发模式）

> 需要 Windows 10/11、Visual Studio 2022（含 C++ 工具链）、CMake、Rust toolchain。
>
> TSF 注册通常需要管理员权限：请用“管理员 PowerShell”运行。

1) 启动引擎：

```powershell
./scripts/start_engine.ps1
```

2) 构建并注册 TIP：

```powershell
./scripts/register_tip.ps1 -Configuration Debug -Platform x64
```

3) 在 Windows 输入法列表里选择 `Shurufa Ghost Text (en-US)`（或在语言栏切换）。

4) 在 Notepad 输入英文，等待灰色 ghost；按 `Tab` 接受，按 `Esc` 取消。

卸载：

```powershell
./scripts/unregister_tip.ps1 -Configuration Debug -Platform x64
```

## 环境变量

- `SHURUFA_ENGINE_PORT`：覆盖引擎监听/TSF 连接端口（默认 `48080`）
- `SHURUFA_ENGINE_HOST`：TSF 连接的引擎主机（默认 `127.0.0.1`；仅 TSF 端使用）

## 文档

- `docs/architecture.md`：模块职责、状态机、性能与安全策略。
- `docs/ipc_protocol.md`：IPC 协议（JSONL）定义与语义。
- `docs/tsf_shell.md`：TSF Shell 关键接口/类结构与实现要点。
