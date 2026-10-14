# 地图与功能配置

公开源码默认项目位于程序旁，包含 shdisplay-project.json、Data/Config 和 Data/earth。模板是空白离线地球，不包含现场地图、数据缓存或设备账号。通过界面保存演练和标注；手工编辑 XML 前退出程序并备份。

## 配置文件对应

| 文件 | 作用 |
| --- | --- |
| shdisplay-project.json | 项目识别、版本和登记资源清单 |
| Data/Config/systemConfig.xml | 主配置入口，common earth 选择 .earth；viewPoints/viewPointEnd 提供启动和 Home 视角 |
| Data/earth/FreeEarth_flat.earth | 公开模板的空白地球，可添加系统影像、高程或服务 |
| Data/earth/examples/HttpTms.earth | TMS 配置示例，服务节点默认注释 |
| Data/earth/examples/HttpXyz.earth | XYZ 模板及 Y 轴方向示例，服务节点默认注释 |
| Data/Config/customLayerConfig.xml | 界面中新增的自定义图层及数据源参数 |
| Data/Config/featureConfig.xml | 系统矢量定义和样式；公开模板为空，不附带现场矢量数据 |
| Data/Config/fixed-spatial.xml | 场景、测量和图层状态的兼容快照；不是地图图层定义的替代文件 |
| Data/Config/marksConfig.xml、plotsConfig.xml | 基础场景固定标记与标绘，属性和关联动作 |
| Data/Config/viewPointConfig.xml | 保存的视点 |
| Data/Config/squad-annotations.xml | 本地标注的文件夹、标记、显隐和关联动作 |
| Data/Config/dynamic-plans.xml | 演练方案、步骤、标记、视角、时间和动作，建议用界面编辑 |
| Data/Config/normal-annotations.xml | 历史部署数据兼容文件 |
| Data/model/H6/routes/flight_routes.xml | 飞行路线配置；模板不附带该旧工程的大型模型数据 |

以主配置、customLayerConfig.xml、featureConfig.xml 的实际定义为准。修改 .earth 或矢量定义后重新打开项目；删除地图定义不能只改 fixed-spatial.xml 的快照。

## 路径基准

```xml
<common earth="earth/FreeEarth_flat.earth" title="ShDisplayViewer"/>
```

推荐的 `earth/...` 相对路径以当前项目 Data 为基准。改为其他文件名时同时修改 earth 属性，并保留实际文件。SHDISPLAY_EARTH_FILE 环境变量可覆盖主配置，通常不设置。

`.earth` 内的相对 URL 和显式缓存路径以该 .earth 所在目录为基准。若 .earth 位于 Data/earth，影像文件位于 Data/assets/world.tif，URL 可写 `../assets/world.tif`。

自定义图层的相对数据源由驱动按启动工作目录读取。程序旁 `map/my-elevation` 是可选布局示例，不是随源码提供的数据。Ubuntu 使用启动脚本，Windows 快捷方式的“起始位置”设置为程序目录。Linux 区分文件名大小写。

## 系统图层：TMS

把 TMS 元数据 URL 填入实际使用的 .earth，节点放在已有 map 内：

```xml
<TMSImage name="自有 TMS 底图">
  <url>https://example.invalid/tiles/tilemapresource.xml</url>
</TMSImage>
```

example.invalid 是占位地址，不提供地图。实际地址应返回 TileMap 元数据而非登录网页，瓦片坐标系、格式和层级以服务为准。ShEarthSDK 本批 4.0.0 包含 TMS 相对 TileSet href 修复，不需要另造本地兼容 XML。配置后重新打开项目并定位到服务覆盖区，确认影像实际显示。

TMS 高程可用 TMSElevation，但需服务提供可用的高程数据格式；不能把普通 jpg 影像当作高程。

## 系统图层：XYZ

```xml
<XYZImage name="自有 XYZ 底图">
  <url>https://example.invalid/tiles/{z}/{x}/{y}.png</url>
  <profile>global-mercator</profile>
  <min_level>0</min_level>
  <max_level>18</max_level>
</XYZImage>
```

上例只表示模板写法，18 不是任意服务的默认上限。global-mercator 用于对应墨卡托布局；WGS84 经纬度的 2×1 根瓦片布局常用 global-geodetic。北向下编号通常使用 {y}，南向上编号通常使用 {-y}，必须以实际服务说明为准。URL 查询字符串中的 & 在 XML 中写成 &amp;。

通过“基础场景 → 自定义图层”添加的图层保存到 customLayerConfig.xml，不会自动写入系统 .earth。

## 本地高程与矢量

FlatGIS 高程需要匹配实际文件的 profile、quadtreeType、format、minLevel、maxLevel 和 tileSize。google 表示对应北向下行号，tms 表示对应南向上行号；不能仅靠修改级别上限生成缺失瓦片。系统矢量使用 featureConfig.xml，数据源、样式和可用格式须由用户提供。

公开源码不包含底图瓦片、高程或现场矢量；用户应自行取得使用及分发权，运行时检查实际数据，而非只检查目录存在。

## 缓存与服务断开

模板使用文件系统缓存，省略 path 时 Viewer 使用程序旁的 cache/earth。已有 `.earth` 可显式设置：

```xml
<options>
  <lighting>false</lighting>
  <cache type="filesystem"><path>../../cache/earth</path></cache>
  <cache_policy usage="read_write"/>
</options>
```

合并到已有 options，不要创建重复节点。上述路径适用于程序 Data/earth 下的文件，指向程序旁 cache/earth；移动 .earth 到别处时需要重新计算相对关系。

read_write 优先读取有效缓存，缺失时联网读取并写入缓存。服务断开时已有缓存仍可使用，未缓存区域/级别不会自动生成。cache_only 只读取缓存；应先在正常联网模式加载该图层，以生成瓦片与图层元数据。no_cache 禁用缓存。

图层内 cache_id 指定 path 下一层缓存标识。固定标识可帮助同一数据源在 URL 调整后复用缓存，前提是数据集、坐标系和切片方式一致。真正更换地图数据源应使用新标识，避免地图混用。改变缓存目录不会自动搬迁旧数据。自定义图层实际缓存统一写到程序旁 cache/earth。

## 程序生成目录

| 目录 | 作用 |
| --- | --- |
| cache/earth | 地图缓存 |
| cache/temporary | 演示副本与临时资源 |
| cache/fontconfig | 字体缓存 |
| settings | 程序设置 |
| log | 运行日志 |
| Projects/default | 程序旁没有项目清单时使用的默认项目 |

以上相对可执行文件目录，不使用系统用户目录保存程序缓存。源码仓不附带这些运行目录。程序目录必须可写。

项目另存、导出和演示副本不会自动收集任意外部地图数据或缓存；改名的 .earth 与自定义数据应另外核对是否已携带。RTSP、网页和程序动作应通过属性界面编辑，公开样例中不要保存真实账号。

详细操作与字段示意见 [USER_GUIDE.md](USER_GUIDE.md)。驱动依据：[osgEarth TMS](https://docs.osgearth.org/en/latest/tms.html)、[XYZ](https://docs.osgearth.org/en/latest/xyz.html)、[Earth File](https://docs.osgearth.org/en/latest/earthfile.html)。
