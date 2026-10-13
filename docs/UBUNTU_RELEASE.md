# Ubuntu 运行方式

适用目标：Ubuntu 22.04 及以上 x86_64 图形桌面。

完整运行包应包含 ShDisplayViewer、run.sh、lib、qt-plugins、Data、shdisplay-project.json、图标和 doc/USER_GUIDE.pdf。不要只复制二进制文件。运行目录应放在当前用户可写的位置。

```bash
cd "解压后的程序目录"
bash run.sh
```

`bash` 用于执行 shell 脚本。包内 run.sh 定位自身目录并设置随包库和插件路径；目标机器仍需正常的系统图形库、OpenGL 驱动和中文字体。

```bash
bash install-desktop.sh
```

上面的命令用于安装解压目录的桌面快捷方式。桌面提示不受信任时，确认来源后右键选择“允许启动”。移动程序目录后，重新执行快捷方式安装脚本；不能直接复用指向原目录的 .desktop。

从源码构建请阅读 [BUILD.md](BUILD.md)，使用 scripts/run-ubuntu.sh。SDK 开发包不等于完整运行包，也不包含全部外部开发依赖。公开源码模板没有现场地图和缓存；配置自己的 TMS/XYZ 或合法本地数据后再验证地图显示。

出现找不到库或平台插件的错误时，先确认使用完整包与 run.sh。地图空白应检查 .earth 入口、服务可达性、坐标系、瓦片方向和覆盖区域，不能仅凭“初始化成功”判断影像加载成功。
