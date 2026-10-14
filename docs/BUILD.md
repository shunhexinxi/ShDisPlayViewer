# 从公开 Viewer 源码构建

公开源码不含 ShEarthSDK 或 ShVideoSDK 的实现。完整 Viewer 需要对应平台的两个 SDK 开发包，使用 SHDISPLAY_EARTH_PROVIDER=PACKAGE 和 SHDISPLAY_VIDEO_PROVIDER=PACKAGE。

## Windows x86_64

准备 MSVC 2022、Qt 6.11.x、CMake 3.25+、Ninja 和对应依赖环境。将 ShEarthSDK 包解压到 sdk/ShEarthSDK/windows-x64，将 ShVideoSDK 包解压到 sdk/ShVideoSDK/windows-x64。目录和导入库见 [SDK_LAYOUT.md](SDK_LAYOUT.md)。

在 cmd 中设置自己的环境路径：

```bat
set "QT_ROOT=C:\Dev\Qt\6.11.2\msvc2022_64"
set "VCPKG_ROOT=C:\Dev\vcpkg"
scripts\Build-Windows.cmd -DSHDISPLAY_EARTH_PROVIDER=PACKAGE -DSHDISPLAY_VIDEO_PROVIDER=PACKAGE -DVIDEOSDK_FFMPEG_RUNTIME_DIR="C:\Dev\ffmpeg7\bin"
scripts\Run-Windows.cmd
```

也可显式指定 -DShEarthSDK_DIR=/path/to/earth/lib/cmake/ShEarthSDK 和 -DShVideoSDK_DIR=/path/to/video/lib/cmake/ShVideoSDK。两个 SDK 的库名和 C++ 命名空间保留原名，属于二进制兼容名称。

公开构建不编译视频实现，因此不需要 ShVideoSDK 私有源码或 FFmpeg 头文件。运行时仍须部署匹配的 FFmpeg 7 DLL、Qt 与地图依赖。构建脚本部署后运行 CTest，输出在 build/windows-release。

## Ubuntu 22.04 及以上 x86_64

准备 GCC/G++、CMake 3.25+、Ninja、Qt 6.11.x、OSG 3.6.5、osgEarth 3.8、FFmpeg 7 及对应 SDK 包。将两个包解压到 sdk/ShEarthSDK/ubuntu2204-x64 和 sdk/ShVideoSDK/ubuntu2204-x64，按自己的依赖安装位置设置环境：

```bash
export QT_ROOT=/path/to/Qt/6.11.2
export OSG_ROOT=/path/to/OpenSceneGraph-3.6.5
export OSGEARTH_ROOT=/path/to/osgEarth-3.8
export CMAKE_PREFIX_PATH="$QT_ROOT;$OSG_ROOT;$OSGEARTH_ROOT"
export LD_LIBRARY_PATH="$QT_ROOT/lib:$OSG_ROOT/lib:$OSGEARTH_ROOT/lib:/path/to/ffmpeg7/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
bash scripts/build-ubuntu.sh -DSHDISPLAY_EARTH_PROVIDER=PACKAGE -DSHDISPLAY_VIDEO_PROVIDER=PACKAGE
bash scripts/run-ubuntu.sh
```

系统默认 Qt/FFmpeg 版本不等于本项目版本。SDK 库为 x86_64，不支持直接与其他 CPU 架构或任意 Qt/编译器 ABI 混用。输出在 build/ubuntu-release；目标桌面需支持 OpenGL 和中文字体。

## 内部开发

只有内部工程具有 private/ShEarthSDK 与 private/ShVideoSDK 时才能选择 SOURCE 模式。内部视频 SDK 构建还需要 FFmpeg 7 开发头文件和库。公开源码不包含这些目录，不可通过源码自动下载包重建 SDK。

缺少 SDK 时可关闭对应后端：SHDISPLAY_ENABLE_EARTH=OFF 或 SHDISPLAY_ENABLE_VIDEO=OFF。这种构建不具有被关闭的地图或 RTSP 功能。

默认项目使用空白地球与脱敏模板；重复构建不覆盖用户方案。地图配置见 [CONFIGURATION.md](CONFIGURATION.md)，火灾示例见 [示例说明](../examples/warehouse-fire/README.md)。自动测试不代替真实地图、显卡与 RTSP 码流实测。
