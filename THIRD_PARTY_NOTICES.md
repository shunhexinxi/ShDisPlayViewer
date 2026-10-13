# 第三方组件与再分发清单

本文件说明 ShDisplayViewer 的直接依赖和已核实的上游许可证来源，**不替代最终运行包的逐文件许可证清单**。仓库的 Apache-2.0 许可证仅授予有权授权的本项目内容，不改变以下组件的原有许可证。当前公开源码候选不包含这些第三方二进制。

自研代码的公开授权范围由发布方确认；VideoSDK 维护署名统一为 shunhe，不改变 FFmpeg、Qt 或其他上游组件的许可证。此次源码包不包含上游库二进制。

| 组件 | 本工程用途 | 上游许可证依据 | 分发运行包前还需核对 |
| --- | --- | --- | --- |
| Qt 6.11.x | 桌面界面、网络、OpenGL | [Qt 官方许可说明](https://doc.qt.io/qt-6/licensing.html)：所用模块按具体发行形态适用 LGPL-3.0、GPL 或商业许可 | 确认实际安装包各模块与插件的许可、源码获取及 LGPL 要求；不能把“使用 Qt”直接等同于无附加义务 |
| FFmpeg 7.x | RTSP 解码及音视频处理 | [FFmpeg 官方法律说明](https://ffmpeg.org/legal.html)：基本为 LGPL-2.1-or-later，启用 GPL/`nonfree` 选项会改变条件 | 保存对应二进制的配置、精确源码版本、声明和源码提供方式；Windows 与 Ubuntu 分别核验 |
| OpenSceneGraph 3.6.x | 三维场景与插件 | [OpenSceneGraph 官方许可说明](https://openscenegraph.github.io/openscenegraph.io/about/license.html)：OSGPL | 保存与实际库相符的许可证及其依赖清单，检查团队是否修改了上游源码 |
| osgEarth 3.8 | 地图引擎 | [osgEarth 3.8 标签的许可证](https://github.com/pelicanmapping/osgearth/blob/osgearth-3.8/LICENSE.txt)：含 LGPL 条款和项目例外 | 保存该版本许可证，核对修改、链接方式及所带地图插件 |
| GDAL | 地理数据格式 | [GDAL 官方许可说明](https://gdal.org/en/stable/license.html)：主体为 MIT，个别文件和构建依赖另有条款 | 以实际打包的 GDAL 及驱动为准整理全部第三方声明 |

Windows 当前 FFmpeg 来源为构建机上记录的 BtbN FFmpeg 7.1.1 LGPL 共享库包；其精确下载记录保留在内部运行依赖目录。Ubuntu 当前 FFmpeg 7.1.1 构建配置包含 `--enable-shared --disable-static`，已检查未出现 `--enable-gpl` 或 `--enable-nonfree`；仍需保留完整构建配置和对应源码供实际下载包使用。上述检查只覆盖 FFmpeg 本身，不代表 Qt、GDAL 或其他传递依赖已经逐项清理。

闭源 FreeEarthSDK **不**受本仓库 Apache-2.0 许可证覆盖，其 Viewer 专用免费运行/开发包将单独提供并采用独立条款。第三方产品集成须另行授权；条款文本和下载渠道尚未定稿，当前 SDK 技术包仅供内部验证。地图瓦片、矢量数据、RTSP 设备和外部网页不随公开源码提供，也各自受提供方条款约束。
