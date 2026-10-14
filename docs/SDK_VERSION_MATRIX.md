# SDK 与平台对应

Viewer 1.0.2 使用 ShEarthSDK 公共接口 4.0.0 与 ShVideoSDK 公共接口 7.0.0。两个 SDK 均为闭源实现，公开仓库只包含公共接口声明；完整源码构建对两个 SDK 都选择 PACKAGE。

| SDK 包 | 适用环境 | 包内容 | 外部开发依赖 |
| --- | --- | --- | --- |
| ShEarthSDK-Windows-x64-4.0.0-20261014 | Windows x86_64，MSVC 2022 Release /MD | 公共头文件、DLL、导入库、地图插件和 CMake 接入文件 | Qt 6.11.x MSVC x64、OSG 3.6.5、osgEarth 3.8 及对应开发库 |
| ShEarthSDK-Ubuntu2204-x64-4.0.0-20261014 | Ubuntu 22.04 x86_64，GCC 11 Release | 公共头文件、.so、地图插件和 CMake 接入文件 | Qt 6.11.x、OSG 3.6.5、osgEarth 3.8 及对应开发库 |
| ShVideoSDK-Windows-x64-7.0.0-20261014 | Windows x86_64，MSVC 2022 Release /MD | 公共头文件、videoSDK.dll、导入库和 CMake 接入文件 | Qt 6.11.x MSVC x64；运行时需要匹配的 FFmpeg 7 DLL |
| ShVideoSDK-Ubuntu2204-x64-7.0.0-20261014 | Ubuntu 22.04 x86_64，GCC 11 Release | 公共头文件、libvideoSDK.so 和 CMake 接入文件 | Qt 6.11.x；运行时需要匹配的 FFmpeg 7 共享库 |

本批 SDK 包保留已验证的 SDK 二进制，地图 SDK 包含 2026-10-09 TMS 相对 TileSet 地址修复；各 SDK 两端公共头文件一致。ShEarthSDK 目录和 CMake 包名已更新，FreeEarthRuntime 库名与 FreeEarth:: 命名空间保留 ABI 兼容。包名中的日期表示本次整理日期，不表示重新编译了 SDK。

SDK 包不包含完整 Qt、OSG、osgEarth 或 FFmpeg 开发环境。它不能替代完整的 Viewer 运行包。不同编译器、Qt 主版本、架构或运行库不能直接混用。Ubuntu 22.04 作为构建基线，其他系统和显卡仍需验证。

SDK 二进制单独作为下载附件提供，不放入源码仓；发布方需要在首次发布时提供对应下载附件及最终 Viewer 专用条款。下载渠道尚未上线时，不应宣称克隆源码即可无条件构建完整地图客户端。

当前 Windows 插件包含 FlatGIS、HgBump、WMTS、HgVector；Ubuntu 包含 FlatGIS、HgBump、WMTS，HgVector 仍为 Windows 专用。标准 TMS 与 XYZ 不依赖 HgVector。
