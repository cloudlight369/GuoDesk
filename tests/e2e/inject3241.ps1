# v3.24.1 E2E：定位失败不许改坏分区设置，大写链接方案要认，超长分区名不能撑爆托盘气泡
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Windows.Forms
$AE = [System.Windows.Automation.AutomationElement]
$TS = [System.Windows.Automation.TreeScope]
$AC = [System.Windows.Automation.Condition]
$CT = [System.Windows.Automation.ControlType]
$root = 'D:\workspace\GuoDesk\artifacts'
$run = Join-Path $root 'e2e3241'
$data = Join-Path $run 'data'
$dl = Join-Path $run 'src_a'
$dlB = Join-Path $run 'src_b'
$exe = Join-Path $root 'Release\GuoDesk.exe'
$report = Join-Path $root 'e2e3241.txt'
$nameA = '定位守卫'
$nameB = '锁定折叠'
$titleZone = 'GuoDesk · 定位守卫'
$titleLock = 'GuoDesk · 锁定折叠'
if (Test-Path $run) { Remove-Item -Recurse -Force $run }
New-Item -ItemType Directory -Force -Path $data, $dl, $dlB | Out-Null
$noBom = New-Object System.Text.UTF8Encoding($false)
$esc = ($dl -replace '\\', '\\')
$escB = ($dlB -replace '\\', '\\')
$layout = '{"version":1,"zones":[{"id":"za","name":"定位守卫","x":180,"y":120,"width":460,"height":380,"collapsed":false,"viewMode":"grid","tileSize":1,"nameLines":2,"mappedFolder":"' + $esc + '","browseInPlace":true,"entries":[]},{"id":"zb","name":"锁定折叠","x":680,"y":120,"width":420,"height":360,"collapsed":true,"locked":true,"viewMode":"grid","tileSize":1,"nameLines":2,"mappedFolder":"' + $escB + '","browseInPlace":true,"entries":[]}],"settings":{"theme":"system","compact":false,"language":"zh","guideDone":true,"labelStyle":0}}'
[System.IO.File]::WriteAllText((Join-Path $data 'layout.json'), $layout, $noBom)
$port = 8099

$results = New-Object System.Collections.Generic.List[string]
function Note([string]$name, [bool]$ok, [string]$detail) {
  $line = '{0} {1} {2}' -f ($(if ($ok) { 'PASS' } else { 'FAIL' })), $name, $detail
  Write-Output $line
  $results.Add($line) | Out-Null
}
$sig = @'
using System;
using System.Runtime.InteropServices;
public class U32 {
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool f);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
 [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
 [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
 [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
}
'@
Add-Type -TypeDefinition $sig -Language CSharp | Out-Null
function Kind-Cond($kind) {
  return [System.Windows.Automation.PropertyCondition]::new($AE::ControlTypeProperty, $kind)
}
function Find-Window([string]$title, [int]$ms) {
  $cond = [System.Windows.Automation.PropertyCondition]::new($AE::NameProperty, $title)
  $deadline = (Get-Date).AddMilliseconds($ms)
  while ((Get-Date) -lt $deadline) {
    $w = $AE::RootElement.FindFirst($TS::Children, $cond)
    if ($null -ne $w) { return $w }
    Start-Sleep -Milliseconds 120
  }
  return $null
}
function Focus-Window($w) {
  if ($null -eq $w) { return $false }
  $h = [IntPtr]$w.Current.NativeWindowHandle
  $cur = [U32]::GetCurrentThreadId()
  for ($i = 0; $i -lt 25; $i++) {
    [U32]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero)
    [U32]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
    $pid0 = [uint32]0
    $tid = [U32]::GetWindowThreadProcessId([U32]::GetForegroundWindow(), [ref]$pid0)
    [void][U32]::AttachThreadInput($cur, $tid, $true)
    [void][U32]::ShowWindow($h, 9)
    [void][U32]::BringWindowToTop($h)
    [void][U32]::SetForegroundWindow($h)
    [void][U32]::AttachThreadInput($cur, $tid, $false)
    if ([U32]::GetForegroundWindow() -eq $h) { return $true }
    Start-Sleep -Milliseconds 40
  }
  return ([U32]::GetForegroundWindow() -eq $h)
}
function Click-At($r) {
  $x = [int]($r.X + $r.Width / 2); $y = [int]($r.Y + $r.Height / 2)
  [void][U32]::SetCursorPos($x, $y)
  Start-Sleep -Milliseconds 60
  [void][U32]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
  [void][U32]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 120
  return $true
}
function Invoke-El($e) {
  if ($null -eq $e) { return $false }
  try {
    $p = $e.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
    $p.Invoke()
    Start-Sleep -Milliseconds 220
    return $true
  } catch { }
  try { Click-At $e.Current.BoundingRectangle; return $true } catch { return $false }
}
function More-Button($win) {
  $r = $win.Current.BoundingRectangle
  $best = $null
  foreach ($b in $win.FindAll($TS::Descendants, (Kind-Cond $CT::Button))) {
    if ($b.Current.IsOffscreen) { continue }
    $br = $b.Current.BoundingRectangle
    if ($br.Height -le 6 -or $br.Y -gt ($r.Y + 64)) { continue }
    if ($null -eq $best -or $br.X -gt $best.Current.BoundingRectangle.X) { $best = $b }
  }
  return $best
}
function Menu-Item([string]$like, [int]$ms) {
  $deadline = (Get-Date).AddMilliseconds($ms)
  while ((Get-Date) -lt $deadline) {
    foreach ($k in @($CT::ListItem, $CT::MenuItem, $CT::Button)) {
      foreach ($w in $AE::RootElement.FindAll($TS::Children, $AC::TrueCondition)) {
        foreach ($e in $w.FindAll($TS::Descendants, (Kind-Cond $k))) {
          if ($e.Current.Name -notlike $like) { continue }
          if ($e.Current.IsOffscreen) { continue }
          return $e
        }
      }
    }
    Start-Sleep -Milliseconds 200
  }
  return $null
}
function Press-Esc {
  [void][U32]::keybd_event(0x1B, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 40
  [void][U32]::keybd_event(0x1B, 0, 2, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 260
}
function Press-Tab {
  [void][U32]::keybd_event(0x09, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 40
  [void][U32]::keybd_event(0x09, 0, 2, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 260
}
function Open-And-Pick($win, [string]$itemName) {
  if ($null -eq $win) { return $false }
  Press-Esc
  $w = Find-Window ($win.Current.Name) 2500
  if ($null -eq $w) { return $false }
  $null = Focus-Window $w
  if (-not (Invoke-El (More-Button $w))) { return $false }
  Start-Sleep -Milliseconds 350
  $item = Menu-Item $itemName 4000
  if ($null -eq $item) { return $false }
  return (Invoke-El $item)
}
function Layout-Text() {
  return [System.IO.File]::ReadAllText((Join-Path $data 'layout.json'))
}
function Set-Value($el, [string]$text) {
  if ($null -eq $el) { return $false }
  try { $el.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern).SetValue($text); return $true } catch { }
  return $false
}
# 窗口里有两种 Edit：标题框（值=分区名）和对话框的链接框。标题排在前面，所以按值把标题框跳过去
function First-Edit([string]$title, [string]$avoid) {
  $w = Find-Window $title 3000
  if ($null -eq $w) { return $null }
  foreach ($e in $w.FindAll($TS::Descendants, (Kind-Cond $CT::Edit))) {
    if ($e.Current.IsOffscreen) { continue }
    $v = ''
    try { $v = $e.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern).Current.Value } catch { }
    if ($v -eq $avoid) { continue }
    return $e
  }
  return $null
}
function Status-In([string]$title) {
  $w = Find-Window $title 2500
  if ($null -eq $w) { return '' }
  foreach ($e in $w.FindAll($TS::Descendants, (Kind-Cond $CT::Text))) {
    $n = $e.Current.Name
    if ($n -like '*下载*' -or $n -like '*链接*') { return $n }
  }
  return ''
}
# 打开"从链接下载…"并填链接、点确定；返回状态行里会出现的那句话供调用方断言
function Send-Url([string]$title, [string]$link, [string]$avoid) {
  if (-not (Open-And-Pick (Find-Window $title 2500) '从链接下载…')) { return $false }
  Start-Sleep -Milliseconds 900
  $box = First-Edit $title $avoid
  if ($null -eq $box) { return $false }
  if (-not (Set-Value $box $link)) { return $false }
  return (Invoke-El (Menu-Item '开始下载' 3000))
}
function Wait-File([string]$path, [int]$seconds) {
  $deadline = (Get-Date).AddSeconds($seconds)
  while ((Get-Date) -lt $deadline -and -not (Test-Path $path)) { Start-Sleep -Milliseconds 500 }
  return (Test-Path $path)
}

$server = Start-Process -FilePath 'node' -ArgumentList @((Join-Path $PSScriptRoot 'srv3240.js'), ('' + $port)) -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 2

$app = Start-Process -FilePath $exe -ArgumentList @('--data-dir', $data) -PassThru
Start-Sleep -Seconds 6

$zone = Find-Window $titleZone 9000
Note 'zone-found' ($null -ne $zone) $titleZone
$lock = Find-Window $titleLock 3000
Note 'locked-zone-found' ($null -ne $lock) $titleLock
if ($null -eq $zone -or $null -eq $lock) {
  Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
  Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
  $results | Set-Content -Path $report -Encoding UTF8
  exit 1
}

# 1) 方案大小写不算数：hTtP:// 也是合法链接，不能被"只能下载 http 或 https 链接"挡回去
$up = 'hTtP://127.0.0.1:' + $port + '/gd3241-up.bin'
Note 'uppercase-url-confirmed' (Send-Url $titleZone $up $nameA) ('url=' + $up)
$landA = Join-Path $dl 'gd3241-up.bin'
Note 'uppercase-scheme-downloaded' (Wait-File $landA 25) ('path=' + $landA)
$bytes = 0
if (Test-Path $landA) { $bytes = (Get-Item $landA).Length }
Note 'uppercase-payload-intact' ($bytes -eq 200000) ('bytes=' + $bytes)
$st = ''
$poll = (Get-Date).AddSeconds(6)
while ((Get-Date) -lt $poll -and $st -notlike '*已定位*') { $st = Status-In $titleZone; Start-Sleep -Milliseconds 200 }
Note 'uppercase-status-says-located' ($st -like '*已定位*') ('status=' + $st)
Press-Esc

# 2) 非 http 依旧要挡，并且说实话
Note 'ftp-url-rejected' (Send-Url $titleZone 'ftp://example.com/nope.bin' $nameA) ''
Start-Sleep -Milliseconds 1400
$st2 = Status-In $titleZone
Note 'non-http-rejected-honestly' ($st2 -like '*http*') ('status=' + $st2)
Press-Esc

# 3) 折叠+锁定的分区：文件照样落地，但绝不能为了"定位"把分区撑开、把锁定设置改掉
$lockUrl = 'http://127.0.0.1:' + $port + '/gd3241-lock.bin'
Note 'locked-url-confirmed' (Send-Url $titleLock $lockUrl $nameB) ''
$landB = Join-Path $dlB 'gd3241-lock.bin'
Note 'locked-file-landed' (Wait-File $landB 25) ('path=' + $landB)
Start-Sleep -Seconds 3
$zb = @((Layout-Text | ConvertFrom-Json).zones | Where-Object { $_.id -eq 'zb' })[0]
Note 'locked-zone-stays-collapsed' (($zb.collapsed -eq $true) -and ($zb.locked -eq $true)) ('collapsed=' + $zb.collapsed + ' locked=' + $zb.locked)
$st3 = Status-In $titleLock
Note 'locked-status-no-false-locate' ($st3 -notlike '*已定位*') ('status=' + $st3)
Note 'locked-window-alive' ($null -ne (Find-Window $titleLock 2500)) ''
Press-Esc

# 4) 分区名可以长到 255 字：气泡缓冲是定长的，超长会把整个进程带走，所以必须裁切而不是崩
$longName = ('L' * 250) + '-3241'
$tb = First-Edit $titleZone
$null = Set-Value $tb $longName
Press-Tab
Start-Sleep -Milliseconds 1500
Note 'long-name-persisted' ((Layout-Text) -like ('*' + $longName + '*')) ('len=' + $longName.Length)
$titleLong = 'GuoDesk · ' + $longName
$w4 = Find-Window $titleLong 4000
Note 'long-name-window-found' ($null -ne $w4) ''
if ($null -ne $w4) {
  $longUrl = 'http://127.0.0.1:' + $port + '/gd3241-long.bin'
  Note 'long-name-url-confirmed' (Send-Url $titleLong $longUrl $longName) ''
  $st4 = Status-In $titleLong
  Write-Output ('LONG-STATUS ' + $st4)
  $landC = Join-Path $dl 'gd3241-long.bin'
  Note 'long-name-file-landed' (Wait-File $landC 25) ('path=' + $landC)
  # 气泡在完成回调里发，给它两秒落地时间
  Start-Sleep -Seconds 3
  Note 'long-name-toast-survived' (-not $app.HasExited) ''
  Note 'long-name-window-still-there' ($null -ne (Find-Window $titleLong 3000)) ''
}

Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 600
$results | Set-Content -Path $report -Encoding UTF8
$fails = @($results | Where-Object { $_ -like 'FAIL*' })
Write-Output ('SUMMARY passed={0} failed={1}' -f ($results.Count - $fails.Count), $fails.Count)
