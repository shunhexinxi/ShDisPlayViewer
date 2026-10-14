# SDK 文件位置与发布内容

两个平台共用 Viewer 源码。ShEarthSDK 和 ShVideoSDK 的实现均为闭源，公共源码仅提供接口声明，开发包提供匹配平台的头文件与库。

| 内容 | 工程内位置 |
| --- | --- |
| 地图公共接口 | sdk/ShEarthSDK/include/ShEarthSDK/EarthRuntime.h |
| 视频公共接口 | sdk/ShVideoSDK/include/ShVideoSDK/VideoPlayer.h |
| Windows 地图 DLL | sdk/ShEarthSDK/windows-x64/bin/FreeEarthRuntime.dll |
| Windows 地图导入库 | sdk/ShEarthSDK/windows-x64/lib/FreeEarthRuntime.lib |
| Ubuntu 地图 SO | sdk/ShEarthSDK/ubuntu2204-x64/lib/libFreeEarthRuntime.so |
| Windows 视频 DLL | sdk/ShVideoSDK/windows-x64/bin/videoSDK.dll |
| Windows 视频导入库 | sdk/ShVideoSDK/windows-x64/lib/videoSDK.lib |
| Ubuntu 视频 SO | sdk/ShVideoSDK/ubuntu2204-x64/lib/libvideoSDK.so |
| 地图 CMake 配置 | 对应平台包的 lib/cmake/ShEarthSDK/ |
| 视频 CMake 配置 | 对应平台包的 lib/cmake/ShVideoSDK/ |
| 地图实现与内部头文件 | private/ShEarthSDK/，不发布 |
| 视频实现与内部头文件 | private/ShVideoSDK/，不发布 |

地图接口版本 4.0.0，视频接口版本 7.0.0。目录和 CMake 包名统一为 ShEarthSDK、ShVideoSDK，头文件引用改用 ShEarthSDK/EarthRuntime.h 与 ShVideoSDK/VideoPlayer.h。FreeEarth::、VideoSDK:: 命名空间以及 FreeEarthRuntime.dll/.so、videoSDK.dll/libvideoSDK.so 和 Fe* 运行依赖保留原名，以维持已有库的 ABI 兼容。

## 开发包边界

每个平台包只提供公共头文件、库文件、CMake 元数据与必要的说明和声明。不含 CPP/C/CXX 实现、内部头文件、私有 CMake/qmake 工程、PDB/DWARF 调试源码或 Viewer 安装程序。SDK 的 DLL/SO 仍需要匹配的 Qt、FFmpeg、OSG/osgEarth 等外部依赖。

公开 Viewer 使用两个 PACKAGE 提供方式。SDK 包放入上表目录后可自动查找，也可显式指定 ShEarthSDK_DIR 与 ShVideoSDK_DIR。VideoSDK::Runtime 会传递 VIDEOSDK_SHARED=1，Windows 手工链接视频 DLL 也需定义该宏。

## 授权

公共接口声明沿用 Apache-2.0，SDK 自研实现及二进制按独立条款分发，准备稿见 [SDK_VIEWER_TERMS_DRAFT.md](SDK_VIEWER_TERMS_DRAFT.md)。历史 Apache-2.0 授权不因后续闭源发布撤回；第三方组件保留自己的许可证。
