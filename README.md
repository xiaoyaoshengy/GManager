# GManager

基于 Qt Widgets 的本地游戏库管理器：把游戏收进自己的库，以卡片流浏览，支持从公开刮削源补全信息与封面/横幅，界面使用 ElaWidgetTools 的 FluentUI 风格组件。

## 截图

| 游戏库 | 游戏详情 |
| --- | --- |
| ![游戏库](assets/readme/home.png) | ![详情页横幅](assets/readme/detail_0.png) |
| 卡片流、状态筛选与关键词搜索 | 横幅背景、封面与信息列 |

![详情页介绍](assets/readme/detail_1.png)

详情页左列是富文本介绍，右列是评分、开发者、类别、标签与相关链接。

## 功能

- **游戏库**：卡片流展示本地库，支持按游玩状态筛选、按标题搜索；标题过长时鼠标悬停会滚动显示完整标题；卡片右键可修改、设置游玩状态或删除。
- **添加游戏**：从刮削源搜索条目，选定后挑选背景图，封面与横幅自动下载入库；也可以只填标题手动添加。
- **修改游戏信息**：手动编辑各字段，或用刮削结果做逐字段对比后取舍。
- **修改图片**：封面与横幅可从链接、本地文件或刮削候选图片中挑选，预览确认后写回。
- **设置**：保存刮削服务所需的访问令牌。
- **其他**：单实例运行、操作日志落盘、网络图片缓存与自动清理。

## 环境要求

- Qt 6.8.0 ~ 6.8.3（实测 6.8.3），需要 Widgets、Sql（SQLite 驱动）与 Network 模块；
- CMake ≥ 3.12，支持 C++17 的编译器。

`CMakeLists.txt` 中的 `QT_SDK_DIR` 默认指向作者机器上的 Qt 路径，构建前请改成自己的路径，或用 `-DQT_SDK_DIR=/path/to/qt` 覆盖。

## 构建与运行

```bash
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j"$(nproc)"
./build/GManager
```

## 数据与配置

数据根目录：Windows 为可执行文件所在目录，Linux 为 `~/.local/share/GManager`。数据库 `games.db` 与配置 `config.ini` 位于根目录，封面、横幅、图片缓存与日志分别在 `data/covers`、`data/banners`、`cache/images`、`logs` 下；Windows 对应 `res/covers`、`res/banners`、`cache/images`、`logs`。数据库只保存文件名，迁移时连同目录一起复制即可。

## 致谢

- [ElaWidgetTools](https://github.com/Liniyous/ElaWidgetTools)：FluentUI 风格 Qt Widgets 组件库，本项目的界面基础来自它。
- Qt：GUI 框架、SQLite 驱动与网络模块。
- [Bangumi](https://bgm.tv) 与 [VNDB](https://vndb.org)：感谢提供公开的条目资料与查询接口，条目与封面版权归各站点及原作者所有。

## 许可证

本项目按 **LGPL-3.0-only** 发布，版权归 GManager 作者所有；许可证全文见 [`LICENSE`](LICENSE)，其中引用的 GPLv3 全文见 [`LICENSE.GPL-3`](LICENSE.GPL-3)。

第三方组件遵循各自许可证：`third_party/ElaWidgetTools` 为 MIT；Qt 6 为 LGPLv3，动态链接，可用同版本自编译库替换；程序图标 `assets/images/gamepad.png` 来自 [Game-icons.net](https://game-icons.net/1x1/delapouite/gamepad.html)，作者 [Delapouite](https://delapouite.com)，按 [CC BY 3.0](http://creativecommons.org/licenses/by/3.0/) 使用，未作修改。

## 说明

截图中的游戏封面、介绍与标签来自刮削结果，仅用于演示界面效果。若分发物不含本 README，请把图标署名一并写入安装包说明或“关于”对话框。
