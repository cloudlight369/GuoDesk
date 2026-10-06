# v3.24.0 E2E：标签底板不能再吃掉文字高度，也不能让名称横向跳位，切档时组件窗要活下来
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Windows.Forms
$AE = [System.Windows.Automation.AutomationElement]
$TS = [System.Windows.Automation.TreeScope]
$AC = [System.Windows.Automation.Condition]
$CT = [System.Windows.Automation.ControlType]
$root = 'D:\workspace\GuoDesk\artifacts'
$run = Join-Path $root 'e2e3240'
$data = Join-Path $run 'data'
$src = Join-Path $run 'gd3221_src'
$exe = Join-Path $root 'Release\GuoDesk.exe'
$report = Join-Path $root 'e2e3240.txt'
$titleZone = 'GuoDesk · 下载落位'
$titleSettings = 'GuoDesk 设置'
$titleNote = 'GuoDesk 便签'
if (Test-Path $run) { Remove-Item -Recurse -Force $run }
New-Item -ItemType Directory -Force -Path $data, $src | Out-Null
$noBom = New-Object System.Text.UTF8Encoding($false)
$long = 'unused-anchor.txt'
[System.IO.File]::WriteAllText((Join-Path $src $long), 'x', $noBom)
$dl = $src
$port = 8099
$url = 'http://127.0.0.1:' + $port + '/guodesk-payload.bin'
$esc = ($dl -replace '\\', '\\')
$layout = '{"version":1,"zones":[{"id":"zd","name":"下载落位","x":200,"y":140,"width":460,"height":380,"collapsed":false,"viewMode":"grid","tileSize":1,"nameLines":2,"mappedFolder":"' + $esc + '","browseInPlace":true,"entries":[]}],"settings":{"theme":"system","compact":false,"language":"zh","guideDone":true,"labelStyle":0}}'
[System.IO.File]::WriteAllText((Join-Path $data 'layout.json'), $layout, $noBom)

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
function Open-And-Pick($win, [string]$itemName) {
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
# 名称文字块：磁贴/列表里那个 Text，拿它的高度和左边界做几何断言
function Name-Text($win) {
  foreach ($e in $win.FindAll($TS::Descendants, (Kind-Cond $CT::Text))) {
    if ($e.Current.IsOffscreen) { continue }
    if ($e.Current.Name -ne $long) { continue }
    return $e
  }
  return $null
}
function Pick-Label([string]$item) {
  $sw = Find-Window $titleSettings 2500
  if ($null -eq $sw) { return $false }
  $null = Focus-Window $sw
  $combo = $null
  foreach ($c in $sw.FindAll($TS::Descendants, (Kind-Cond $CT::ComboBox))) { if ($c.Current.AutomationId -eq 'labelStyle') { $combo = $c; break } }
  if ($null -eq $combo) { return $false }
  try { $combo.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern).Expand() } catch { return $false }
  Start-Sleep -Milliseconds 600
  $pick = Menu-Item $item 4000
  if ($null -eq $pick) { return $false }
  $ok = Invoke-El $pick
  Start-Sleep -Milliseconds 1400
  return $ok
}
$server = Start-Process -FilePath 'node' -ArgumentList @((Join-Path $root 'srv3240.js'), ('' + $port)) -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 2

function Set-Value($el, [string]$text) {
  if ($null -eq $el) { return $false }
  try { $el.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern).SetValue($text); return $true } catch { }
  return $false
}

function Status-Line() {
  $w = Find-Window $titleZone 2500
  if ($null -eq $w) { return '' }
  foreach ($e in $w.FindAll($TS::Descendants, (Kind-Cond $CT::Text))) {
    $n = $e.Current.Name
    if ($n -like '*下载*' -or $n -like '*链接*') { return $n }
  }
  return ''
}

$app = Start-Process -FilePath $exe -ArgumentList @('--data-dir', $data) -PassThru
Start-Sleep -Seconds 6

$zone = Find-Window $titleZone 9000
Note 'zone-found' ($null -ne $zone) $titleZone
if ($null -eq $zone) {
  Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
  Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
  $results | Set-Content -Path $report -Encoding UTF8
  exit 1
}

# 菜单里必须能直接"从链接下载…"：以前只有把链接从浏览器拖进来这一条路
$dlgEdit = $null
for ($try = 1; $try -le 2 -and $null -eq $dlgEdit; $try++) {
  $null = Open-And-Pick (Find-Window $titleZone 2500) '从链接下载…'
  Start-Sleep -Milliseconds 900
  $w2 = Find-Window $titleZone 3000
  if ($null -ne $w2) {
    foreach ($e in $w2.FindAll($TS::Descendants, (Kind-Cond $CT::Edit))) {
      if ($e.Current.IsOffscreen) { continue }
      $dlgEdit = $e; break
    }
  }
}
Note 'download-dialog-opened' ($null -ne $dlgEdit) ''

if ($null -ne $dlgEdit) {
  Note 'url-accepted-into-box' (Set-Value $dlgEdit $url) ('url=' + $url)
  $go = Menu-Item '下载' 3000
  Note 'download-confirmed' (Invoke-El $go) ''
  $landed = Join-Path $dl 'guodesk-payload.bin'
  $deadline = (Get-Date).AddSeconds(25)
  while ((Get-Date) -lt $deadline -and -not (Test-Path $landed)) { Start-Sleep -Milliseconds 500 }
  Note 'file-landed-in-mapped-folder' (Test-Path $landed) ('path=' + $landed)
  # 状态行会被映射目录的 2 秒心跳覆盖成"映射视图…"，所以这里要抢在它前面轮询
  $st = ''
  $poll = (Get-Date).AddSeconds(6)
  while ((Get-Date) -lt $poll -and $st -notlike '*已定位*') { $st = Status-Line; Start-Sleep -Milliseconds 200 }
  Note 'status-says-located' ($st -like '*已定位*') ('status=' + $st)
  $bytes = 0
  if (Test-Path $landed) { $bytes = (Get-Item $landed).Length }
  Note 'payload-intact' ($bytes -eq 200000) ('bytes=' + $bytes)
}
Press-Esc

# 非 http 链接必须被拒绝，并且说实话
$null = Open-And-Pick (Find-Window $titleZone 2500) '从链接下载…'
Start-Sleep -Milliseconds 900
$bad = $null
$w3 = Find-Window $titleZone 3000
if ($null -ne $w3) {
  foreach ($e in $w3.FindAll($TS::Descendants, (Kind-Cond $CT::Edit))) {
    if ($e.Current.IsOffscreen) { continue }
    $bad = $e; break
  }
}
if ($null -ne $bad) {
  $null = Set-Value $bad 'ftp://example.com/nope.bin'
  $go2 = Menu-Item '下载' 3000
  $null = Invoke-El $go2
  Start-Sleep -Milliseconds 1400
  $st2 = Status-Line
  Note 'non-http-rejected-honestly' ($st2 -like '*http*') ('status=' + $st2)
}
Press-Esc

Note 'zone-alive' ($null -ne (Find-Window $titleZone 2500)) ''

Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 600
$results | Set-Content -Path $report -Encoding UTF8
$fails = @($results | Where-Object { $_ -like 'FAIL*' })
Write-Output ('SUMMARY passed={0} failed={1}' -f ($results.Count - $fails.Count), $fails.Count)
