# GitHub 建仓与首次发布填写说明

本说明用于上传已经整理的公开源码目录，不直接上传内部工程或现场运行包。FreeEarthSDK 实现闭源，二进制开发包单独作为附件候选；发布前需审定其 Viewer 专用条款。

## 建仓页面怎么填写

打开 GitHub 的 New repository 页面。

| 字段 | 建议填写 |
| --- | --- |
| Owner | 选择你实际登录的 GitHub 账号或有权限的公司组织；不是填公司全称 |
| Repository name | `ShDisplayViewer` |
| Description | `跨平台三维地图部署与步骤演练系统，支持 Windows / Ubuntu、TMS / XYZ、RTSP 及本地方案编辑，无需 IMS 登录。` |
| Visibility | `Public`，审核完上传材料后公开 |
| Add a README file | 不勾选，整理目录已有 README.md |
| Add .gitignore | 不选模板，目录已有 .gitignore |
| Choose a license | 不让网页重新生成，目录已有完整 Apache-2.0 LICENSE |

然后点击 Create repository。避免同时生成不同 README/许可证造成第一次推送冲突。流程依据：[GitHub 建仓说明](https://docs.github.com/en/repositories/creating-and-managing-repositories/creating-a-new-repository)。

## 仓库 About 区

| 内容 | 建议 |
| --- | --- |
| Description | 使用上表描述 |
| Website | `http://www.shunhexinxi.com` |
| Topics | `qt`、`cpp`、`3d-map`、`osgearth`、`simulation`、`emergency-drill`、`desktop-application`、`windows`、`linux`、`rtsp` |

README 已写入 shunhe、青岛顺和信息技术有限公司和官网。商务邮箱及其他公开联系方式保留【待补充】，不要把示例填成不存在的邮箱。

## 上传哪些内容

把源码候选目录的**内容**作为仓库根：根目录要直接看到 CMakeLists.txt、README.md、LICENSE、src、sdk、resources 和 docs。不要把整个内部 ShDisplayViewer 目录或源码 ZIP 当作仓库唯一文件。

源码 ZIP 是交付介质，上传前解压。隐藏文件 `.gitignore`、`.gitattributes`、`.github/` 也要保留。文件清单由交付包中的 SOURCE-MANIFEST.json 与外部上传清单核对。

不上传：private、dependencies、build、release、runtime、dist、现场 map、cache、settings、log、真实项目配置、SDK DLL/.so、大型运行 ZIP 和设备凭据。

### 推荐使用 GitHub Desktop

1. 安装并由你自己登录 GitHub Desktop，选择 File → Add local repository，定位解压后的干净源码目录。
2. 如果提示不是 Git 仓库，选择创建本地仓库，确认使用当前源码目录作为仓库根，没有再套一层同名目录。
3. 检查 Changes 仅包含上传清单中的文件；提交说明可填 `Initial public source release for ShDisplayViewer 1.0.2`。
4. 如果已在网页建仓，用 Repository settings 的 Remote 配置其真实仓库地址，再推送；如果尚未建仓，可用 Publish repository，名称 ShDisplayViewer，并取消 Keep this code private 后发布。

具体按钮可能随客户端版本变化，以当前界面为准；不要同时创建两个同名远端仓库。只在这份隔离源码目录初始化 Git，不在内部工作工程根目录操作。

### 命令行方式

以下命令由你在确认后的源码目录执行。本次整理并未运行 git init、commit、push，也没有读取登录令牌。

```bash
cd "解压后的源码目录"
git init -b main
git add .
git diff --cached --stat
git commit -m "Initial public source release for ShDisplayViewer 1.0.2"
git remote add origin https://github.com/你的账号或组织/ShDisplayViewer.git
git push -u origin main
```

将“你的账号或组织”换成实际仓库 Owner；不要照抄占位地址。认证使用 GitHub Desktop 或你已有的正常凭据管理方式，不在命令或源码中写密码/令牌。GitHub 的 git push 不能用普通账号密码代替令牌/SSH 认证。依据：[上传文件](https://docs.github.com/en/repositories/working-with-files/managing-files/adding-a-file-to-a-repository)、[命令行认证](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/about-authentication-to-github)。

网页 Upload files 每批最多 100 文件，单文件最多 25 MiB，且不会按 .gitattributes 自动规范换行。本源码有多于 100 个文件，建议使用 Desktop/Git；若网页上传，需要分批并保持完整目录结构。[GitHub 文件上传限制](https://docs.github.com/en/repositories/working-with-files/managing-files/adding-a-file-to-a-repository)。

## 首次 Release

| 字段 | 建议填写 |
| --- | --- |
| Tag | `v1.0.2`，发布确认过的同一份源码提交 |
| Target | `main`，核对是本次验证的提交 |
| Release title | `ShDisplayViewer 1.0.2` |
| Description | 下面的发布说明示例，按实际附加文件调整 |
| Draft | 附件与 SDK 条款尚未审定时先保存为草稿 |
| Pre-release | 仅发布测试版本时选；已有测试不能替代对所有目标机器的兼容保证 |

可复制的说明：

```text
ShDisplayViewer 1.0.2

三维地图部署与步骤演练客户端，支持 Windows 和 Ubuntu x86_64。
提供方案/步骤/动态标记、视角编排、本地标注、地图图层与 RTSP 关联动作。
无需 IMS、登录或激活。Viewer 和 VideoSDK 自研源码采用 Apache-2.0。

FreeEarthSDK 为单独提供的闭源 Viewer 专用开发包，完整源码构建需该包及外部开发依赖。
公开模板不含现场底图、高程、缓存、设备账号或真实项目。
操作手册见 docs/USER_GUIDE.pdf；地图配置见 docs/CONFIGURATION.md。
虚构城市火灾示例位于 examples/warehouse-fire。

开发者：青岛顺和信息技术有限公司
项目维护：shunhe
官网：http://www.shunhexinxi.com
商务邮箱：【待补充】
```

GitHub 自动提供 Source code ZIP/tar.gz，它们只代表该标签的源码。独立 SDK 包和完整 Viewer 运行包应另外作为 Release assets，不能把技术 SDK 包称为 Viewer 安装包。[GitHub Releases 说明](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases)。

此次交付的两份 SDK 开发包是技术候选；正式作为下载附件前须用审定的最终条款替换草案，并保留适用声明。此前包含现场地图 URL、高程与 cache 的运行包不列入本次公开附件，不直接上传。

## 发布前最后核对

- 仓库根结构与文件清单一致，`.github` 等隐藏内容未漏。
- Viewer/VideoSDK/素材许可证与 SDK 二进制边界写清楚，第三方声明保留。
- 商务邮箱如尚未确定，可保留待补充并以官网联系；不编造邮箱和 GitHub Owner。
- 若说明中承诺 SDK 下载，实际 Releases 必须已附加匹配包及最终条款。
- 公开源码配置不含内网地址、账号、地图瓦片、日志和缓存；操作手册为公开版。
- 首次上传后打开 README、PDF、示例和构建链接，核对 GitHub 页面能够正常访问。
