# Update the current tracking branch, then use the shared build entry point.
param([string]$Wx = '')
$ErrorActionPreference = 'Stop'
try {
    Set-Location -LiteralPath $PSScriptRoot
    $git = (Get-Command git -ErrorAction Stop).Source
    $exe = Join-Path $PSScriptRoot 'MyEDA\build\Debug\MyEDA.exe'
    if (Get-Process MyEDA -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe }) {
        throw '请先保存电路并关闭本项目的 MyEDA，再更新。'
    }
    Write-Host '[1/3] 检查并更新当前分支 ...' -ForegroundColor Cyan
    $changes = & $git status --porcelain --untracked-files=normal
    if ($LASTEXITCODE -ne 0) { throw '无法读取 Git 状态，请确认这是 Git 克隆的项目。' }
    if ($changes) {
        $changes | ForEach-Object { Write-Host $_ }
        throw '存在本地修改或未跟踪文件。请先妥善保存并提交，再更新。只验证本地代码可用 build.bat。'
    }
    $branch = & $git symbolic-ref --quiet --short HEAD
    if ($LASTEXITCODE -ne 0) { throw '当前处于 detached HEAD，请先切换到开发分支。' }
    $null = & $git rev-parse --abbrev-ref --symbolic-full-name '@{u}'
    if ($LASTEXITCODE -ne 0) { throw '当前分支没有远程跟踪分支。首次推送请参照 docs/开发协作/四人协作指南.md。' }
    Write-Host "更新分支：$branch"
    & $git pull --ff-only
    if ($LASTEXITCODE -ne 0) { throw '拉取失败：请检查网络或分支分叉提示。未开始编译，也不会自动合并或覆盖代码。' }
    Write-Host '[2/3] 编译并测试更新后的代码 ...' -ForegroundColor Cyan
    $buildScript = Join-Path $PSScriptRoot 'build.ps1'
    $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $buildScript)
    if ($Wx) { $arguments += @('-Wx', $Wx) }
    & powershell.exe @arguments
    if ($LASTEXITCODE -ne 0) { throw '代码已拉取，但编译或测试失败，未启动程序。请查看上方错误。' }
    Write-Host '[3/3] 启动 MyEDA ...' -ForegroundColor Cyan
    $app = Start-Process -FilePath $exe -WorkingDirectory (Split-Path -Parent $exe) -PassThru
    $null = $app.WaitForInputIdle(15000)
    $app.Refresh()
    if ($app.HasExited) { throw 'MyEDA 启动后退出，请检查程序错误。' }
    Write-Host '更新、编译和测试完成，已启动 MyEDA。' -ForegroundColor Green
    exit 0
} catch {
    Write-Host "操作停止：$($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
