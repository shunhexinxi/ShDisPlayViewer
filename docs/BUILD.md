# 从公开源码构建 ShDisplayViewer

源码仓不包含闭源 FreeEarthSDK 实现、第三方开发环境、地图瓦片或摄像机凭据。完整地图版本使用 FreeEarthSDK 4.0.0 二进制开发包，选择 `SHDISPLAY_EARTH_PROVIDER=PACKAGE`。平台矩阵见 [SDK_VERSION_MATRIX.md](SDK_VERSION_MATRIX.md)。

## Windows x86_64

准备 Visual Studio 2022 Build Tools 的 C++ x64 工具、Qt 6.11.x MSVC x64、CMake 3.25+、Ninja，以及含 OSG 3.6.5 / osgEarth 3.8 开发文件的 vcpkg 环境。FFmpeg 7 的开发包应含 `include/`、`libwin64/`、`bin/`；运行库放 bin，头文件和导入库不能由普通 Viewer 安装包代替。

在 cmd 中设置自己的路径：

```bat
set "QT_ROOT=C:\Dev\Qt\6.11.2\msvc2022_64"
set "VCPKG_ROOT=C:\Dev\vcpkg"
set "FREEEARTH_SDK_ROOT=C:\Dev\FreeEarthSDK-Viewer-Windows-x64-4.0.0-20261013"
scripts\Build-Windows.cmd -DSHDISPLAY_EARTH_PROVIDER=PACKAGE -DFreeEarthSDK_DIR="%FREEEARTH_SDK_ROOT%\lib\cmake\FreeEarthSDK" -DVIDEOSDK_FFMPEG_ROOT="C:\Dev\ffmpeg7" -DVIDEOSDK_FFMPEG_RUNTIME_DIR="C:\Dev\ffmpeg7\bin"
scripts\Run-Windows.cmd
```

示例路径不是必需安装位置。脚本会调用 MSVC、配置 CMake、编译 Release、部署库并执行 CTest。输出在 `build/windows-release`，不会使用或修改原 Qt 客户端。公开源码中无私有 SDK 时，PACKAGE 也是默认选择。

## Ubuntu 22.04 及以上 x86_64

准备 GCC/G++、CMake 3.25+、Ninja、pkg-config、Qt 6.11.x、OSG 3.6.5、osgEarth 3.8、GDAL/CURL/OpenGL 开发库、FFmpeg 7 开发库，以及匹配的 FreeEarthSDK 开发包。Ubuntu 22.04 系统默认 Qt/FFmpeg 开发版本不等于本项目要求的版本，应设置自己的依赖前缀。

```bash
export QT_ROOT=/path/to/Qt/6.11.2
export OSG_ROOT=/path/to/OpenSceneGraph-3.6.5
export OSGEARTH_ROOT=/path/to/osgEarth-3.8
export FREEEARTH_SDK_ROOT=/path/to/FreeEarthSDK-Viewer-Ubuntu2204-x64-4.0.0-20261013
export PKG_CONFIG_PATH=/path/to/ffmpeg7/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}
export CMAKE_PREFIX_PATH="$QT_ROOT;$OSG_ROOT;$OSGEARTH_ROOT;$FREEEARTH_SDK_ROOT"
export LD_LIBRARY_PATH="$QT_ROOT/lib:$OSG_ROOT/lib:$OSGEARTH_ROOT/lib:/path/to/ffmpeg7/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
bash scripts/build-ubuntu.sh -DSHDISPLAY_EARTH_PROVIDER=PACKAGE -DFreeEarthSDK_DIR="$FREEEARTH_SDK_ROOT/lib/cmake/FreeEarthSDK" -DVIDEOSDK_BUILD_SHARED=ON
bash scripts/run-ubuntu.sh
```

输出在 `build/ubuntu-release`。运行脚本设置当前 SDK 和视频库的搜索路径；OSG/osgEarth、FFmpeg 等开发依赖的前缀仍需由构建者设置。图形桌面需支持 OpenGL；中文界面和标注需可用 CJK 字体。

## 无 SDK 时开发界面

可以向上述构建命令追加 `-DSHDISPLAY_ENABLE_EARTH=OFF` 开发界面、XML 数据和步骤播放逻辑。这种构建没有真正的三维地图功能，不能当作完整地图客户端。视频功能若关闭，应同时说明 RTSP 不可用。

```bash
ctest --test-dir build/ubuntu-release --output-on-failure
```

Windows 对应目录是 `build/windows-release`。测试通过不代替目标显卡、地图服务、实际 RTSP 码流和外部程序的验证。

## 默认项目和分发

首次构建在程序旁创建默认项目，使用空白地球和脱敏模板；不含现场底图、高程或设备地址。重复构建保留已有配置，不覆盖用户方案。可按 [CONFIGURATION.md](CONFIGURATION.md) 接入自己的地图，并导入 [火灾示例](../examples/warehouse-fire/README.md)。

普通用户应使用完整 Viewer 运行包；SDK 开发包用于源码构建。发布运行包时需包含实际依赖、插件及适用声明，源码仓的 Apache-2.0 不改变第三方库或闭源 SDK 的许可。
