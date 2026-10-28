# MyEDA — 数字电路原理图编辑器

“工业软件创新训练 I”四人课程项目，使用 C++17、wxWidgets 3.2 和 CMake。

在画布上放置逻辑门、连接引脚，点击开关验证 0/1 逻辑，保存工程或导出文件到 KiCad。

## 快速开始（Windows）

先安装 Visual Studio 2022 的“使用 C++ 的桌面开发”（含 CMake 工具），准备 wxWidgets 3.2 的 x64 静态库。首次配置见[新人上手指南](docs/使用指南/新人上手指南.md)。

| 你想做什么 | 仓库根目录中的入口 |
|---|---|
| 首次编译，或验证自己修改的代码 | 双击 `build.bat`，编译和测试通过后打开 `MyEDA/build/Debug/MyEDA.exe` |
| 更新当前分支并运行 | 先关闭 MyEDA，再双击 `update-and-run.bat` |
| 不更新，只运行已有程序 | 双击 `run.bat` |

更新入口要求工作区干净且当前分支有远程跟踪分支；有本地修改、拉取失败、编译失败或测试失败时停止。它不会自动提交、暂存或覆盖改动。Git 只更新源码，程序仍需编译；入口会自动完成增量编译。

打开示例：程序中选择“文件 → 打开”，选择 `examples/half_adder/half_adder.eda`，按 F5 后点击开关。

## 功能与范围

- 6 种逻辑门、开关、指示灯，共 8 种内置元件；支持用真值表定义双输入、单输出元件。
- 放置、移动、连线、框选、复制粘贴、删除、撤销和重做。
- 0/1 组合逻辑仿真，导线按逻辑值着色，指示灯显示输出。
- `.eda` 工程存档；KiCad 网表导入/导出；KiCad 原理图导出。

支持顶层时钟源和 D 触发器的步进仿真；当前没有模拟电压电流或器件延迟仿真。KiCad 样例展示封装和网络导入，尚未完成 PCB 布线；默认封装用于对接演示，不能直接视为可制造电路。详细限制见[操作手册](docs/使用指南/功能操作手册.md)。

完整分类索引见 [docs/README.md](docs/README.md)，示例说明见 [examples/README.md](examples/README.md)。

## 文档导航

| 目的 | 文档 |
|---|---|
| 安装、运行、排查启动错误 | [新人上手指南](docs/使用指南/新人上手指南.md) |
| 四人分支、提交、更新和合并 | [四人协作指南](docs/开发协作/四人协作指南.md) |
| 菜单、鼠标操作、快捷键与文件格式 | [功能操作手册](docs/使用指南/功能操作手册.md) |
| 理解代码和仿真原理 | [实现讲解](docs/开发协作/实现讲解-新手版.md) |
| 四人分工与期末贡献报告 | [四人分工与贡献说明](docs/课程资料/四人分工与贡献说明.md) |
| 分工和答辩演示 | [分工与答辩手册](docs/课程资料/分工与答辩手册.md) |
| 组织课程报告 | [课程报告写作指南](docs/课程资料/课程报告写作指南.md) |
| 数字电路基础 | [学习笔记](docs/学习笔记/) |
| 一页连续阅读 | [项目完整教程](docs/项目完整教程.md)（自动生成，请勿直接编辑） |

## 目录

```text
仓库根目录/
├── README.md                       项目介绍和入口导航
├── run.bat                         直接运行已编译程序
├── build.bat / build.ps1            编译本地代码并测试
├── update-and-run.bat / .ps1        更新、编译、测试并运行
├── MyEDA/
│   ├── src/                        程序源码和界面资源
│   ├── test/                       逻辑、文件与画布测试
│   ├── build/                      本机生成文件（不提交）
│   └── CMakeLists.txt
├── docs/
│   ├── README.md                   文档总索引
│   ├── 使用指南/                   安装运行、功能操作
│   ├── 开发协作/                   团队协作、实现讲解
│   ├── 课程资料/                   分工答辩、报告指南
│   ├── 学习笔记/                   数字电路基础
│   ├── images/                     截图、速查卡、上手图
│   └── 项目完整教程.md              自动生成的合集
├── examples/
│   ├── half_adder/                 半加器工程及 KiCad 文件
│   └── reference/                  KiCad 参考网表
└── tools/                          文档生成、图片生成和脚本检查
```

## 测试与文档维护

`build.bat` 会编译并运行测试；必须看到测试通过且脚本成功结束。逻辑与文件测试目前包含 64 项检查、子电路及合并兼容测试 53 项，画布测试另有 59 项，后续以实际输出为准。

以下命令在仓库根目录的 **PowerShell** 中执行。三种命令行工具模式：

```powershell
.\MyEDA\build\Debug\test_sim.exe --export-netlist .\examples\half_adder\half_adder.eda .\MyEDA\build\half_adder.net
.\MyEDA\build\Debug\test_sim.exe --export-kicad-sch .\examples\half_adder\half_adder.eda .\MyEDA\build\half_adder.kicad_sch
.\MyEDA\build\Debug\test_sim.exe --parse-netlist .\examples\half_adder\half_adder.net
```

修改 `docs/` 中源文档后，用 Python 3 更新并检查合集：

```powershell
python .\tools\merge_docs.py
python .\tools\merge_docs.py --check
```

Python 只用于维护文档，不是运行 MyEDA 的前置条件。演示图片和速查卡不会随文本自动更新，修改对应功能后需人工复核。

维护构建脚本时，可在成功编译一次后运行 `python .\tools\test_build_failure.py`：它在临时目录模拟测试编译失败，确认脚本不会运行遗留的旧测试。

## 子电路封装

支持把完整电路封装为多输入、多输出元件，并通过 `.edasub` 跨工程复用；演示工程见 `examples/subcircuits/`（从仓库根目录查找）。操作见[子电路封装指南](docs/使用指南/子电路封装指南.md)。

时序元件合并说明：支持顶层 D 触发器和时钟源，每 500ms 自动步进；D 触发器在上升沿采样。子电路封装目前用于组合逻辑，包含时钟或触发器的选区会提示暂不支持。
