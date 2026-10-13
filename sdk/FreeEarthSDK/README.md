# FreeEarthSDK 接入

自研 SDK 维护署名：**shunhe**；对外开发者：青岛顺和信息技术有限公司。

本目录只提供 `include/FreeEarthSDK/EarthRuntime.h` 公共接口和接入说明。FreeEarthSDK 自研实现与 DLL/.so 保持闭源，不受源码仓根目录 Apache-2.0 授权。完整 Viewer 使用独立 Viewer 专用 SDK 开发包，以 PACKAGE 模式构建。

## 获取与版本

公共接口版本 **4.0.0**，Viewer 版本 **1.0.2**。当前独立 SDK 包名与依赖矩阵见 [SDK_VERSION_MATRIX.md](../../docs/SDK_VERSION_MATRIX.md)。首次发布时维护方需把对应 Windows 和 Ubuntu 包单独附加到 Releases；SDK 开发包与 Viewer 运行包是不同下载文件。

SDK 支持 Windows MSVC 2022 x86_64 Release /MD 和 Ubuntu 22.04 GCC 11 x86_64 Release。依赖 Qt 6.11.x、OSG 3.6.5、osgEarth 3.8 等开发及运行库，不承诺跨编译器或任意发行版的 C++ ABI 稳定。

本批 SDK 包包含 2026-10-09 TMS 相对 TileSet 地址兼容修复：以元数据地址为基准在内存中补全相对链接，不改写服务端 XML。仅重新编译 Viewer 不会更新旧 SDK。

## 构建

```bash
cmake -S . -B build/viewer -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DSHDISPLAY_EARTH_PROVIDER=PACKAGE \
  -DFreeEarthSDK_DIR=/path/to/FreeEarthSDK/lib/cmake/FreeEarthSDK
cmake --build build/viewer
```

外部 Qt、OSG/osgEarth 和 FFmpeg 依赖仍需设置。Windows 包含 bin/DLL、lib/导入库和 CMake 元数据，Ubuntu 包含 lib/.so 和 CMake 元数据。地图插件和所需 Fe 库必须随对应运行包部署。具体命令见 [BUILD.md](../../docs/BUILD.md)。

没有 SDK 时可使用 SHDISPLAY_ENABLE_EARTH=OFF 开发界面和数据功能，但没有真正的三维地图。

## 专用授权

产品方向为 Viewer 免费免激活运行，可修改、重新构建 Viewer；FreeEarthSDK 在其他产品中独立集成另行商业授权。SDK 二进制下载包必须包含发布方审定的最终条款与适用声明。准备文本见 [Viewer 专用条款草案](../../docs/FREEEARTH_VIEWER_TERMS_DRAFT.md)，草案尚不代表已经生效的二进制授权。

公共接口和源码模板不包含私有实现、SDK 二进制或现场地图。第三方组件保留自己的许可证，专用 SDK 条款不缩减这些组件的既有开源权利。
