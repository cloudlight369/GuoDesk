# GuoDesk

GuoDesk 是一个 Windows 桌面分区整理工具：在桌面上创建多个可自由拖动的"分区"窗口，把文件、文件夹和应用快捷方式拖进去归位。入口只是引用，**原文件始终保持在原位**。

![平台](https://img.shields.io/badge/platform-Windows%2010%2022000%2B-blue) ![协议](https://img.shields.io/badge/license-MIT-green) ![技术栈](https://img.shields.io/badge/C%2B%2B20-WinUI%203-purple)

## 功能

- **分区窗口**：可同时开多个分区，自由拖动、折叠、重命名，位置和大小自动记住
- **拖入即收**：把文件 / 文件夹 / 应用快捷方式拖进分区即可添加入口，自动去重、路径规范化
- **入口管理**：双击打开；右键可打开、定位原文件、重新定位、移除入口（只移除引用，不删原文件）
- **拖拽排序**：入口可在分区内排序，也可跨分区移动
- **托盘常驻**：新增分区、显示全部、试验桌面嵌入、退出
- **配置安全**：布局保存在 `%LocalAppData%\GuoDesk`，原子写入 + 损坏自动从备份恢复

## 安装

从 [Releases](https://github.com/cloudlight369/GuoDesk/releases) 下载：

- **安装版**：`GuoDesk-x.y.z-setup.exe`，双击安装即可。按用户安装（不需要管理员权限），自带全部运行库
- **免安装版**：`GuoDesk-x.y.z-portable.zip`，解压后直接运行 `GuoDesk\GuoDesk.exe`，适合绿色环境或 U 盘携带

## 从源码构建

依赖：

- Visual Studio 2022 Build Tools（含 "使用 C++ 的桌面开发" 与 "通用 Windows 平台开发" 工作负载）
- Windows App SDK / C++WinRT 通过 NuGet 自动还原

```powershell
git clone https://github.com/cloudlight369/GuoDesk.git
cd GuoDesk
./scripts/build.ps1              # 生成 artifacts\Release\GuoDesk.exe（自包含目录）
./scripts/build.ps1 -Configuration Debug
```

运行核心逻辑自测：

```powershell
./artifacts/Release/GuoDesk.exe --self-test report.log
```

制作安装程序（需 [Inno Setup 6](https://jrsoftware.org/isinfo.php)）：

```powershell
& "<Inno Setup 安装目录>\ISCC.exe" scripts/guodesk.iss
# 输出 artifacts\installer\GuoDesk-<版本>-setup.exe
```

制作免安装版 zip：

```powershell
./scripts/package-portable.ps1    # 输出 artifacts\installer\GuoDesk-<版本>-portable.zip
```

## 使用说明

- 启动后在托盘图标右键：**新增分区 / 显示全部 / 试验桌面嵌入 / 退出**
- 分区右上角：`＋` 添加入口，`⠿` 拖动窗口，`···` 更多菜单，`⌃` 折叠
- "试验桌面嵌入"为实验性功能，把分区窗口宿主到桌面图标层之上，失败时自动回退普通窗口

## 项目结构

```
src/GuoDesk/       C++/WinUI 3 源码
  Core.*           分区/入口数据模型、JSON 序列化、原子化配置存储
  Shell.*          文件对话框、打开/定位、窗口嵌入、图标加载
  DeskWindow.*     分区窗口 UI、拖拽、托盘控制器
tests/             核心逻辑自测（--self-test）
scripts/           构建脚本与 Inno Setup 打包脚本
```

## 协议

[MIT](LICENSE)
