# 源码与 SDK 发布边界

Viewer 源码、公共接口声明及已授权素材按 Apache-2.0 公开；ShEarthSDK 和 ShVideoSDK 的自研实现与二进制按独立条款分发。维护署名为 shunhe，开发者为青岛顺和信息技术有限公司。

## Viewer 源码仓

包含 src/、tests/、公开构建脚本、清洁配置模板、素材、文档，以及两个 SDK 的各一个公共接口头文件和接入说明。SDK 实现、内部头文件、私有 CMake/qmake 工程、DLL/SO、第三方开发环境、现场数据与缓存不进入源码仓。

## SDK 开发包

分别提供 ShEarthSDK 与 ShVideoSDK 的 Windows / Ubuntu 包。开发包只含公共头文件、DLL/SO、Windows 导入库、CMake 查找配置、说明及适用声明；禁止包含任何 SDK 的 CPP/C/CXX 实现、内部头文件、私有构建工程和调试源码。

普通用户使用完整 Viewer 运行包；开发者取得两个 SDK 包后，以 PACKAGE 模式构建 Viewer。两个 SDK 自研实现保持闭源，专用授权准备稿见 [SDK_VIEWER_TERMS_DRAFT.md](SDK_VIEWER_TERMS_DRAFT.md)，最终授权仍需维护方审定。SDK 闭源不改变 Qt、FFmpeg 或其他第三方的许可与分发义务。

## 历史发布

如果 ShVideoSDK 的旧版本已按 Apache-2.0 对外发布，新的闭源发布方式不追溯撤销已经授予的许可。网页删除文件也不会移除 Git 提交历史中的旧实现。已准备的旧公开材料归入本地私有历史目录，不能再作为新版上传包。

## 导出与检查

```bash
python3 scripts/export-public.py --name source-candidate
python3 scripts/check-public.py exports/source-candidate
```

导出工具只访问指定源码、模板和文档，并剪除私有实现、平台二进制、依赖、地图数据与缓存。检查器禁止 SDK 实现文件和内部头文件出现在公开候选中。SDK 二进制作为单独附件准备，不能用整个内部工程代替公开候选。

源码检查只验证明显误带文件及敏感字段，不证明许可合规或真实设备兼容。仓库填写方式见 [GITHUB_SETUP.md](GITHUB_SETUP.md)。
