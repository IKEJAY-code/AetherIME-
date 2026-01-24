# TSF Shell 关键实现点（C++ / COM / TSF）

> 目标：Phase 1 做一个可安装 TIP：能读取上下文、请求引擎、显示灰色 ghost、Tab 接受、Esc 清除。

## 关键 COM/TSF 接口

- `ITfTextInputProcessor`
  - `Activate(ITfThreadMgr*, TfClientId)`：初始化、注册 sinks、创建消息窗口、启动 IPC
  - `Deactivate()`：释放资源、停止 IPC、清除 ghost
- `ITfKeyEventSink`
  - `OnTestKeyDown/Up`：仅在 ghost 可接受时吃 `Tab`；其它键默认放行
  - `OnKeyDown/Up`：`Tab` -> accept；`Esc` -> clear；方向键/退格 -> clear
- `ITfThreadMgrEventSink`
  - `OnSetFocus(docMgrFocus, docMgrPrev)`：焦点切换时切换 text edit sink；并清理 ghost/ring buffer
- `ITfTextEditSink`
  - `OnEndEdit(context, ecReadOnly, editRecord)`：读 surrounding text，触发去抖请求（忽略由自身 ghost 修改造成的回调）
- `ITfCompositionSink`
  - `OnCompositionTerminated(ecWrite, composition)`：跟踪 ghost composition 生命周期，避免悬空指针
- `ITfDisplayAttributeProvider`
  - `EnumDisplayAttributeInfo / GetDisplayAttributeInfo`：提供灰色样式的 display attribute
- `ITfDisplayAttributeInfo`（由 provider 返回）
  - `GetAttributeInfo`：设置 `TF_DISPLAYATTRIBUTE`（灰字，无下划线/背景）

## 线程模型与消息回传

- TSF 对象运行在 **STA**（DLL 注册 ThreadingModel=Apartment）。
- **TSF 线程绝不做阻塞 IPC**：
  - IPC client 使用后台线程（socket 读写）
  - 后台线程收到响应后，通过消息窗口 `PostMessage` 回到 TSF 线程，再在 edit session 内更新 UI

## Composition 管理（ghost）

核心思路：

1) 获取 insertion point (`GetSelection` + `Collapse(TF_ANCHOR_START)`)
2) `ITfContextComposition::StartComposition` 创建 ghost composition
3) 在 composition range `SetText` 插入 suggestion
4) 使用 `GUID_PROP_ATTRIBUTE` 属性，把 range 标为自定义 display attribute（灰色）
5) 让 selection/caret 保持在 ghost 前（用户继续输入会覆盖/推动 ghost，再由状态机更新）

清理：

- `ClearGhost`：删除 range 文本 + `EndComposition`
- `AcceptGhost`：`EndComposition` 但不删除，清除 display attribute，并把 caret 移到末尾

## 失焦清理

在 `OnSetFocus` / `Deactivate`：

- `ClearGhost()`
- 清空 ring buffer
- 取消 in-flight request（发 cancel + 本地 request_id gating）

## 错误处理

- 所有 TSF/COM 调用检查 `HRESULT`；失败不崩溃，不影响用户继续输入：
  - IPC 不可用：不显示 ghost
  - edit session 失败：直接放弃本次更新
  - display attribute 注册失败：降级为不显示 ghost（或显示但无样式）

