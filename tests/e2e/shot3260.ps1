# v3.26.0 截图：叠放浮层选中两项时的排版（计数单独一行，按钮不能被挤出面板）
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
$report = Join-Path $root 'e2e3260.txt'
$titleZone = 'GuoDesk · 多选这一叠'
$titlePeek = 'GuoDesk 叠放浮层'
$titleGuide = '欢迎使用 GuoDesk'
if (Test-Path $run) { Remove-Item -Recurse -Force $run }
New-Item -ItemType Directory -Force -Path $data, $src | Out-Null
$noBom = New-Object System.Text.UTF8Encoding($false)
$names = @('pick-1.txt', 'pick-2.txt', 'pick-3.txt', 'pick-4.txt', 'pick-5.txt')
foreach ($n in $names) { [System.IO.File]::WriteAllText((Join-Path $src $n), $n, $noBom) }
$esc = ($src -replace '\\', '\\')
function Entry([string]$id, [string]$file) {
  $q = [char]34
  return '{' + $q + 'id' + $q + ':' + $q + $id + $q + ',' + $q + 'path' + $q + ':' + $q + $esc + '\\' + $file + $q + ',' + $q + 'stack' + $q + ':' + $q + 's1' + $q + '}'
}
$ents = @()
for ($i = 0; $i -lt 5; $i++) { $ents += (Entry ('k' + ($i + 1)) $names[$i]) }
# guideDone 故意写 false：引导页要在启动时自己弹出来，才能验它到底讲了哪几件事
$layout = '{"version":1,"zones":[{"id":"zk","name":"多选这一叠","x":150,"y":120,"width":460,"height":360,"collapsed":false,"viewMode":"grid","tileSize":2,"nameLines":2,"stacks":[{"id":"s1","name":"待处理"}],"entries":[' + ($ents -join ',') + ']}],"settings":{"theme":"system","compact":false,"language":"zh","guideDone":false,"labelStyle":0}}'
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

$app = Start-Process -FilePath $exe -ArgumentList @('--data-dir', $data) -PassThru
Start-Sleep -Seconds 6

# 1) 引导页：新加的两件事必须真的看得见，编号也要跟着长到 11
$guide = Find-Window $titleGuide 9000
Note 'guide-opens-on-first-run' ($null -ne $guide) $titleGuide
if ($null -eq $guide) {
  Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
  $results | Set-Content -Path $report -Encoding UTF8
  exit 1
}
Note 'guide-has-eleven-rows' (@(In-Window-Any $guide $CT::Text '10').Count -ge 1 -and @(In-Window-Any $guide $CT::Text '11').Count -ge 1) ''
Note 'guide-teaches-the-download-entry' (@(In-Window-Any $guide $CT::Text '*从链接下载*').Count -ge 1) ''
Note 'guide-teaches-the-badge' (@(In-Window-Any $guide $CT::Text '*点角标摊开这一叠*').Count -ge 1) ''
$null = Focus-Window $guide
Note 'guide-dismisses' (Invoke-El (Button-Named $guide '开始使用')) ''
Start-Sleep -Milliseconds 1500
Note 'guide-window-gone' ($null -eq (Find-Window $titleGuide 1500)) ''
Note 'guide-flag-persisted' ((Layout-Text) -like '*"guideDone":true*') ''

# 2) 浮层 Ctrl 多选：计数跟着走，再点一次要取消
$zone = Find-Window $titleZone 6000
Note 'zone-found' ($null -ne $zone) $titleZone
$badge = @(In-Window $zone $CT::Text '5')
Note 'badge-found' ($badge.Count -ge 1) ''
if ($badge.Count -lt 1) {
  Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
  $results | Set-Content -Path $report -Encoding UTF8
  exit 1
}
$null = Focus-Window $zone
$null = Click-At $badge[0].Current.BoundingRectangle
$peek = Find-Window $titlePeek 5000
# 窗口先出现在 UIA 里、内容才排好版：立刻查会读到一块空面板
Start-Sleep -Milliseconds 1200
Note 'panel-opens' ($null -ne $peek) $titlePeek
Note 'panel-hint-mentions-ctrl' (@(In-Window $peek $CT::Text '*Ctrl*').Count -ge 1) ''
$cells = @(In-Window $peek $CT::Text 'pick-*')
$null = Focus-Window (Find-Window $titlePeek 3000)
$null = Ctrl-Click-At $cells[0].Current.BoundingRectangle
$null = Ctrl-Click-At $cells[1].Current.BoundingRectangle
Add-Type -AssemblyName System.Drawing
$p = Find-Window $titlePeek 3000
$r = $p.Current.BoundingRectangle
$x = [int]$r.X; $y = [int]$r.Y; $w = [int]$r.Width; $h = [int]$r.Height
$bmp = New-Object System.Drawing.Bitmap($w, $h)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($x, $y, 0, 0, (New-Object System.Drawing.Size($w, $h)))
$out = Join-Path $root 'shot3260-panel.png'
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Output ('SAVED ' + $out + ' rect=' + $w + 'x' + $h)
Stop-Process -Id $app.Id -ErrorAction SilentlyContinue
