# 维护工具

| 文件 | 用途 | 输出 |
|---|---|---|
| `merge_docs.py` | 合并六份分篇教程，`--check` 只检查同步 | `docs/项目完整教程.md` |
| `make_onboarding_card.ps1` | 生成新人上手图 | `docs/images/onboarding/` |
| `make_role_cards.ps1` | 生成四人答辩速查卡 | `docs/images/role-cards/` |
| `test_build_failure.py` | 检查测试编译失败后不会运行旧测试程序 | 临时目录，不改项目源码 |

Python 工具需要 Python 3；图片生成工具需要 Windows PowerShell 和 System.Drawing。请先核对生成脚本内容是否符合当前功能与目录，再更新配图。日常运行软件使用根目录的 `run.bat`；编译与更新使用根目录的对应入口。
