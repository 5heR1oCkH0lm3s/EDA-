$ErrorActionPreference = 'Stop'
# 生成四张"答辩速查卡"(每人一张,PNG),方便打印带上台
# 用法: powershell -NoProfile -ExecutionPolicy Bypass -File tools\make_role_cards.ps1
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $repoRoot
Add-Type -AssemblyName System.Drawing

$navy  = [System.Drawing.Color]::FromArgb(31, 58, 95)
$ink   = [System.Drawing.Color]::FromArgb(45, 45, 45)
$grey  = [System.Drawing.Color]::FromArgb(110, 110, 110)
$line  = [System.Drawing.Color]::FromArgb(205, 213, 224)
$soft  = [System.Drawing.Color]::FromArgb(246, 248, 251)
$code  = [System.Drawing.Color]::FromArgb(240, 243, 247)
$green = [System.Drawing.Color]::FromArgb(232, 246, 234)
$greenLine = [System.Drawing.Color]::FromArgb(120, 180, 130)
$white = [System.Drawing.Color]::White

$cards = @(
  @{ tag = 'A'; label = 'A界面'; name = '界面'; mod = '主窗口与所有对话框';
     resp = @(
        '主窗口三栏布局:元件库树 + 绘图区 + 属性表',
        '菜单栏与工具栏(25 个菜单项 / 10 个工具按钮)',
        '属性表随选中、拖动、撤销、粘贴实时刷新',
        '自定义元件的"定义"和"管理"两个对话框',
        '未保存修改提醒(标题星号 + 关闭前询问)');
     code = @(
        'src/ui/MyFrame.cpp : CreateClientArea()   三栏布局',
        'src/ui/MyFrame.cpp : RefreshPropertyTable()   属性表刷新',
        'src/ui/MyFrame.cpp : ConfirmDiscardChanges()  未保存提醒',
        'src/ui/MyFrame.cpp : canvas->SetSelectionCallback(...)  注册回调',
        'src/ui/CustomDialog.h : CustomGateDialog / CustomManagerDialog');
     qa = @(
        @('五个界面元素是什么?', '菜单、工具栏、元件库树、绘图区、属性表。'),
        @('属性表怎么知道该刷新?', '画布内容一变化就回调通知主窗口,不是轮询;画布不需要知道主窗口是谁。'),
        @('为什么用 sizer 不用写死坐标?', '窗口拉伸时布局自动适配:比例 0 保持固定宽,比例 1 吃掉剩余空间。'),
        @('关闭时怎么提醒存盘?', '画布有 dirty 标记,有改动时标题加星号;关闭/新建/打开前弹"保存/不保存/取消"。'));
     demo = @('① 点画布上一个元件 → 右侧属性表立刻显示它的属性',
              '② 点"选择"框选两个元件 → 属性表显示"已选中 2 个元件"',
              '③ 关闭窗口 → 弹出"有未保存的修改,要先保存吗?"');
     practice = '在"帮助"菜单加一项"使用说明",点开弹出 wxMessageBox' },

  @{ tag = 'B'; label = 'B编辑'; name = '数据模型与绘图编辑'; mod = '绘图区交互';
     resp = @(
        '数据模型:元件 Component / 连线 Wire / 引脚坐标',
        '放置、拖动(支持整组)、引脚连线、删除(级联删线)',
        '框选与多选(Shift/Ctrl 加选)、全选',
        '撤销 / 重做(快照式)',
        '元件符号绘制(与门 D 形、或门折线、非门三角…)');
     code = @(
        'src/core/Component.h : struct Component / Wire / GetPinPos()',
        'src/editor/DrawingCanvas.cpp : OnPaint()   总体绘制',
        'src/editor/DrawingCanvas.cpp : DrawGate()   各种门符号',
        'src/editor/DrawingCanvas.cpp : HitTest() / HitPin()   命中测试',
        'src/editor/DrawingCanvas.cpp : PushUndo() / Undo()   快照撤销');
     qa = @(
        @('为什么移动元件时连线跟着走?', '连线存的是"元件编号+引脚号",不是坐标;画的时候才用 GetPinPos 现算位置。'),
        @('怎么判断鼠标点到了哪个元件?', 'HitTest 逐个比较是否落在包围盒里,从后往前找(后画的在上层)。'),
        @('拖动是怎么实现的?', @('按下时记下鼠标相对元件中心的偏移,移动时用"鼠标位置 − 偏移"反算中心;',
                                   '多选时每个元件各记一份偏移,所以能整组平移。')),
        @('撤销为什么不用"反操作"?', @('改动前把电路存成 JSON 文本压栈,撤销就是读回;',
                                      '一份代码覆盖放置/删除/拖动/连线/粘贴等所有操作。')));
     demo = @('① 点树里"与门"→ 画布点两下,放出两个门',
              '② 拖动其中一个 → 移动;点"连线"→ 点输出引脚再点输入引脚',
              '③ 选中按 Delete(连线一起消失)→ Ctrl+Z 恢复');
     practice = '把画布网格间距从 25 改成 40(DrawingCanvas::OnPaint 里两处)' },

  @{ tag = 'C'; label = 'C文件网表'; name = '文件与网表'; mod = '存档 · 网表 · 与 KiCad 对接';
     resp = @(
        '.eda 工程存档(JSON):元件 + 连线 + 自定义元件库',
        '网表导出/导入(KiCad S 表达式格式)',
        '网络归并:把"一根根连线"合并成"电气网络"(处理扇出)',
        '默认封装:让元件能进 PCB',
        'KiCad 原理图导出(.kicad_sch)+ 在 KiCad 里端到端验证');
     code = @(
        'src/file/CircuitFile.h : CircuitToJson() / JsonToCircuit()',
        'src/file/NetlistExport.h : BuildNets()   并查集归并网络',
        'src/file/NetlistExport.h : ExportKiCadNetlist() / ParseKiCadNetlist()',
        'src/file/NetlistExport.h : DefaultFootprint()   默认封装',
        'src/file/KicadSchExport.h : ExportKicadSchematic()');
     qa = @(
        @('工程文件里有什么?', @('三段:components(元件)、wires(连线)、customGates(自定义元件库);',
                                '写严格读宽容 —— 新增字段用可选解析,老文件也能打开。')),
        @('为什么"一根连线不等于一个网络"?', @('一个输出常接多个输入(扇出),电气上属于同一个网络;用并查集归并,',
                                             '否则同一引脚会进两个网络,PCB 软件会当短路/冲突报错。')),
        @('怎么保证 KiCad 能认我们的网表?', '让 KiCad 自己导出一份参考网表,逐字段对比;实测导入错误 0,并生成 PCB。'),
        @('封装是什么?为什么必须写?', @('封装是 PCB 上的实际焊盘形状;导入时读的是紧跟 (value) 的',
                                      '(footprint) 元素,不写会报"没有分配封装"。')));
     demo = @('① 文件→导出网表,用记事本看结构',
              '② 展示样例四件套:.eda / .net / .kicad_sch / .kicad_pcb',
              '③ (选做)KiCad 里文件→导入→网表,点"加载并测试"看到错误 0');
     practice = '把门的默认封装改成 DIP-8,重新导出网表看变化' },

  @{ tag = 'D'; label = 'D仿真验证'; name = '仿真与验证'; mod = '仿真引擎 · 测试 · 端到端验收';
     resp = @(
        '仿真引擎:迭代传播算法(不碰界面的纯函数)',
        '自定义元件的真值表求值',
        '64 项单元测试 + 3 个命令行工具',
        '半加器验收(四种输入组合)',
        'KiCad 端到端验证方法(导入 0 错误)');
     code = @(
        'src/sim/Simulation.h : SimulateCircuit()   迭代传播',
        'src/sim/Simulation.h : FindSource()   沿线找信号来源',
        '真值表位序:第 i 位 ↔ 输入组合 i,i = A*2+B(与门=8、与非门=7)',
        'test/test_sim.cpp : 64 项检查(逻辑/存档往返/网表往返/互操作)',
        '命令行:--export-netlist / --export-kicad-sch / --parse-netlist');
     qa = @(
        @('仿真怎么算的?', @('初始化(开关=自身状态、门=0),然后重复"元件数+2"轮,',
                            '每轮让每个门读输入算输出,信号必定传到最远的灯。')),
        @('自定义元件为什么 4 个勾选框就够?', '2 输入共 4 种组合,每种输出 0/1 定完行为就唯一;仿真时按输入组合查表。'),
        @('你们怎么测试的?', @('三层:单元测试 64 项(不开界面)、GUI 自动化(比对保存出来的文件)、',
                              '端到端验收(半加器真值表 + KiCad 导入生成 PCB)。')),
        @('单元测试抓到过什么真 bug?', @('存盘函数漏了自定义元件分支,自定义元件存盘后会变成 LED;',
                                      '纯函数测试 1 秒就抓到了。')));
     demo = @('① 打开半加器 → 点"仿真"→ 点开关,报真值表(和灯/进位灯)',
              '② 命令行跑 test_sim.exe → 展示 45 行 PASS 与 ALL PASS',
              '③ 展示 KiCad 导入 0 错误截图与生成的 PCB');
     practice = '给 test_sim.cpp 加一条非门测试用例,跑出新的 PASS' }
)

function FillRect($g, $x, $y, $w, $h, $color) {
    $b = New-Object System.Drawing.SolidBrush($color); $g.FillRectangle($b, $x, $y, $w, $h); $b.Dispose()
}
function StrokeRect($g, $x, $y, $w, $h, $color, $width) {
    $p = New-Object System.Drawing.Pen($color, $width); $g.DrawRectangle($p, $x, $y, $w, $h); $p.Dispose()
}
function WarnIfTooLong($g, $s, $size, $maxW, $where) {
    $f = New-Object System.Drawing.Font('Microsoft YaHei', $size)
    $w = $g.MeasureString($s, $f).Width
    $f.Dispose()
    if ($w -gt $maxW) {
        $head = $s; if ($head.Length -gt 22) { $head = $head.Substring(0, 22) + '...' }
        Write-Host ("  ! 文字过长(" + [int]$w + " > " + $maxW + ") " + $where + ":" + $head) -ForegroundColor Yellow
    }
}
function Txt($g, $s, $x, $y, $size, $color, $bold, $family) {
    if (-not $family) { $family = 'Microsoft YaHei' }
    $st = [System.Drawing.FontStyle]::Regular; if ($bold) { $st = [System.Drawing.FontStyle]::Bold }
    $f = New-Object System.Drawing.Font($family, $size, $st)
    $b = New-Object System.Drawing.SolidBrush($color)
    $g.DrawString($s, $f, $b, [float]$x, [float]$y); $f.Dispose(); $b.Dispose()
}

$W = 1400; $H = 2060
foreach ($c in $cards) {
    $bmp = New-Object System.Drawing.Bitmap($W, $H)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'; $g.TextRenderingHint = 'ClearTypeGridFit'
    $g.Clear([System.Drawing.Color]::White)

    # 标题栏
    FillRect $g 0 0 $W 200 $navy
    WarnIfTooLong $g ("MyEDA 答辩速查卡 · " + $c.tag + " " + $c.name) 44 1250 '标题'
    Txt $g ("MyEDA 答辩速查卡 · " + $c.tag + " " + $c.name) 70 52 44 $white $true $null
    Txt $g ("我负责:" + $c.mod) 74 136 26 ([System.Drawing.Color]::FromArgb(200, 216, 236)) $false $null

    # 模块一:我负责什么
    $y = 230
    FillRect $g 60 $y 1280 360 $soft; StrokeRect $g 60 $y 1280 360 $line 1
    Txt $g '我负责什么' 100 ($y + 22) 32 $navy $true $null
    $ly = $y + 84
    foreach ($r in $c.resp) { WarnIfTooLong $g $r 23 1180 '我负责什么'; Txt $g ('· ' + $r) 108 $ly 23 $ink $false $null; $ly += 52 }

    # 模块二:关键代码
    $y = 620
    FillRect $g 60 $y 1280 380 $soft; StrokeRect $g 60 $y 1280 380 $line 1
    Txt $g '我的关键代码在哪' 100 ($y + 22) 32 $navy $true $null
    $ly = $y + 84
    foreach ($r in $c.code) {
        FillRect $g 96 ($ly - 4) 1208 40 $code
        WarnIfTooLong $g $r 19 1180 '关键代码'; Txt $g $r 112 ($ly + 2) 19 $navy $false 'Consolas'
        $ly += 56
    }

    # 模块三:问答
    $y = 1030
    FillRect $g 60 $y 1280 700 $soft; StrokeRect $g 60 $y 1280 700 $line 1
    Txt $g '被问到时怎么答' 100 ($y + 22) 32 $navy $true $null
    $ly = $y + 82
    foreach ($pair in $c.qa) {
        WarnIfTooLong $g $pair[0] 23 1180 '问答-问题'; Txt $g ('Q  ' + $pair[0]) 108 $ly 23 $navy $true $null
        $ans = $pair[1]
        if ($ans -is [array]) {
            $ay = $ly + 38
            foreach ($aline in $ans) {
                WarnIfTooLong $g $aline 21 1120 '问答-答案'; if ($aline -eq $ans[0]) { Txt $g ('A  ' + $aline) 136 $ay 21 $ink $false $null }
                else                    { Txt $g ('    ' + $aline) 136 $ay 21 $ink $false $null }
                $ay += 32
            }
            $ly += 156
        } else {
            WarnIfTooLong $g $ans 21 1120 '问答-答案'; Txt $g ('A  ' + $ans) 136 ($ly + 38) 21 $ink $false $null
            $ly += 156
        }
    }

    # 模块四:我演示哪一步
    $y = 1760
    FillRect $g 60 $y 1280 160 $green; StrokeRect $g 60 $y 1280 160 $greenLine 2
    Txt $g '我演示哪一步' 100 ($y + 18) 30 ([System.Drawing.Color]::FromArgb(35, 110, 55)) $true $null
    $ly = $y + 66
    foreach ($d in $c.demo) {
        WarnIfTooLong $g $d 21 1180 '演示步骤'; Txt $g $d 108 $ly 21 ([System.Drawing.Color]::FromArgb(35, 110, 55)) $false $null
        $ly += 32
    }

    # 模块五:上手练习
    $y = 1950
    WarnIfTooLong $g ('我的 30 分钟上手练习:' + $c.practice) 22 1280 '练习'; Txt $g ('我的 30 分钟上手练习:' + $c.practice) 70 $y 22 $grey $false $null

    $path = Join-Path $repoRoot ("docs/images/role-cards/速查卡-" + $c.label + ".png")
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "已生成 $path"
}
