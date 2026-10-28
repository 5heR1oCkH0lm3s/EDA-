# Windows PowerShell 5.1 / PowerShell 7; UTF-8 with BOM.
param([string]$Wx = '', [switch]$Clean)
$ErrorActionPreference = 'Stop'
try {
    $root = $PSScriptRoot
    $buildDir = Join-Path $root 'MyEDA\build'
    $exe = Join-Path $buildDir 'Debug\MyEDA.exe'
    if (Get-Process MyEDA -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe }) {
        throw '请先保存电路并关闭本项目的 MyEDA，再编译。'
    }
    $command = Get-Command cmake -ErrorAction SilentlyContinue
    $cmake = if ($command) { $command.Source } else { $null }
    if (-not $cmake) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (Test-Path -LiteralPath $vswhere) {
            $cmake = & $vswhere -latest -products '*' -find 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' | Select-Object -First 1
        }
    }
    if (-not $cmake) { throw '找不到 CMake。请用 Visual Studio Installer 安装 C++ CMake 工具，或将 CMake 加入 PATH。' }
    if (-not $Wx) { $Wx = $env:WXWIN }
    if (-not $Wx) {
        $candidates = @(
            (Join-Path $root '..\基础平台搭建\基础平台搭建\基础工具软件-程序和例子\wxWidgets-3.2.2.1'),
            (Join-Path $root '..\..\基础平台搭建\基础平台搭建\基础工具软件-程序和例子\wxWidgets-3.2.2.1'),
            'C:\wxWidgets-3.2.2.1',
            'D:\EDA工业软件\基础平台搭建\基础平台搭建\基础工具软件-程序和例子\wxWidgets-3.2.2.1'
        )
        $Wx = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    }
    if (-not $Wx -or -not (Test-Path -LiteralPath (Join-Path $Wx 'lib\vc_x64_lib'))) {
        throw '找不到 wxWidgets x64 静态库。请设置 WXWIN，或用 build.bat -Wx "你的 wxWidgets 目录" 指定。'
    }
    Write-Host "wxWidgets: $Wx"
    Write-Host '[1/3] 配置工程 ...' -ForegroundColor Cyan
    & $cmake -S (Join-Path $root 'MyEDA') -B $buildDir -G 'Visual Studio 17 2022' -A x64 "-DwxWidgets_ROOT_DIR=$Wx" "-DwxWidgets_LIB_DIR=$Wx/lib/vc_x64_lib"
    if ($LASTEXITCODE -ne 0) { throw 'CMake 配置失败，请查看上方错误。' }
    Write-Host '[2/3] 编译程序 ...' -ForegroundColor Cyan
    $buildArgs = @('--build', $buildDir, '--config', 'Debug', '--target', 'MyEDA')
    if ($Clean) { $buildArgs += '--clean-first' }
    & $cmake @buildArgs
    if ($LASTEXITCODE -ne 0) { throw '程序编译失败，请查看上方第一条错误。' }
    Write-Host '[3/3] 编译并运行测试 ...' -ForegroundColor Cyan
    & $cmake --build $buildDir --config Debug --target test_sim test_modules
    if ($LASTEXITCODE -ne 0) { throw '测试程序编译失败；未运行旧测试程序。' }
    & (Join-Path $buildDir 'Debug\test_sim.exe')
    if ($LASTEXITCODE -ne 0) { throw '测试未通过，请查看 FAIL 项；本次构建不算成功。' }
    Write-Host "编译和测试全部通过。程序：$exe" -ForegroundColor Green
    exit 0
} catch {
    Write-Host "操作停止：$($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
