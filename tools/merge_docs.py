# 把 docs/ 下六份教程合并成一个《docs/项目完整教程.md》
# 目的:在 GitHub 网页上不用翻文件夹,一页连续读完;开头带可点击的目录
# 用法: python tools/merge_docs.py   (源文档更新后重跑一次即可,合并结果会重新生成)
# 注意: 源文档仍是"唯一的真相",本脚本只做拼接,不修改它们
import io
import re
import argparse
from pathlib import Path

BASE = Path(__file__).resolve().parents[1]
ROOT = "docs"
OUT = "docs/项目完整教程.md"

PARTS = [
    ("第一部分 新人上手(把项目跑起来)",            "docs/使用指南/新人上手指南.md"),
    ("第二部分 功能操作手册(菜单/按钮/文件格式)",  "docs/使用指南/功能操作手册.md"),
    ("第三部分 分工与答辩手册(分工/讲稿/演示/20问)", "docs/课程资料/分工与答辩手册.md"),
    ("第四部分 实现讲解(每个功能为什么这么写)",     "docs/开发协作/实现讲解-新手版.md"),
    ("第五部分 课程报告写作指南(怎么组织成报告)",   "docs/课程资料/课程报告写作指南.md"),
    ("第六部分 四人协作指南", "docs/开发协作/四人协作指南.md"),
]

def read(path):
    return (BASE / path).read_text(encoding="utf-8-sig")

def strip_h1(text):
    """去掉源文档开头的 # 大标题(合并后用"第X部分"标题代替),其余原样保留"""
    lines = text.split("\n")
    out = []
    removed = False
    for ln in lines:
        if not removed and ln.startswith("# "):
            removed = True
            continue
        out.append(ln)
    # 去掉开头多余空行
    while out and out[0].strip() == "":
        out.pop(0)
    return "\n".join(out)

def gh_anchor(title):
    """按 GitHub 的规则把标题转成锚点:小写、去标点(保留中文/字母/数字/连字符)、空格变连字符"""
    t = title.strip().lower()
    t = re.sub(r"[^\w\u4e00-\u9fff\- ]", "", t)
    t = t.replace(" ", "-")
    return t

out = io.StringIO()
w = out.write

w("# MyEDA 项目完整教程(合集版)\n\n")
w("> 期末分工与贡献填写见 [四人分工与贡献说明](课程资料/四人分工与贡献说明.md)。\n\n")
w("> **自动生成，请勿直接编辑。** 请修改 `docs/` 下源文档，再运行 `python tools/merge_docs.py`；用 `--check` 检查是否同步。\n\n")
w("> 本文件把仓库里分散的六份教程**合并成一页**,在网页上可以从上往下连续读,不用来回点文件。\n")
w("> 每部分开头有说明;想分文件细看,`docs/` 下的原始文档内容与此完全一致。\n")
w("> 配图(截图/速查卡/一页图)在 `docs/` 与 `docs/images/screenshots/` 里,文中提到时会标注文件名。\n\n")

# ---------- 目录 ----------
w("## 目录\n\n")
for title, path in PARTS:
    w(f"- [{title}](#{gh_anchor(title)})\n")
w("- [附录 网表与原理图样例(可直接对照看)](#附录-样例文件在哪)\n")
w("\n---\n\n")

# ---------- 各部分 ----------
for title, path in PARTS:
    w(f"# {title}\n\n")
    w(strip_h1(read(path)).rstrip() + "\n\n")
    w("---\n\n")

# ---------- 附录:样例文件在哪 ----------
w("# 附录 样例文件在哪\n\n")
w("| 文件 | 是什么 |\n|---|---|\n")
w("| `examples/half_adder/half_adder.eda` | 我们的工程文件(可打开继续编辑) |\n")
w("| `examples/half_adder/half_adder.net` | 导出的 KiCad 网表(可被 PCB 软件导入) |\n")
w("| `examples/half_adder/half_adder.kicad_sch` | 导出的 KiCad 原理图(可打开查看) |\n")
w("| `examples/half_adder/half_adder.kicad_pcb` | KiCad 导入网表后生成的 PCB |\n")
w("| `docs/images/screenshots/*.png` | 关键验证截图(仿真真值表/框选/KiCad 导入 0 错误/渲染电路) |\n")
w("| `docs/images/role-cards/速查卡-*.png` | 四张答辩速查卡(A界面/B编辑/C文件网表/D仿真验证) |\n")
w("| `docs/images/onboarding/新人上手-一页图.png` | 上手三步的一页图 |\n\n")
w("> 本合集由 `tools/merge_docs.py` 从六份源文档生成;源文档更新后重跑一次即可。\n")

result = out.getvalue()
parser = argparse.ArgumentParser(description="生成或检查教程合集")
parser.add_argument("--check", action="store_true", help="只检查合集是否与源文档一致")
args = parser.parse_args()
target = BASE / OUT
if args.check:
    if not target.exists() or target.read_text(encoding="utf-8") != result:
        raise SystemExit("合集未同步，请运行 python tools/merge_docs.py 并提交生成结果。")
    print("教程合集与源文档一致。")
    raise SystemExit(0)
with target.open("w", encoding="utf-8", newline="\n") as output:
    output.write(result)

n_lines = result.count("\n")
n_parts = result.count("\n# 第")
print(f"已生成 {OUT}: {n_lines} 行, {n_parts} 个部分")
