# v3.27.0 E2E：折叠高度要跟着屏幕缩放走，列表选中要整行看得出，悬停不许把选中洗掉
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Windows.Forms
$AE = [System.Windows.Automation.AutomationElement]
$TS = [System.Windows.Automation.TreeScope]
$AC = [System.Windows.Automation.Condition]
$CT = [System.Windows.Automation.ControlType]
$root = 'D:\workspace\GuoDesk\artifacts'
$run = Join-Path $root 'e2e3260'
$data = Join-Path $run 'data'
$src = Join-Path $run 'files'
$exe = Join-Path $root 'Release\GuoDesk.exe'
$report = Join-Path $root 'e2e3270.txt'
$titleList = 'GuoDesk · 整行底色'
$titleCollapse = 'GuoDesk · 折叠高度'
$titlePeek = 'GuoDesk 叠放浮层'
$titleGuide = '欢迎使用 GuoDesk'
if (Test-Path $run) { Remove-Item -Recurse -Force $run }
New-Item -ItemType Directory -Force -Path $data, $src | Out-Null
$noBom = New-Object System.Text.UTF8Encoding($false)
$names = @('row-1.txt', 'row-2.txt', 'row-3.txt', 'row-4.txt')
foreach ($n in $names) { [System.IO.File]::WriteAllText((Join-Path $src $n), $n, $noBom) }
$esc = ($src -replace '\\', '\\')
function Entry([string]$id, [string]$file) {
  $q = [char]34
  return '{' + $q + 'id' + $q + ':' + $q + $id + $q + ',' + $q + 'path' + $q + ':' + $q + $esc + '\\' + $file + $q + '}'
}
$ents = @()
for ($i = 0; $i -lt 5; $i++) { $ents += (Entry ('k' + ($i + 1)) $names[$i]) }
# guideDone 故意写 false：引导页要在启动时自己弹出来，才能验它到底讲了哪几件事
$layout = '{"version":1,"zones":[{"id":"zl","name":"整行底色","x":150,"y":120,"width":460,"height":360,"collapsed":false,"viewMode":"list","tileSize":2,"nameLines":2,"entries":[' + ($ents -join ',') + ']},{"id":"zc","name":"折叠高度","x":700,"y":700,"width":360,"height":240,"collapsed":true,"viewMode":"list","tileSize":2,"nameLines":2,"entries":[]}],"settings":{"theme":"system","compact":false,"language":"zh","guideDone":true,"labelStyle":0}}'
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
 [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr h);
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
  Start-Sleep -Milliseconds 150
  return $true
}
function Ctrl-Click-At($r) {
  [void][U32]::keybd_event(0x11, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 40
  $x = [int]($r.X + $r.Width / 2); $y = [int]($r.Y + $r.Height / 2)
  [void][U32]::SetCursorPos($x, $y)
  Start-Sleep -Milliseconds 60
  [void][U32]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
  [void][U32]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 150
  [void][U32]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 120
  return $true
}
function Invoke-El($e) {
  if ($null -eq $e) { return $false }
  try {
    $p = $e.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
    $p.Invoke()
    Start-Sleep -Milliseconds 260
    return $true
  } catch { }
  try { Click-At $e.Current.BoundingRectangle; return $true } catch { return $false }
}
function In-Window($win, $kind, [string]$like) {
  if ($null -eq $win) { return @() }
  $hits = @()
  foreach ($e in $win.FindAll($TS::Descendants, (Kind-Cond $kind))) {
    if ($e.Current.IsOffscreen) { continue }
    if ($e.Current.Name -notlike $like) { continue }
    $hits += $e
  }
  return $hits
}
function In-Window-Any($win, $kind, [string]$like) {
  # 引导页可以滚动：内容在树里就算讲到了，不该因为"这一屏没露出来"判失败
  if ($null -eq $win) { return @() }
  $hits = @()
  foreach ($e in $win.FindAll($TS::Descendants, (Kind-Cond $kind))) {
    if ($e.Current.Name -notlike $like) { continue }
    $hits += $e
  }
  return $hits
}
function Button-Named($win, [string]$name) {
  foreach ($e in (In-Window $win $CT::Button $name)) { return $e }
  return $null
}
function Layout-Text() {
  return [System.IO.File]::ReadAllText((Join-Path $data 'layout.json'))
}

Add-Type -AssemblyName System.Drawing
function Sample([int]$x, [int]$y) {
  $bmp = New-Object System.Drawing.Bitmap(1, 1)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($x, $y, 0, 0, (New-Object System.Drawing.Size(1, 1)))
  $c = $bmp.GetPixel(0, 0)
  $g.Dispose(); $bmp.Dispose()
  return $c
}
function Band-At($zoneRect, $row, [string]$tag) {
  $x = [int]($zoneRect.Right - 26)
  $y = [int]($row.Current.BoundingRectangle.Y + $row.Current.BoundingRectangle.Height / 2)
  $c = Sample $x $y
  $delta = [int]$c.B - [int]$c.R
  Write-Host ($tag + ' b-r=' + $delta + ' rgb=' + $c.R + ',' + $c.G + ',' + $c.B)
  return $delta
}

$app = Start-Process -FilePath $exe -ArgumentList @('--data-dir', $data) -PassThru
Start-Sleep -Seconds 6
$zone = Find-Window $titleList 9000
Note 'list-zone-found' ($null -ne $zone) $titleList
if ($null -eq $zone) { $results | Set-Content -Path $report -Encoding UTF8; Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue; exit 1 }
$czone = Find-Window $titleCollapse 6000
Note 'collapsed-zone-found' ($null -ne $czone) ''
if ($null -ne $czone) {
  $hwnd = [IntPtr] $czone.Current.NativeWindowHandle
  $dpi = [int] [U32]::GetDpiForWindow($hwnd)
  $want = [int] [math]::Round(88 * $dpi / 96)
  $got = [int] $czone.Current.BoundingRectangle.Height
  Note 'collapsed-height-follows-dpi' ($got -eq $want) ('dpi=' + $dpi + ' actual=' + $got + ' want=' + $want)
}
$rows = @(In-Window $zone $CT::Text 'row-*')
Note 'list-shows-four' ($rows.Count -eq 4) ('rows=' + $rows.Count)
if ($rows.Count -lt 4) { $results | Set-Content -Path $report -Encoding UTF8; Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue; exit 1 }
$zr = $zone.Current.BoundingRectangle
$null = Focus-Window $zone
Note 'band-absent-before-select' ((Band-At $zr $rows[0] 'r1') -lt 21) ''
$null = Click-At $rows[0].Current.BoundingRectangle
Start-Sleep -Milliseconds 500
# 点一下之后列表会重画，旧元素的位置会变成 NaN：每轮都要重新取一遍
$zone = Find-Window $titleList 3000
$rows = @(In-Window $zone $CT::Text 'row-*')
$zr = $zone.Current.BoundingRectangle
Note 'selected-row-carries-band' ((Band-At $zr $rows[0] 'r1') -gt 20) ''
Note 'neighbour-stays-flat' ((Band-At $zr $rows[1] 'r2') -lt 21) ''
$null = Ctrl-Click-At $rows[2].Current.BoundingRectangle
Start-Sleep -Milliseconds 500
$zone = Find-Window $titleList 3000
$rows = @(In-Window $zone $CT::Text 'row-*')
$zr = $zone.Current.BoundingRectangle
Note 'ctrl-pick-adds-second-band' ((Band-At $zr $rows[2] 'r3') -gt 20) ''
Note 'first-still-selected' ((Band-At $zr $rows[0] 'r1') -gt 20) ''
# 悬停：选中的那行不许被灰色洗掉，没选中的那行不许偷偷变蓝
$x1 = [int]($rows[0].Current.BoundingRectangle.X + 6); $y1 = [int]($rows[0].Current.BoundingRectangle.Y + 6)
$null = [U32]::SetCursorPos($x1, $y1)
Start-Sleep -Milliseconds 350
Note 'hover-keeps-selection' ((Band-At $zr $rows[0] 'r1-hover') -gt 20) ''
$x2 = [int]($rows[1].Current.BoundingRectangle.X + 6); $y2 = [int]($rows[1].Current.BoundingRectangle.Y + 6)
$null = [U32]::SetCursorPos($x2, $y2)
Start-Sleep -Milliseconds 350
Note 'hover-unselected-not-blue' ((Band-At $zr $rows[1] 'r2-hover') -lt 21) ''
$null = [U32]::SetCursorPos(4, 4)
Note 'app-still-running' (@(Get-Process GuoDesk -ErrorAction SilentlyContinue).Count -ge 1) ''
Note 'zone-alive' ($null -ne (Find-Window $titleList 2500)) ''
$results | Set-Content -Path $report -Encoding UTF8
$bad = @($results | Where-Object { $_ -like 'FAIL*' }).Count
Write-Output ('SUMMARY passed=' + ($results.Count - $bad) + ' failed=' + $bad)
Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
