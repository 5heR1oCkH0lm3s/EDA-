$ErrorActionPreference = 'Stop'
# 生成"新人上手"一页图(PNG),方便发群或贴进报告
# 用法: powershell -NoProfile -ExecutionPolicy Bypass -File tools\make_onboarding_card.ps1
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $repoRoot
Add-Type -AssemblyName System.Drawing

$W = 1400; $H = 2000
$bmp = New-Object System.Drawing.Bitmap($W, $H)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = 'AntiAlias'
$g.TextRenderingHint = 'ClearTypeGridFit'
$g.Clear([System.Drawing.Color]::White)

$navy  = [System.Drawing.Color]::FromArgb(31, 58, 95)
$ink   = [System.Drawing.Color]::FromArgb(45, 45, 45)
$grey  = [System.Drawing.Color]::FromArgb(110, 110, 110)
$line  = [System.Drawing.Color]::FromArgb(205, 213, 224)
$soft  = [System.Drawing.Color]::FromArgb(246, 248, 251)
$code  = [System.Drawing.Color]::FromArgb(240, 243, 247)
$green = [System.Drawing.Color]::FromArgb(232, 246, 234)
$greenLine = [System.Drawing.Color]::FromArgb(120, 180, 130)
$white = [System.Drawing.Color]::White

function FillRect($x, $y, $w, $h, $color) {
    $b = New-Object System.Drawing.SolidBrush($color)
    $g.FillRectangle($b, $x, $y, $w, $h); $b.Dispose()
}
function StrokeRect($x, $y, $w, $h, $color, $width = 2) {
    $p = New-Object System.Drawing.Pen($color, $width)
    $g.DrawRectangle($p, $x, $y, $w, $h); $p.Dispose()
}
function Txt($s, $x, $y, $size, $color, $bold = $false, $family = 'Microsoft YaHei') {
    $style = if ($bold) { [System.Drawing.FontStyle]::Bold } else { [System.Drawing.FontStyle]::Regular }
    $f = New-Object System.Drawing.Font($family, $size, $style)
    $b = New-Object System.Drawing.SolidBrush($color)
    $g.DrawString($s, $f, $b, [float]$x, [float]$y)
    $f.Dispose(); $b.Dispose()
}
function Mono($s, $x, $y, $size, $color = $null) {
    if (-not $color) { $color = $script:navy }
    Txt $s $x $y $size $color $false 'Consolas'
}
function CodeBox($s, $x, $y, $w, $h, $size = 24) {
    FillRect $x $y $w $h $script:code
    StrokeRect $x $y $w $h $script:line 1
    Mono $s ($x + 22) ($y + ($h - $size * 1.6) / 2) $size
}

# ---------- 顶部标题 ----------
FillRect 0 0 $W 230 $navy
Txt 'MyEDA 项目 · 新人上手' 80 58 50 $white $true
Txt '三步把项目跑起来 —— 不用敲任何编译命令' 84 150 26 ([System.Drawing.Color]::FromArgb(200, 216, 236))

# ---------- ① 同步代码 ----------
$y = 280
FillRect 70 $y 1260 440 $soft
StrokeRect 70 $y 1260 440 $line 1
Txt '① 同步代码(把最新代码拿到自己电脑)' 110 ($y + 26) 32 $navy $true
Txt '情况 A:之前克隆过 —— 进入项目文件夹,右键 → Open Git Bash here,输入:' 110 ($y + 92) 23 $ink
CodeBox 'git pull' 130 ($y + 130) 1160 62 26
Txt '情况 B:第一次拿项目 —— 同样打开 Git Bash,克隆一份(国内用镜像地址):' 110 ($y + 214) 23 $ink
CodeBox 'git clone https://ghfast.top/https://github.com/Pengyang-uestc/EDA-.git' 130 ($y + 252) 1160 62 19
Txt '克隆完会多出一个 EDA- 文件夹,里面就是整个项目。' 110 ($y + 340) 22 $grey
Txt '提示:Git 没装的话,先用课程《基础工具软件搭建指南》装好,再回来做这一步。' 110 ($y + 376) 22 $grey

# ---------- ② 编译 ----------
$y = 750
FillRect 70 $y 1260 230 $soft
StrokeRect 70 $y 1260 230 $line 1
Txt '② 双击 build.bat(就在项目文件夹里)' 110 ($y + 26) 32 $navy $true
Txt '它会自动:找 wxWidgets  →  生成 Visual Studio 工程  →  编译  →  跑单元测试' 110 ($y + 92) 23 $ink
Txt '第一次大约 1~2 分钟。以后改了代码,再双击一次就行(不用删任何目录)。' 110 ($y + 134) 23 $ink
Txt '看不到这个文件? 说明还没执行第 ① 步,代码是旧的。' 110 ($y + 176) 22 $grey

# ---------- ③ 运行 ----------
$y = 1010
FillRect 70 $y 1260 200 $soft
StrokeRect 70 $y 1260 200 $line 1
Txt '③ 双击运行程序' 110 ($y + 26) 32 $navy $true
Txt '编译成功后,可执行文件在这里(直接双击运行):' 110 ($y + 88) 23 $ink
CodeBox 'MyEDA\build\Debug\MyEDA.exe' 130 ($y + 124) 1160 56 22

# ---------- 成功的标志 ----------
$y = 1240
FillRect 70 $y 1260 250 $green
StrokeRect 70 $y 1260 250 $greenLine 2
Txt '成功的标志:黑窗口最后会打印这些(看到"全部完成"就对了)' 110 ($y + 24) 30 ([System.Drawing.Color]::FromArgb(35, 110, 55)) $true
Mono '=== ALL PASS ===' 150 ($y + 92) 26 ([System.Drawing.Color]::FromArgb(35, 110, 55))
Mono '全部完成!程序在这里:' 150 ($y + 136) 26 ([System.Drawing.Color]::FromArgb(35, 110, 55))
Mono '  ...\MyEDA\build\Debug\MyEDA.exe' 150 ($y + 180) 26 ([System.Drawing.Color]::FromArgb(35, 110, 55))

# ---------- 常见问题(每条两行:现象 + 怎么办,避免一行太长被截断) ----------
$y = 1520
FillRect 70 $y 1260 420 $soft
StrokeRect 70 $y 1260 420 $line 1
Txt '常见问题' 110 ($y + 24) 32 $navy $true
$faq = @(
    @('提示"找不到 wxWidgets"', '用参数指定位置:  .\build.ps1 -Wx "D:\你的路径\wxWidgets-3.2.2.1"'),
    @('git 连不上、clone 卡住', '用 ① 里的镜像地址克隆;有梯子的话换成 github.com 原地址'),
    @('编译报一堆红色 error', '先试清理重建:  .\build.ps1 -Clean   ;还不行把前几条报错截图发群里'),
    @('双击 build.bat 一闪就没了', '在文件夹里右键 Open Git Bash here,输入 ./build.bat 运行即可看到报错')
)
$qy = $y + 82
foreach ($item in $faq) {
    Txt '·' 118 $qy 23 $navy $true
    Txt $item[0] 145 $qy 22 $ink $true
    Txt $item[1] 172 ($qy + 38) 21 $grey
    $qy += 84
}

# ---------- 页脚 ----------
Txt '详细说明见 docs/使用指南/新人上手指南.md   ·   遇到问题先截图发群里,不要自己乱试' 80 1950 22 $grey

$bmp.Save((Join-Path $repoRoot 'docs/images/onboarding/新人上手-一页图.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Host '已生成 docs/images/onboarding/新人上手-一页图.png'
