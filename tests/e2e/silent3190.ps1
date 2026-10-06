# v3.19.0 安装包静默安装验证：装 -> 校验版本/文件 -> 启动 -> 卸载 -> 校验清理干净
$ErrorActionPreference = 'Stop'
$out = 'D:\workspace\GuoDesk\artifacts\silent3190.txt'
$log = New-Object System.Collections.Generic.List[string]
function Say([string]$s) { Write-Output $s; $log.Add($s) | Out-Null }

$setup = 'D:\workspace\GuoDesk\artifacts\installer\GuoDesk-3.19.0-setup.exe'
$dir = Join-Path $env:LOCALAPPDATA 'Programs\GuoDesk'
$exe = Join-Path $dir 'GuoDesk.exe'
$uninst = Join-Path $dir 'unins000.exe'

Get-Process GuoDesk -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

$p = Start-Process -FilePath $setup -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART' -Wait -PassThru
Say ('install exit=' + $p.ExitCode)
Say ('exe exists=' + (Test-Path $exe))
if (Test-Path $exe) {
  $vi = (Get-Item $exe).VersionInfo
  Say ('product version=' + $vi.ProductVersion + ' file version=' + $vi.FileVersion)
  Say ('launch=' + (Test-Path (Join-Path $dir 'Microsoft.UI.Xaml') -PathType Container) + ' dlls=' + @(Get-ChildItem $dir -Filter '*.dll').Count)
  Start-Process -FilePath $exe
  Start-Sleep -Seconds 5
  $proc = @(Get-Process GuoDesk -ErrorAction SilentlyContinue)
  Say ('running processes=' + $proc.Count)
  Say ('windows=' + ((Get-Process GuoDesk -ErrorAction SilentlyContinue | ForEach-Object { $_.MainWindowTitle }) -join ';'))
  Get-Process GuoDesk -ErrorAction SilentlyContinue | Stop-Process -Force
  Start-Sleep -Seconds 1
}
if (Test-Path $uninst) {
  $u = Start-Process -FilePath $uninst -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART' -Wait -PassThru
  Say ('uninstall exit=' + $u.ExitCode)
}
Start-Sleep -Seconds 2
Say ('dir remains=' + (Test-Path $dir))
if (Test-Path $dir) { Say ('leftover=' + ((Get-ChildItem $dir | ForEach-Object { $_.Name }) -join ',')) }

# 重新安装并启动，恢复用户的常驻实例
$p2 = Start-Process -FilePath $setup -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART' -Wait -PassThru
Say ('reinstall exit=' + $p2.ExitCode + ' exe=' + (Test-Path $exe))
Start-Process -FilePath $exe
Start-Sleep -Seconds 4
Say ('restored processes=' + @(Get-Process GuoDesk -ErrorAction SilentlyContinue).Count)
[System.IO.File]::WriteAllLines($out, $log, (New-Object System.Text.UTF8Encoding($false)))
