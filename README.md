# ShDisplayViewer

![ShDisplayViewer 图标](resources/application/shdisplayviewer-128.png)

**三维地图部署与步骤演练系统，支持 Windows 和 Ubuntu。**

将事件位置、行动路线、力量部署和关联信息组织成可编辑、可播放的演练方案，用于桌面推演、方案汇报和培训展示。核心流程是 **方案 → 步骤 → 动态标记 → 视角与关联动作**。

项目维护方：**shunhe**；开发者：**青岛顺和信息技术有限公司**；官网：[www.shunhexinxi.com](http://www.shunhexinxi.com)。

基于 C++17 / Qt 6，使用 EarthSDK 地图组件和 FFmpeg 视频封装。Viewer 无需 IMS 服务、登录、激活或角色权限；项目保存在本地，在线地图、RTSP 和网页可按需联网。

[使用操作说明](docs/USER_GUIDE.md) · [PDF 手册](docs/USER_GUIDE.pdf) · [地图配置](docs/CONFIGURATION.md) · [源码构建](docs/BUILD.md) · [SDK 接入](sdk/FreeEarthSDK/README.md) · [城市火灾示例](examples/warehouse-fire/README.md) · [更新记录](CHANGELOG.md)

## 功能

| 能力 | 内容 |
| --- | --- |
| 动态演练 | 方案、步骤及动态标记；播放、暂停、停止、前后步骤、时间轴定位和循环；顺序或同时显示 |
| 视角编排 | 保存标记/步骤视角、飞行时间；随步骤及标记切换镜头 |
| 地图部署 | 点线面、箭头标绘、本地标注、图层管理、测量及保存视点 |
| 关联动作 | 鼠标点击或标记出现时自动打开 RTSP、网页或程序；视频和本次启动程序可按 10 秒、30 秒、1 分钟延时关闭 |
| 地图服务 | 本地数据、标准 HTTP TMS、XYZ 及 SDK 支持的其他地图格式；可配置影像缓存 |
| 项目管理 | 新建、打开、保存、另存及导出；导入旧 Qt 动态标记 XML；演示副本 |
| 窗口与手册 | 两端蓝色标题栏、窗口全屏/还原、地图全屏、帮助菜单打开本地 PDF |

系统浏览器网页不提供延时关闭。Windows 与 Linux 程序路径按各自系统填写。Viewer 不提供真实导航、车辆连续行驶、火势物理仿真或灾害处置决策。

## 使用

普通用户使用包含全部依赖的 Viewer 运行包。Windows 解压到可写目录后双击 `ShDisplayViewer.exe`；Ubuntu 22.04 及以上 x86_64 图形桌面使用包内 `bash run.sh`，可执行 `bash install-desktop.sh` 创建快捷方式。不要只复制主程序。

**本仓库是源码仓，FreeEarthSDK 为单独分发的闭源二进制 SDK。** 完整源码构建需取得匹配平台的 Viewer 专用 SDK 开发包和外部开发依赖。SDK 与 Viewer 运行包应从本仓库 Releases 中分别获取；若维护方尚未附加下载文件，请查看 SDK 接入说明，不能用源码自动下载包代替 SDK。

公开源码默认是空白离线地球，没有预设商业底图、现场高程、地图缓存、摄像机地址或业务账号。请按 [地图配置说明](docs/CONFIGURATION.md) 配置有权使用的地图服务或本地数据。

首次构建自带简单三步骤演练。要查看较完整场景，使用“项目 → 导入 Qt 动态标记 XML…”选择 `examples/warehouse-fire`，另选空目录保存新项目，再配置自己的地图。

城市仓库火灾示例包含 **8 个阶段、30 个标记、150 秒播放时间**：发现报警、疏散接应、首批力量前往、到场部署、请求增援、增援协同、火情控制和清点复盘。所有设施、点位与路线关系均为虚构，不能作为真实道路导航或消防预案使用。

## 开发

- C++17、CMake 3.25+、Qt 6.11.x。
- Windows x86_64：MSVC 2022，Release /MD。
- Ubuntu 22.04 x86_64：GCC 11 作为构建基线。
- FreeEarthSDK 4.0.0、OSG 3.6.5、osgEarth 3.8；视频后端使用 FFmpeg 7。

公开源码使用 `SHDISPLAY_EARTH_PROVIDER=PACKAGE`。没有 SDK 时可关闭地图后端开发界面，但这不是完整地图产品。逐步命令与依赖说明见 [BUILD.md](docs/BUILD.md)；跨平台检查范围见 [VALIDATION.md](docs/VALIDATION.md)。

默认项目优先级：命令行 `--project` → 程序旁项目 → 程序旁 `Projects/default`。缓存、设置、日志和临时副本保存在程序目录；运行目录须可写。重复构建保留用户现有配置。

## 目录

| 路径 | 内容 |
| --- | --- |
| `src/` | Viewer 应用、界面与演练模块 |
| `tests/` | 项目、配置、标记、动作及播放测试 |
| `sdk/videoSDK/` | 公开的视频封装源码和接口，外部依赖 FFmpeg/Qt |
| `sdk/FreeEarthSDK/` | 地图 SDK 公共接口与接入说明，不含闭源实现 |
| `resources/`、`assets/` | 可分发图标、纹理、模型和清洁项目模板 |
| `examples/warehouse-fire/` | 虚构城市火灾演练示例与生成脚本 |
| `scripts/` | 公开构建、运行、发布筛查及快捷方式脚本 |
| `docs/` | 操作、地图配置、SDK、构建和发布说明 |
| `.github/` | 问题反馈和贡献模板 |

## 许可

Viewer、VideoSDK 自研源码和随附已授权素材采用 [Apache-2.0](LICENSE)。项目署名为 shunhe，见 [AUTHORS.md](AUTHORS.md) 和 [NOTICE](NOTICE)。

FreeEarthSDK 自研实现与二进制保持闭源；Viewer 免费免激活使用，SDK 在其他产品中集成需另行商业授权。最终 SDK 下载包采用独立 Viewer 专用条款，不受根目录 Apache-2.0 覆盖。其边界见 [SDK 接入](sdk/FreeEarthSDK/README.md)。

Qt、FFmpeg、OSG/osgEarth、GDAL 等第三方组件保留各自许可证。源码仓不附带这些第三方二进制；另行发布运行包时需提供与实际依赖版本和构建相符的声明及源码获取资料，见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

## 技术服务与定制

面向直接使用演练软件的单位，提供行业流程与功能定制、业务系统集成、地图与视频接入、私有环境部署、培训与技术支持。具体范围、交付及费用单独约定。

| 信息 | 内容 |
| --- | --- |
| 项目维护 | shunhe |
| 公司 | 青岛顺和信息技术有限公司 |
| 官网 | [www.shunhexinxi.com](http://www.shunhexinxi.com) |
| 商务邮箱 | 【无】 |
| 其他公开联系方式 | 【无】 |

## 反馈与贡献

欢迎通过 Issues 提交问题和建议；请提供版本、操作系统、复现步骤及脱敏样例。不要公开设备凭据、令牌、现场敏感数据或无权分发的地图。

代码与文档贡献请阅读 [CONTRIBUTING.md](CONTRIBUTING.md)。SDK 商业集成与行业定制请通过官网联系。
