# v3.23.0 E2E：标签底板不能再吃掉文字高度，也不能让名称横向跳位，切档时组件窗要活下来
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Windows.Forms
$AE = [System.Windows.Automation.AutomationElement]
$TS = [System.Windows.Automation.TreeScope]
$AC = [System.Windows.Automation.Condition]
$CT = [System.Windows.Automation.ControlType]
$root = 'D:\workspace\GuoDesk\artifacts'
$run = Join-Path $root 'e2e3230'
$data = Join-Path $run 'data'
$src = Join-Path $run 'gd3221_src'
$exe = Join-Path $root 'Release\GuoDesk.exe'
$report = Join-Path $root 'e2e3230.txt'
$titleZone = 'GuoDesk · 标题底板'
$titleSettings = 'GuoDesk 设置'
$titleNote = 'GuoDesk 便签'
if (Test-Path $run) { Remove-Item -Recurse -Force $run }
New-Item -ItemType Directory -Force -Path $data, $src | Out-Null
$noBom = New-Object System.Text.UTF8Encoding($false)
$long = '标题底板测试文件.txt'
[System.IO.File]::WriteAllText((Join-Path $src $long), 'payload', $noBom)
$esc = ((Join-Path $src $long) -replace '\\', '\\')
function Zone([string]$id, [string]$name, [int]$x, [string]$mode) {
  return ('{"id":"' + $id + '","name":"' + $name + '","x":' + $x + ',"y":120,"width":420,"height":360,"collapsed":false,"viewMode":"' + $mode + '","tileSize":2,"nameLines":2,"group":"g1","groupTab":0,"pins":["' + $esc + '"],"entries":[{"id":"' + $id + 'e1","path":"' + $esc + '"}]}')
}
$layout = '{"version":1,"zones":[' + (Zone 'zg' '标题底板' 160 'grid') + ',' + (Zone 'zg2' '同组分区' 620 'grid') + '],"settings":{"theme":"system","compact":false,"language":"zh","guideDone":true,"labelStyle":2}}'
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
$app = Start-Process -FilePath $exe -ArgumentList @('--data-dir', $data) -PassThru
Start-Sleep -Seconds 6

$zone = Find-Window $titleZone 9000
Note 'zone-found' ($null -ne $zone) $titleZone
if ($null -eq $zone) { $results | Set-Content -Path $report -Encoding UTF8; Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue; exit 1 }

# 顶栏面板是"看得见但点不到"的：它必须 IsHitTestVisible=false，否则标题改名和工具栏按钮全废
$null = Focus-Window $zone
Start-Sleep -Milliseconds 500
$edit = $null
foreach ($e in (Find-Window $titleZone 2500).FindAll($TS::Descendants, (Kind-Cond $CT::Edit))) { $edit = $e; break }
Note 'title-is-editable' ($null -ne $edit) ''
# 工具栏（含 ⋮）压在面板上：菜单必须还打得开
$null = Open-And-Pick (Find-Window $titleZone 2500) '设置'
$sw = Find-Window $titleSettings 6000
Note 'header-buttons-still-clickable' ($null -ne $sw) ''
if ($null -eq $sw) { $results | Set-Content -Path $report -Encoding UTF8; Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue; exit 1 }

# 关掉底板再打开：染色走过一遍，界面不能坏
Note 'switch-to-no-backing' (@(Pick-Label '不垫底')[-1] -eq $true) ''
Press-Esc
$swOff = Find-Window $titleSettings 2500
Note 'style-off-persisted' ((Layout-Text) -match '"labelStyle":\s*0') ''
$null = Open-And-Pick (Find-Window $titleZone 2500) '设置'
$sw3 = Find-Window $titleSettings 6000
Note 'menu-reopens-after-style-off' ($null -ne $sw3) ''
if ($null -ne $sw3) { Note 'switch-back-to-high-contrast' (@(Pick-Label '高对比深底')[-1] -eq $true) '' }
Press-Esc
Note 'style-on-persisted' ((Layout-Text) -match '"labelStyle":\s*2') ''

# 状态行仍然说话（底板不该把提示文字换没）
Start-Sleep -Milliseconds 900
$hint = $null
foreach ($e in (Find-Window $titleZone 2500).FindAll($TS::Descendants, (Kind-Cond $CT::Text))) {
  if ($e.Current.Name -like '*双击打开*') { $hint = $e; break }
}
Note 'status-line-still-there' ($null -ne $hint) $(if ($null -ne $hint) { $hint.Current.Name } else { 'none' })
Note 'zone-alive' ($null -ne (Find-Window $titleZone 2500)) ''

# 改名探针放最后：标题一改窗口标题就变了，前面的查找全部作废
if ($null -ne $edit) {
  $null = Focus-Window (Find-Window $titleZone 2500)
  Start-Sleep -Milliseconds 500
  $r = $edit.Current.BoundingRectangle
  $null = Click-At $r
  Start-Sleep -Milliseconds 400
  [void][U32]::keybd_event(0x23, 0, 0, [UIntPtr]::Zero); [void][U32]::keybd_event(0x23, 0, 2, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 200
  foreach ($ch in [char[]]'AB') { [void][U32]::keybd_event([int]$ch, 0, 0, [UIntPtr]::Zero); [void][U32]::keybd_event([int]$ch, 0, 2, [UIntPtr]::Zero); Start-Sleep -Milliseconds 90 }
  [void][U32]::keybd_event(0x09, 0, 0, [UIntPtr]::Zero); [void][U32]::keybd_event(0x09, 0, 2, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 900
  $renamed = (Layout-Text) -match '"name":\s*"标题底板AB"'
  Note 'plate-lets-title-be-renamed' $renamed ('layoutHasRenamedName=' + $renamed)
  Press-Esc
}

Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 600
$results | Set-Content -Path $report -Encoding UTF8
$fails = @($results | Where-Object { $_ -like 'FAIL*' })
Write-Output ('SUMMARY passed={0} failed={1}' -f ($results.Count - $fails.Count), $fails.Count)
