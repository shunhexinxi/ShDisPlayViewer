# ShVideoSDK 二进制接入

维护署名：shunhe；开发者：青岛顺和信息技术有限公司。接口版本 7.0.0，依赖 Qt 6.11.x 和 FFmpeg 7。

自 2026-10-14 起，本目录只提供公共头文件 `include/ShVideoSDK/VideoPlayer.h` 和接入说明。自研实现放在 `private/ShVideoSDK/`，不进入公开源码仓或 SDK 开发包；平台开发包只包含公共头文件、DLL/SO、导入库及必要的 CMake 元数据、说明和声明。

把对应平台包解压到 `windows-x64/` 或 `ubuntu2204-x64/` 后，Viewer 的 PACKAGE 模式可自动查找。也可显式指定 `ShVideoSDK_DIR=/path/to/sdk/lib/cmake/ShVideoSDK`。目录清单见 [SDK_LAYOUT.md](../../docs/SDK_LAYOUT.md)。

```cmake
find_package(ShVideoSDK 7.0 CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE VideoSDK::Runtime)
```

导入目标会传递 `VIDEOSDK_SHARED=1`；Windows 手工链接 DLL 时也需定义该宏，使 Qt 元对象使用 DLL 导入声明。SDK 不包含完整 Qt/FFmpeg 运行环境。

SDK 目录、公共头文件路径与 CMake 包名为 ShVideoSDK；为兼容已有二进制，库名保留 videoSDK.dll / libvideoSDK.so，C++ 命名空间及 CMake 目标保留 VideoSDK::。这不会改变 SDK 的闭源发布边界。

公共接口声明继续按项目 Apache-2.0 许可提供，自研实现及二进制采用独立条款，见 [SDK 专用条款草案](../../docs/SDK_VIEWER_TERMS_DRAFT.md)。历史版本若已按 Apache-2.0 对外提供，已有授权仍按原条款有效。Qt 和 FFmpeg 保留各自许可证。
