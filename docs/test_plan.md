# 测试计划（手工为主）

## 手工测试矩阵（Phase 1 MVP）

应用：

- Notepad（必测）
- Edge / Chrome（输入框、textarea、contenteditable）
- VS Code（编辑器、终端）
- Word（如可，TSF 行为差异较多）

场景：

- 英文连续输入：ghost 出现、更新、Tab 接受、Esc 清除
- 快速打字：ghost 不闪烁/不卡顿；输入不卡（主线程无阻塞）
- Tab 原始行为：无 ghost 时 Tab 不被吃（缩进/切焦点仍正常）
- 失焦：切窗口/切控件后 ghost 自动清理
- 光标移动/选择：方向键、鼠标点击、选择文本后 ghost 清理
- 引擎慢/不可用：断开 `core_engine` 后不崩溃、不阻塞、不乱插字

## 边界与安全

- 密码框（InputScope=Password）：不显示 ghost；不发送上下文
- 敏感输入框（可扩展：PIN/OTP/Payment）：同上
- 超时：TSF 端设置超时/晚到丢弃（Phase 2）

## 观察点（Debug）

- `%TEMP%\\shurufa_tsf.log` 是否有连接/请求/响应日志（不应包含原文内容）
- `core_engine` stderr 是否有连接与请求日志（Phase 1 可打印，但后续应改成统计/采样）

