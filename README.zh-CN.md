# LILYGO UI Gallery

[English](README.md) | **简体中文**

LILYGO UI Gallery 是面向 LILYGO UI 的独立图库应用，基于 C++17、
[LILYGO UI AppKit](https://github.com/LILYGO-UI/cm0-appkit) 和 LVGL 9.5 构建。
应用采用参考 iOS 相册的界面，浏览相机照片、截图和随包附带的示例图片，
并将文件系统状态、展示状态和控件分别放在 Model、ViewModel 和 View 层。

项目提供 CM0 ARM64 Linux 设备构建，以及用于开发和测试的 SDL2 桌面模拟器。

## 预览

<p>
  <img src="assets/previews/preview1.png" alt="使用方形照片网格的图库页面" width="240">
  <img src="assets/previews/preview-viewer.png" alt="带缩略图胶卷的全屏照片查看器" width="240">
</p>

更多横竖屏界面见 [`assets/previews`](assets/previews)。

## 功能

- **本地图库：** 启动或刷新时扫描相机照片、`$HOME/Pictures` 和截图，显示最近
  修改的 24 张 JPEG、PNG 或 PPM 图片；有相机缩略图时优先使用。Debian 安装时
  会把内置示例导入 `$HOME/Pictures`，因此这些图片可以像用户照片一样删除。
- **相册：** 底部标签切换 Library 和 Albums。All Photos、Camera、Screenshots
  和 Samples 集合显示封面与数量，查看器中的照片切换保留当前集合范围。
- **响应式界面：** 白色页面、大标题、照片数量和蓝色导航。方形缩略图网格使用
  2 像素间距贴边排列，在横竖屏下按可用宽度重排为 3、4 或 6 列。
- **全屏查看：** 完整显示照片，不裁切内容，同时显示修改日期和文件名。
  可通过缩略图胶卷、前后按钮或横向滑动切换照片；使用加号和减号按 25%
  步进缩放，放大后拖动照片查看其他区域，向下滑动返回网格。
- **照片详情：** 向上滑动或点击详情按钮打开可滚动的详情面板，查看文件名、
  修改时间、大小、来源和位置；关闭面板后恢复查看器控件。
- **删除确认：** 通过底部操作面板确认永久删除，同时处理独立的相机缩略图。
  示例图片只读。

主机图库为空时会自动加载 `assets/gallery/` 中的只读示例。
目录路径、环境变量覆盖、格式支持和示例回退规则见
[图片与存储](docs/02-images-and-storage.md)（英文）。

## 开发环境搭建

需要 CMake 3.21 或更高版本、用于运行 LPM 的 Python 3.11 或更高版本，
以及支持 C++17 的 C/C++ 编译器。主机模拟器依赖 SDL2 和 FreeType。
在已有仓库中初始化锁定版本的 AppKit 依赖：

```sh
git submodule update --init --recursive
```

源码获取、Linux 和 macOS 依赖、LPM 安装及设备交叉编译器的配置见
[开发指南](docs/03-development.md)（英文）。项目标识和软件包元数据由
[`lpm.toml`](lpm.toml) 提供；[`CMakePresets.json`](CMakePresets.json) 定义构建配置。
所有项目命令均在仓库根目录执行。

## 构建与测试

### 桌面模拟器

```sh
lpm start --foreground
```

此命令在 `build/host-simulator` 中配置并构建 Debug 模拟器，然后以前台模式启动。
运行需要图形桌面会话。执行 `lpm start --no-build --foreground` 可复用已有构建产物。

### 测试

```sh
lpm test
```

此命令配置并构建主机模拟器，然后运行 Model、ViewModel、图片解码及横竖屏 UI 测试。
渲染快照输出到 `build/host-simulator/`。覆盖范围和示例渲染命令见
[测试与渲染](docs/03-development.md#tests-and-rendering)（英文）。修改 UI 后，
应按 [UI 开发规范](docs/01-user-interface.md) 的要求检查快照。

### CM0 设备构建

```sh
lpm pack
```

此命令为 AArch64 架构的 CM0 Raspberry Pi OS 交叉编译 Release 构建，
并将 ARM64 Debian 软件包输出到 `dist/`。首次构建会下载锁定版本的 CM0 BSP，
开发机需预先安装交叉编译器。SDK 缓存、安装内容和运行时依赖见
[设备构建指南](docs/03-development.md#cm0-device-build)（英文）。

直接使用 CMake 的命令和常见问题见[开发指南](docs/03-development.md)（英文）。

## 文档

| 指南 | 内容 |
| --- | --- |
| [开发指南](docs/03-development.md)（英文） | 依赖、LPM、测试、快照、交叉编译、打包和常见问题 |
| [图片与存储](docs/02-images-and-storage.md)（英文） | 来源目录、环境变量、示例回退、格式和删除行为 |
| [架构概览](docs/00-overview.md) | 模块边界、数据流、生命周期和响应式布局 |
| [UI 开发规范](docs/01-user-interface.md) | 布局、字体、所有权和 UI 验证要求 |

## 致谢

界面使用 [LILYGO UI AppKit](https://github.com/LILYGO-UI/cm0-appkit)
和 [LVGL](https://lvgl.io/)。AppKit 提供共享运行时字体及其许可声明。
[示例图片](assets/gallery/README.md) 为开发和测试提供可重复的图库演示素材。
