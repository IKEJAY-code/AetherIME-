# tsf_shell

Windows TSF Text Service，负责上下文读取、显示灰色 ghost、接受/清除等。

## 依赖

- Windows 10/11
- Visual Studio 2022（含 C++ 桌面开发工具集）
- CMake 3.20+
- Windows SDK (自带 VS2022 即可)

## 构建

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug   # 或 Release
```

输出 DLL 位于 `build/bin/<Config>/shurufa_tsf_shell.dll`，注册脚本会自动使用该路径。

## 注册 / 卸载

需在 **管理员 PowerShell** 中运行：

```powershell
./scripts/register_tip.ps1   -Configuration Debug   -Platform x64
./scripts/unregister_tip.ps1 -Configuration Debug   -Platform x64
```

## 连接 core_engine

- 默认连接 `127.0.0.1:48080`
- 可用环境变量覆盖：
  - `SHURUFA_ENGINE_HOST`（默认 `127.0.0.1`）
  - `SHURUFA_ENGINE_PORT`（默认 `48080`）

TIP 会在 `%TEMP%\\shurufa_tsf.log` 写入少量调试日志（不包含原文输入）。
