# AGENTS.md — GuoDesk 开发须知

本文件是跨机器协作的项目记忆，供任何接手本仓库的 AI agent 或开发者快速上手。最后更新：2026-09-27。

## 项目是什么

GuoDesk：Windows 桌面分区整理工具（C++20 + C++/WinRT + WinUI 3，Windows App SDK 1.8 自包含，零 .NET 依赖，MIT）。
分区窗口收纳文件/文件夹/快捷方式**入口**——只存引用，不动原文件，缺失标 ⚠ 可重新定位。

对标：Stardock Fences（付费）、DeskBox（开源头部，5400+ 星，C# WinUI3 + Rust）。GuoDesk 定位差异：更轻、更稳、纯 C++ 单体；不拼功能广度，拼"引用式入口语义 + 配置可恢复"的基础体验。

## 常用命令

```powershell
./scripts/build.ps1                 # 构建 → artifacts\Release\（自包含目录）
./artifacts/Release/GuoDesk.exe --self-test report.log   # 核心自测（当前 13 项）
# 安装包（Inno Setup 6 在 %LOCALAPPDATA%\Programs\Inno Setup 6）
& "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" scripts/guodesk.iss
./scripts/package-portable.ps1      # 免安装 zip（用系统 bsdtar 写标准路径）
```

发布流程：版本号同步改 `scripts/guodesk.iss`、`scripts/package-portable.ps1`、`src/GuoDesk/app.rc` → 重打两包 → 静默安装测试 → 提交推送 → GitHub API 建 Release 传资产（token 从 `git credential fill` 取，勿打印）。

## 已知坑（都踩过，勿重复）

1. **XAML 编译器偶发不生成 `App.xaml.g.hpp`**（Generated Files 目录留下 .backup）。恢复备份即可编译：`cp "src/GuoDesk/Generated Files/App.xaml.g.hpp.backup" "src/GuoDesk/Generated Files/App.xaml.g.hpp"`。每次 build 前先检查该文件是否存在。
2. **本机 git 无身份配置**：提交用 `git -c user.name="cloudlight" -c user.email="19404678+cloudlight369@users.noreply.github.com"`（与 Initial commit 作者一致），勿改全局配置。
3. **Inno Setup 6.7 卸载**：UnInstDaemon 在应用实例还在跑时会卡住留下 DLL——先杀 GuoDesk 进程再卸载。
4. **PowerShell 逗号优先级高于乘法**：`New-Object X(2 * $a, 2 * $a)` 会把 `$a` 解析成数组，先算好变量再传。
5. **Git Bash 调 tar 会命中 GNU tar**（报 "Cannot connect to D:"）；PowerShell 里也要用全路径 `$env:SystemRoot\System32\tar.exe`。
6. **DPI 坐标系**：截屏/枚举窗口前先 `SetProcessDPIAware()`，否则 GetWindowRect 返回虚拟化坐标和 CopyFromScreen 不一致。
7. rc 文件里字符串只用 ASCII（FileDescription "GuoDesk Desktop Zones"），避免 rc.exe 编码问题。

## 代码地图

```
src/GuoDesk/
  Core.*        数据模型（Zone/Entry/Settings/Layout）、JSON 序列化、原子存储（temp→ReplaceFile→backup）、路径去重
  Shell.*       Win32/COM：文件对话框、打开/定位、桌面宿主嵌入、缩略图加载
  DeskWindow.*  分区窗口 UI + 托盘控制器（Controller 也在这个文件）
  App.xaml.cpp  wWinMain 入口（--self-test / --data-dir / --desktop 参数）
  app.rc/app.ico  图标资源（IDI_GUODESK=101）+ VERSIONINFO
tests/CoreTests.cpp   --self-test 用例
scripts/              build.ps1 / guodesk.iss / package-portable.ps1 / make-icon.ps1 / make-ico.py
```

配置存于 `%LocalAppData%\GuoDesk\layout.json`（原子写 + 损坏自动恢复）。

## 当前状态与续作清单（v0.2.0 应用感升级，2026-09-27 暂停于半途）

已完成：v0.1.0 全链路发布（源码/安装包/免安装包 + Release）；图标链路（AI 图 → make-icon.ps1 → app.ico → app.rc → exe 已嵌入并验证，提交 2edffeb）。

**接下来按顺序做：**

1. **托盘图标**：DeskWindow.cpp `Controller::AddTray` 里 `LoadIconW(nullptr,IDI_APPLICATION)` 改为 `LoadIconW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(101))`，失败再回退。
2. **安装包图标/版本**：guodesk.iss 加 `SetupIconFile=..\src\GuoDesk\app.ico`，`#define AppVersion "0.2.0"`；package-portable.ps1 默认参数同步。
3. **build.ps1** 加一行：拷贝 `src/GuoDesk/app.ico` → 输出目录 `guodesk.ico`（给设置窗口 AppWindow.SetIcon 用）。
4. **Core.cpp**：Serialize/Deserialize 持久化 `settings`（`{"theme":"","compact":false}`，读侧用 HasKey 容错）。Core.h 的 Settings 结构已加好。
5. **DeskWindow.cpp 视觉重写**：
   - 字符按钮换 FontIcon（Segoe Fluent Icons）：Add=E710、ChevronUp=E70E、More=E712、拖动手柄 Gripps=E7C2（**该码点未验证，截图确认**，不像就换）
   - 根布局去掉硬编码色和 RequestedTheme(Dark)：`window.SystemBackdrop(MicaBackdrop())`，磁贴用主题资源刷（CardBackgroundFillColorDefault/Secondary、TextFillColorSecondary，Lookup 失败回退写死色）
   - 桌面嵌入（WS_CHILD）时 Mica 无效 → SetDesktop(true) 里给 root 设 `ApplicationPageBackgroundThemeBrush` 不透明回退
   - 磁贴 PointerEntered/Exited 换背景 + 图标缓存（Shell.cpp 静态 map<path,ImageSource>，LoadIcon 协程取到缩略图后写入）+ 窗口淡入（Storyboard DoubleAnimation ~220ms，pch.h 已加 Animation.h）
6. **SettingsWindow.h/cpp 新建**（代码构建 UI，别加新 XAML）：开机自启 ToggleSwitch（HKCU\...\Run 写 "GuoDesk"=引号包 exe 路径；vcxproj 需加 **advapi32.lib**）、主题 ComboBox（Default/Light/Dark → root.RequestedTheme + Save）、紧凑磁贴、关于页；Controller 加 `std::unique_ptr<SettingsWindow> settings` + ShowSettings/CloseSettings（Closed 里 reset）；托盘菜单和分区 ··· 菜单加"设置"。
7. **单实例唤起**：Controller 构造检测到已有实例 → `FindWindowW(L"GuoDesk.Controller")` + `PostMessageW(prev, WM_APP+2,0,0)` → 抛自定义 `already_running`；wWinMain 捕获后静默 return 0（别再弹错误框）；MessageProc 处理 WM_APP+2 → Show()+SetForegroundWindow。
8. **实测**：编译 → 自测 13 项 → 启动截图看 Mica/图标/磁贴 → 设置窗口各开关生效（注册表确认）→ 二次启动唤起 → 桌面嵌入不回归 → 拖拽排序实机验证（v0.1.0 起一直没实测过，v0.2.0 必须做）。
9. **发布**：静默安装/卸载测试 → 提交推送 → Release v0.2.0（API：POST /releases tag_name=v0.2.0，传 setup.exe + portable.zip，正文用文件方式避免 heredoc 中文编码坑——之前 PATCH 用 Write 工具写 JSON 文件再 --data-binary @file 才成功）。

## 用户偏好

- 中文沟通；交付物必须是**可直接双击运行的 EXE + 安装程序**，不是源码包
- UI 必须达到"产品级应用感"：真图标、Fluent 图标、Mica 材质、设置页缺一不可（v0.1.0 被批"一点不像应用"）
- 推送/发布前需确认；提交作者身份用 cloudlight/cloudlight369 noreply 邮箱
