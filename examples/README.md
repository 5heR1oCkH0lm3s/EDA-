# 示例与参考文件

| 路径 | 用途 |
|---|---|
| [half_adder/half_adder.eda](half_adder/half_adder.eda) | 在 MyEDA 中打开半加器，按 F5 后点击开关仿真 |
| [half_adder/half_adder.net](half_adder/half_adder.net) | 导出的 KiCad 网表 |
| [half_adder/half_adder.kicad_sch](half_adder/half_adder.kicad_sch) | KiCad 原理图 |
| [half_adder/half_adder.kicad_pro](half_adder/half_adder.kicad_pro) | KiCad 工程 |
| [half_adder/half_adder.kicad_pcb](half_adder/half_adder.kicad_pcb) | KiCad 导入后的 PCB 样例（未完成布线） |
| [reference/kicad参考网表-真实导出.net](reference/kicad参考网表-真实导出.net) | KiCad 导出的参考网表，用于格式对照 |

在仓库根目录的 PowerShell 中打开样例：

```powershell
.\MyEDA\build\Debug\MyEDA.exe .\examples\half_adder\half_adder.eda
```

演示截图集中在 `docs/images/screenshots/`，与可编辑的工程样例分开保存。个人练习工程请另外保存，避免覆盖团队样例。

## 子电路封装

支持把完整电路封装为多输入、多输出元件，并通过 `.edasub` 跨工程复用；演示工程见 `examples/subcircuits/`（从仓库根目录查找）。操作见[子电路封装指南](../docs/使用指南/子电路封装指南.md)。
