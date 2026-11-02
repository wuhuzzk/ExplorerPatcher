# 开始菜单多显示器定位修复

本次处理的现象：边缘模式不跟随鼠标所在屏幕；居中时菜单已跨屏但自动隐藏的主屏任务栏仍然弹出；竖屏副屏上的菜单布局/展开位置不正确。

## 定位证据

- 当前系统匹配的 `twinui.pcshell.dll` 符号与反汇编表明：`XamlLauncher::OnShellHookMessage` → `OnHotkeyInvoke` → `ShowStartView(ILSM_HSHELLTASKMAN)`。这条路径不经过旧的 Progman `SC_TASKLIST` 钩子。
- 键盘显示路径调用 `ImmersiveMonitorHelper::AdjustMonitorConnectedIfNeeded()`；开始按钮路径保留显式的显示器连接。
- 原有 `Start_SetWindowRgn()` 仅在居中分支重新按 `MonitorOverride` 选择屏幕，再移动已显示的窗口。此时菜单内部布局、动画和任务栏的目标可能仍然属于另一屏。
- 本机只读线程探针确认：桌面线程和任务栏线程获取的是 `combase.dll` 中的 COM 代理；`ApplicationManager_DesktopShellWindow` 所属线程才能获取 `twinui.pcshell.dll` 中的真实 launcher 实现。

## 实现

1. 在上述 immersive shell 线程取得 launcher 的公开 COM 接口；只在实现地址属于 `twinui.pcshell.dll` 时安装 inline hook，拒绝修改 COM 代理。
2. 仅对多屏键盘打开开始菜单，在显示前按设置选择目标显示器并执行 `ConnectToMonitor()`。成功后使用开始按钮的显示路径，避免系统重新选择另一屏。原有调用标志和返回值不变；任一连接步骤失败时回退原生路径。
3. 鼠标点击开始按钮以及单显示器场景不执行此重定向。
4. 居中定位跟随 launcher 的实际连接屏幕，删除显示后再次依据鼠标位置跨屏搬移的行为。不再因为副屏任务栏没有开启按钮居中而回退主屏。
5. 保留既有布局/动画钩子按 launcher 连接屏幕查询工作区与任务栏边缘的行为。自动隐藏开关仍读取主任务栏的全局属性；没有把该属性误当成每个副屏窗口都具有的属性。
6. 旧 `SC_TASKLIST` 路径共用选屏逻辑，并在 COM 操作失败时保留原消息；修正可见性查询和关闭菜单参数的错误处理。

## 已验证与待验证

- MSVC Release x64 目标编译成功，产物为 `build/Release/x64/ExplorerPatcher.amd64.dll`。工程仍有既有编译警告，本次未宣称全工程零警告。
- `test/run-start-monitor-regression.ps1` 提取真实生产函数进行模拟依赖测试：显示器选择、枚举顺序、负坐标、竖屏工作区、主副屏交替、可选输出为空、COM 失败回退、连接/显示调用顺序、鼠标与单屏路径、原标志和返回值。
- 已有经典菜单接口与导航栏回归测试通过。
- 2026-11-02 已接入真实竖屏副屏并部署本次补丁。主屏物理尺寸为 1920×1080，副屏竖屏尺寸为 1080×2160。
- 用户按 Win 键实测后确认三项均正常：边缘模式跟随鼠标所在屏幕；居中/边缘配合自动隐藏时唤起正确任务栏；竖屏副屏菜单不再从屏幕中间错误展开。
- 安装后的 Explorer PID 为 23232，运行正常；通过本次构建 PDB 对应的内存只读核验，`g_startMonitorHookState=2`（已安装），`bMonitorOverride=0`（鼠标所在屏幕）。这不是以模拟结果替代用户实测。
- 两个安装目标的 SHA256 均为 `E098EC7B8CB0158E639108D5F98BBAF56FE1C1F78B770C7CC38609A7F8DBF030`，与测试包一致。安装前各自的 DLL 已备份，原始备份哈希核验通过。
- 其他 Windows 构建、不同 DPI/屏幕排列的全面兼容性未作保证。鼠标点击路径、指定显示器和失败回退有独立回归覆盖，未将其描述为全部完成了真人操作验收。

## 安装与验证记录

测试包在 `test/start-menu-monitor-package/`，包含当前构建、安装/回滚脚本和基线哈希。安装需使用管理员 PowerShell 7，操作会重启当前会话的 Explorer。它会备份当前安装的两个 DLL，保留此前导航栏修复；不修改 EP 设置。

本机实测结论见上节。回滚入口：`test/start-menu-monitor-package/backups/20261102-213820-260/ROLLBACK.ps1`，需在管理员 PowerShell 7 中运行；回滚前脚本会再次校验当前 DLL，防止覆盖后来安装的其他版本。

后续跨版本兼容性测试建议覆盖不同 DPI、更多任务栏边缘、指定屏幕热插拔、菜单打开后移动鼠标等；这些不是本次已经完成的真人验收项目。

相关上游记录：[#1622](https://github.com/valinet/ExplorerPatcher/issues/1622)、[#4959](https://github.com/valinet/ExplorerPatcher/issues/4959)、[#5138](https://github.com/valinet/ExplorerPatcher/pull/5138)。本实现没有直接套用 PR 中无条件按光标选择显示器的逻辑。
