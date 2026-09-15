# LILYGO UI Gallery 总览

本项目是面向 LILYGO UI 的独立图库应用，界面参考 iOS 相册，采用白色背景、大标题、
蓝色导航和贴边照片网格，并按当前 AppKit 工程约束组织为可测试的 C++17
Model、ViewModel 和 LVGL View。

## 数据与行为

- 默认读取 `/root/photos`、`/root/screenshots` 和安装包中的
  `/usr/share/lilygo-ui-gallery/gallery` 示例目录。
- 支持 JPEG、PNG、PPM，以及相机生成的 `360x640` RGB565 缩略图。
- 合并来源目录后按修改时间倒序排列，最多展示 24 项。
- 底部 Library / Albums 标签切换图库与相册；页面顶部显示大标题和照片数量。
- Albums 显示 All Photos、Camera、Screenshots 和 Samples 的封面与数量，点击后
  按真实来源筛选网格；查看器切换照片时保留当前筛选范围。
- 网格点击后进入浅色全屏查看器，以完整适配方式显示照片，上方显示文件修改日期
  和文件名；可点选底部缩略图胶卷、使用前后按钮或横向滑动切换照片。
- 删除前显示底部操作面板，明确提示永久删除；删除相机照片时同步删除独立的
  `.thumb.rgb565` 文件。示例照片只读，删除按钮不可用。
- 刷新按钮重新扫描目录，新拍摄的照片无需重启应用即可出现。

主机模拟器可用 `LILYGO_GALLERY_PHOTO_DIR`、`LILYGO_GALLERY_SCREENSHOT_DIR` 和
`LILYGO_GALLERY_SAMPLE_DIR` 覆盖默认目录。设备构建始终使用默认目录，除非启动
进程显式提供同名环境变量。

## 架构

```text
src/pages/gallery/gallery_model.*       目录扫描、排序、选择和删除
src/pages/gallery/gallery_view_model.*  展示模式、命令和 LVGL Subject
src/pages/gallery/gallery_image.*       PNG/JPEG/PPM/RGB565 解码与缩放
src/pages/gallery/gallery_view.*        网格、查看器、手势和删除确认层
src/app.*                               AppKit 生命周期与页面装配
```

Model 不依赖 LVGL。View 只通过 ViewModel 命令修改业务状态，图片缓冲由 View
持有且生命周期覆盖对应 Canvas。页面关闭时先删除 LVGL 对象，再释放图片缓冲和
ViewModel。

## 响应式布局

标准视口为竖屏 `568x1232` 和横屏 `1232x568`。照片网格使用贴边方形缩略图与
`2px` 间距，依据容器宽度切换 3、4 或 6 列；相册封面按可用宽度排列为 2 或 4 列。
查看器使用 contain-fit，避免裁切照片。底部操作栏使用固定高度和稳定的图标按钮
尺寸；视口高度不足 `400px` 时隐藏胶卷，为照片保留空间。方向变化不会改变当前
筛选、选择或查看模式。

## 验收

```sh
cmake --preset host-simulator
cmake --build --preset host-simulator --parallel
ctest --preset host-simulator
```

渲染测试在 `build/host-simulator/` 生成网格、相册、查看器和删除确认层的横竖屏快照。
交叉构建和 Debian 打包使用 `cm0-cross` preset。
