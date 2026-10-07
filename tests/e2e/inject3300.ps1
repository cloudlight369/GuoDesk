# v3.30.0 E2E：叠放浮层要能翻页（30 件不再只摊 25 件），选中集跨页保留，小叠不该长出页码条
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Windows.Forms
$AE = [System.Windows.Automation.AutomationElement]
$TS = [System.Windows.Automation.TreeScope]
$AC = [System.Windows.Automation.Condition]
$CT = [System.Windows.Automation.ControlType]
$root = 'D:\workspace\GuoDesk\artifacts'
$run = Join-Path $root 'e2e3300'
$data = Join-Path $run 'data'
$src = Join-Path $run 'files'
$exe = Join-Path $root 'Release\GuoDesk.exe'
$report = Join-Path $root 'e2e3300.txt'
$titleBig = 'GuoDesk · 翻页这一叠'
$titleSmall = 'GuoDesk · 小叠不必翻'
$titlePeek = 'GuoDesk 叠放浮层'
if (Test-Path $run) { Remove-Item -Recurse -Force $run }
New-Item -ItemType Directory -Force -Path $data, $src | Out-Null
$noBom = New-Object System.Text.UTF8Encoding($false)
$big = @()
for ($i = 1; $i -le 30; $i++) { $big += ('pk{0:d2}.txt' -f $i) }
$small = @('sm1.txt', 'sm2.txt', 'sm3.txt', 'sm4.txt')
foreach ($n in ($big + $small)) { [System.IO.File]::WriteAllText((Join-Path $src $n), $n, $noBom) }
$esc = ($src -replace '\\', '\\')
$entries = @()
for ($i = 0; $i -lt $big.Count; $i++) { $entries += ('{"id":"b' + ($i + 1) + '","path":"' + $esc + '\\' + $big[$i] + '","stack":"s1"}') }
for ($i = 0; $i -lt $small.Count; $i++) { $entries += ('{"id":"s' + ($i + 1) + '","path":"' + $esc + '\\' + $small[$i] + '","stack":"s2"}') }
$layout = '{"version":1,"zones":[' +
  '{"id":"zA","name":"翻页这一叠","x":150,"y":120,"width":460,"height":360,"collapsed":false,"viewMode":"grid","tileSize":2,"nameLines":2,"stacks":[{"id":"s1","name":"取件"}],"entries":[' + ($entries[0..29] -join ',') + ']},' +
  '{"id":"zB","name":"小叠不必翻","x":700,"y":120,"width":420,"height":320,"collapsed":false,"viewMode":"grid","tileSize":2,"nameLines":2,"stacks":[{"id":"s2","name":"零钱"}],"entries":[' + ($entries[30..33] -join ',') + ']}' +
  '],"settings":{"theme":"system","compact":false,"language":"zh","guideDone":true,"labelStyle":0}}'
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
function Press-Key([byte]$vk) {
  [void][U32]::keybd_event($vk, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 60
  [void][U32]::keybd_event($vk, 0, 2, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 500
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
function Button-Named($win, [string]$name) {
  foreach ($e in (In-Window $win $CT::Button $name)) { return $e }
  return $null
}
function Cells($win) { return @(In-Window $win $CT::Text 'pk*') }
function Picked($win) {
  $t = @(In-Window $win $CT::Text '已选*')
  if ($t.Count -eq 0) { return 'none' }
  return $t[0].Current.Name
}

$app = Start-Process -FilePath $exe -ArgumentList @('--data-dir', $data) -PassThru
Start-Sleep -Seconds 6

$zone = Find-Window $titleBig 9000
Note 'zone-found' ($null -ne $zone) $titleBig
if ($null -eq $zone) {
  Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
  $results | Set-Content -Path $report -Encoding UTF8
  exit 1
}
$badge = @(In-Window $zone $CT::Text '30')
Note 'badge-counts-whole-pile' ($badge.Count -ge 1) ('hits=' + $badge.Count)
if ($badge.Count -lt 1) {
  Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
  $results | Set-Content -Path $report -Encoding UTF8
  exit 1
}

# 1) 30 件的一叠：第一屏摊 25 格，页码条露面，尾巴上的文件这一屏看不到
$null = Focus-Window $zone
$null = Click-At $badge[0].Current.BoundingRectangle
$peek = Find-Window $titlePeek 6000
Start-Sleep -Milliseconds 1400
Note 'panel-opens' ($null -ne $peek) $titlePeek
if ($null -eq $peek) {
  Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
  $results | Set-Content -Path $report -Encoding UTF8
  exit 1
}
$peek = Find-Window $titlePeek 2000
Note 'pager-is-visible' ($null -ne (Button-Named $peek '▶')) ''
Note 'page-label-says-first' (@(In-Window $peek $CT::Text '第 1 / 2 页*').Count -ge 1) ''
Note 'first-page-lists-25' ((Cells $peek).Count -eq 25) ('cells=' + (Cells $peek).Count)
Note 'head-is-on-first-page' (@(In-Window $peek $CT::Text 'pk01.txt').Count -ge 1) ''
Note 'tail-waits-on-next-page' (@(In-Window $peek $CT::Text 'pk26.txt').Count -eq 0) ''
$h0 = [int]$peek.Current.BoundingRectangle.Height
Note 'panel-height-measured' ($h0 -gt 200) ('height=' + $h0)
$prev0 = Button-Named $peek '◀'
Note 'back-disabled-on-first-page' ($null -ne $prev0 -and -not $prev0.Current.IsEnabled) ''
$next = Button-Named $peek '▶'
Note 'next-enabled-on-first-page' ($null -ne $next -and $next.Current.IsEnabled) ''

# 2) 真点 ▶（不用 InvokePattern：只有真点击才会把键盘焦点交给面板），翻页不许把面板尺寸改掉
$null = Click-At $next.Current.BoundingRectangle
Start-Sleep -Milliseconds 900
$peek = Find-Window $titlePeek 2000
Note 'page-label-says-last' (@(In-Window $peek $CT::Text '第 2 / 2 页*').Count -ge 1) ''
Note 'last-page-lists-the-rest' ((Cells $peek).Count -eq 5) ('cells=' + (Cells $peek).Count)
Note 'first-page-left-behind' (@(In-Window $peek $CT::Text 'pk01.txt').Count -eq 0) ''
Note 'tail-is-now-visible' (@(In-Window $peek $CT::Text 'pk30.txt').Count -ge 1) ''
$h1 = [int]$peek.Current.BoundingRectangle.Height
Note 'page-flip-keeps-size' ($h1 -eq $h0) ('before=' + $h0 + ' after=' + $h1)
$prev = Button-Named $peek '◀'
Note 'back-is-enabled-on-last-page' ($null -ne $prev -and $prev.Current.IsEnabled) ''
$n2 = Button-Named $peek '▶'
Note 'next-disabled-on-last-page' ($null -ne $n2 -and -not $n2.Current.IsEnabled) ''

# 3) 键盘翻页：刚真点过 ▶，焦点就在面板里，左右键该直接换页
$null = Press-Key 0x25
$peek = Find-Window $titlePeek 2000
Note 'arrow-left-flips-back' (@(In-Window $peek $CT::Text '第 1 / 2 页*').Count -ge 1) ''
Note 'arrow-lands-on-first-page' ((Cells $peek).Count -eq 25) ('cells=' + (Cells $peek).Count)
$null = Press-Key 0x27
$peek = Find-Window $titlePeek 2000
Note 'arrow-right-goes-forward' (@(In-Window $peek $CT::Text '第 2 / 2 页*').Count -ge 1) ''

# 4) 最后一页上 Ctrl 选一件，用 ◀ 翻回去选中还在（picked 存的是路径，不是这一屏的第几格）
$c27 = @(In-Window $peek $CT::Text 'pk27.txt')
Note 'target-cell-on-last-page' ($c27.Count -ge 1) ''
if ($c27.Count -ge 1) {
  $null = Ctrl-Click-At $c27[0].Current.BoundingRectangle
  $peek = Find-Window $titlePeek 2000
  Note 'pick-on-last-page' ((Picked $peek) -like '*1*') ('text=' + (Picked $peek))
}
$back = Button-Named $peek '◀'
$null = Click-At $back.Current.BoundingRectangle
Start-Sleep -Milliseconds 900
$peek = Find-Window $titlePeek 2000
Note 'back-button-flips' (@(In-Window $peek $CT::Text '第 1 / 2 页*').Count -ge 1) ''
Note 'pick-survives-the-flip' ((Picked $peek) -like '*1*') ('text=' + (Picked $peek))
Note 'first-page-is-back' ((Cells $peek).Count -eq 25) ('cells=' + (Cells $peek).Count)
$c2 = @(In-Window $peek $CT::Text 'pk02.txt')
if ($c2.Count -ge 1) {
  $null = Ctrl-Click-At $c2[0].Current.BoundingRectangle
  $peek = Find-Window $titlePeek 2000
  Note 'pick-adds-across-pages' ((Picked $peek) -like '*2*') ('text=' + (Picked $peek))
}
$null = Invoke-El (Button-Named (Find-Window $titlePeek 2000) '关闭')
Start-Sleep -Milliseconds 900
Note 'panel-closes' ($null -eq (Find-Window $titlePeek 1500)) ''

# 5) 只有 4 件的小叠：一屏就摊得下，不该长出页码条
$zoneB = Find-Window $titleSmall 6000
Note 'small-zone-found' ($null -ne $zoneB) $titleSmall
$b2 = @(In-Window $zoneB $CT::Text '4')
Note 'small-badge-found' ($b2.Count -ge 1) ('hits=' + $b2.Count)
if ($b2.Count -ge 1) {
  $null = Focus-Window (Find-Window $titleSmall 2000)
  $null = Click-At $b2[0].Current.BoundingRectangle
  $pk2 = Find-Window $titlePeek 6000
  Start-Sleep -Milliseconds 1200
  Note 'small-panel-opens' ($null -ne $pk2) ''
  if ($null -ne $pk2) {
    $pk2 = Find-Window $titlePeek 2000
    Note 'small-panel-has-no-pager' ($null -eq (Button-Named $pk2 '▶')) ''
    $sm = @(In-Window $pk2 $CT::Text 'sm*.txt')
    Note 'small-panel-lists-all' ($sm.Count -eq 4) ('cells=' + $sm.Count)
    Note 'small-panel-title-counts-four' (@(In-Window $pk2 $CT::Text '*4 项*').Count -ge 1) ''
    $null = Invoke-El (Button-Named $pk2 '关闭')
    Start-Sleep -Milliseconds 800
  }
}

Note 'app-still-running' (-not $app.HasExited) ''
Note 'zone-alive' ($null -ne (Find-Window $titleBig 2500)) ''

Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 600
$results | Set-Content -Path $report -Encoding UTF8
$fails = @($results | Where-Object { $_ -like 'FAIL*' })
Write-Output ('SUMMARY passed={0} failed={1}' -f ($results.Count - $fails.Count), $fails.Count)
