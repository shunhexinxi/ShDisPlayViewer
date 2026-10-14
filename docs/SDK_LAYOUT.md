# SDK 文件位置与本地整合目录

ShDisplayViewer 的 Windows 和 Ubuntu 版本共用一套 Viewer 源码。操作系统相关代码由条件编译选择；DLL 与 SO 必须分平台使用，不能互相替代。

## 公共接口

| SDK | 本地文件 | 作用 |
| --- | --- | --- |
| FreeEarthSDK | `sdk/FreeEarthSDK/include/FreeEarthSDK/EarthRuntime.h` | 地图、标记、视角、环境和飞行等公共接口；接口版本 4.0.0 |
| VideoSDK | `sdk/videoSDK/include/VideoSDK/VideoPlayer.h` | RTSP 视频播放器公共接口；SDK 版本 7.0.0 |

FreeEarthSDK 的旧 Fe* 私有头文件属于闭源实现，不是 Viewer 构建所需的公开接口。

## 本地 SDK 开发包

维护方的本地整合目录可包含以下独立 SDK 开发包；GitHub 源码导出工具和 `.gitignore` 均排除这些二进制目录。

| 文件或目录 | 作用 |
| --- | --- |
| `sdk/FreeEarthSDK/windows-x64/include/` | Windows SDK 公共头文件 |
| `sdk/FreeEarthSDK/windows-x64/bin/FreeEarthRuntime.dll` | Windows 地图 SDK 主动态库 |
| `sdk/FreeEarthSDK/windows-x64/bin/Fe*.dll` | 地图 SDK 内部运行依赖 |
| `sdk/FreeEarthSDK/windows-x64/bin/osgPlugins-3.6.5/` | SDK 地图插件 |
| `sdk/FreeEarthSDK/windows-x64/lib/FreeEarthRuntime.lib` | MSVC 链接所需的 DLL 导入库，不是另一份运行程序 |
| `sdk/FreeEarthSDK/windows-x64/lib/cmake/FreeEarthSDK/` | Windows SDK 的 CMake 查找配置 |
| `sdk/FreeEarthSDK/ubuntu2204-x64/include/` | Ubuntu SDK 公共头文件 |
| `sdk/FreeEarthSDK/ubuntu2204-x64/lib/libFreeEarthRuntime.so` | Ubuntu 地图 SDK 主动态库 |
| `sdk/FreeEarthSDK/ubuntu2204-x64/lib/libFe*.so` | 地图 SDK 内部运行依赖 |
| `sdk/FreeEarthSDK/ubuntu2204-x64/lib/osgPlugins-3.6.5/` | Ubuntu SDK 地图插件 |
| `sdk/FreeEarthSDK/ubuntu2204-x64/lib/cmake/FreeEarthSDK/` | Ubuntu SDK 的 CMake 查找配置 |
| `sdk/videoSDK/prebuilt/windows-x64/bin/videoSDK.dll` | 单独构建的 Windows VideoSDK 动态库 |
| `sdk/videoSDK/prebuilt/windows-x64/lib/videoSDK.lib` | Windows VideoSDK DLL 导入库 |
| `sdk/videoSDK/prebuilt/ubuntu2204-x64/lib/libvideoSDK.so` | Ubuntu VideoSDK 动态库 |

VideoSDK 默认可静态链接，此时 Windows 没有独立 `videoSDK.dll`，视频代码已经链接进 Viewer。需要独立动态库时配置 `VIDEOSDK_BUILD_SHARED=ON`。这些二进制开发包不包含完整 Qt、FFmpeg、OSG/osgEarth 等第三方环境，不能作为 Viewer 安装包直接运行。

独立使用 VideoSDK 动态库时，应使用 `find_package(videoSDK 7.0 CONFIG REQUIRED)` 和 `target_link_libraries(... VideoSDK::Runtime)`，由导入目标传递头文件路径及 `VIDEOSDK_SHARED=1`。Windows 手工配置链接时也必须定义 `VIDEOSDK_SHARED=1`，使 Qt 元对象数据使用 DLL 导入声明。

## 使用地图 SDK

公开源码使用 `SHDISPLAY_EARTH_PROVIDER=PACKAGE`。将对应 FreeEarthSDK 开发包解压到上表的平台目录，未指定其他 SDK 路径时，CMake 会尝试从该目录查找。也可显式选择任意目录：

```bash
cmake -S . -B build/viewer -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DSHDISPLAY_EARTH_PROVIDER=PACKAGE \
  -DFreeEarthSDK_DIR=/path/to/sdk/lib/cmake/FreeEarthSDK
```

另需准备匹配的 Qt、FFmpeg 和平台依赖，完整命令见 [BUILD.md](BUILD.md)。本地有私有 SDK 实现时也可使用 SOURCE 模式重新构建 SDK；私有实现不进入 GitHub 源码包。

## 分发边界

公共源码与公共接口按项目声明开放；FreeEarthSDK 实现及其二进制按独立 Viewer 专用条款提供。SDK 二进制包应作为独立附件分发，草案条款仍需发布方定稿。不要把整个本地 ShDisplayViewer 开发目录上传 GitHub，请使用源码导出工具生成的候选目录。
