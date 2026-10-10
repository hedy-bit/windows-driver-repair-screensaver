# Windows 驱动修复循环屏保

项目结构和维护约定见 [项目总览](PROJECT_OVERVIEW.md) 与 [AGENTS.md](AGENTS.md)。

C++17 / Win32 原生屏保，无第三方依赖。只模拟画面，不产生真实蓝屏、不安装驱动、不修改系统、不实际重启。

每轮约 2 分 46 秒：驱动错误蓝屏（DRIVER_IRQL_NOT_LESS_OR_EQUAL / nvlddmkm.sys）→ 准备自动修复 → 诊断 → 修复进度 → 修复完成 → 模拟重启 → 再次蓝屏，无限循环。

蓝屏收集持续 72 秒，修复进度持续 70 秒。百分比会停留数秒再跳动，并在 99% 停顿；加载圆点也会间歇卡顿。停顿仅影响动画，Alt+F4 仍可立即退出。

## 编译

安装 Visual Studio 的“使用 C++ 的桌面开发”组件和 CMake 后，在本目录执行：

```powershell
cmake -S . -B build
cmake --build build --config Release
```

产物：`build/Release/DriverRepairLoop.scr`（单配置生成器可能放在 `build/`）。

## 使用

- 双击 `.scr` 或执行 `DriverRepairLoop.scr /s`：全屏运行。
- 仅 Alt+F4 退出；忽略普通按键、鼠标点击和移动，失去焦点时继续运行。Windows 保留的系统快捷键（如 Ctrl+Alt+Delete、Win、Alt+Tab）不拦截。
- 右键 `.scr` → “安装”：打开 Windows 屏幕保护程序设置，可按需启用。
- `/c`：显示说明；`/p 窗口句柄`：支持系统屏保预览。
- 多显示器时覆盖整个虚拟桌面，内容按比例居中。

全屏画面不显示模拟标注、退出提示或循环计数。Alt+F4 仍可退出。画面中的驱动名称仅用于展示，不代表电脑实际故障；修复与重启均为动画，程序不会执行实际系统操作。
