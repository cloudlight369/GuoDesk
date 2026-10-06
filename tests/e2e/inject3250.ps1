# v3.25.0 E2E：叠放浮层要能摊开成员、不许改分区高度，「在分区里展开」要回到发起它的那一页
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Windows.Forms
$AE = [System.Windows.Automation.AutomationElement]
$TS = [System.Windows.Automation.TreeScope]
$AC = [System.Windows.Automation.Condition]
$CT = [System.Windows.Automation.ControlType]
$root = 'D:\workspace\GuoDesk\artifacts'
$run = Join-Path $root 'e2e3250'
$data = Join-Path $run 'data'
$src = Join-Path $run 'files'
$exe = Join-Path $root 'Release\GuoDesk.exe'
$report = Join-Path $root 'e2e3250.txt'
$titleZone = 'GuoDesk · 摊开这一叠'
$titlePeek = 'GuoDesk 叠放浮层'
if (Test-Path $run) { Remove-Item -Recurse -Force $run }
New-Item -ItemType Directory -Force -Path $data, $src | Out-Null
$noBom = New-Object System.Text.UTF8Encoding($false)
$names = @('peek-1.txt', 'peek-2.txt', 'peek-3.txt', 'peek-4.txt')
foreach ($n in $names) { [System.IO.File]::WriteAllText((Join-Path $src $n), $n, $noBom) }
$esc = ($src -replace '\\', '\\')
function Entry([string]$id, [string]$file) {
  return '{"id":"' + $id + '","path":"' + $esc + '\\' + $file + '","stack":"s1"}'
}
$layout = '{"version":1,"zones":[{"id":"zp","name":"摊开这一叠","x":150,"y":120,"width":460,"height":360,"collapsed":false,"viewMode":"grid","tileSize":2,"nameLines":2,"stacks":[{"id":"s1","name":"取件"}],"entries":[' + ((Entry 'p1' $names[0]) + ',' + (Entry 'p2' $names[1]) + ',' + (Entry 'p3' $names[2]) + ',' + (Entry 'p4' $names[3])) + ']}],"settings":{"theme":"system","compact":false,"language":"zh","guideDone":true,"labelStyle":0}}'
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
function Zone-Height([string]$title) {
  $w = Find-Window $title 2500
  if ($null -eq $w) { return -1 }
  return [int]$w.Current.BoundingRectangle.Height
}

$app = Start-Process -FilePath $exe -ArgumentList @('--data-dir', $data) -PassThru
Start-Sleep -Seconds 6

$zone = Find-Window $titleZone 9000
Note 'zone-found' ($null -ne $zone) $titleZone
if ($null -eq $zone) {
  Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
  $results | Set-Content -Path $report -Encoding UTF8
  exit 1
}
$h0 = Zone-Height $titleZone
Note 'zone-height-measured' ($h0 -gt 100) ('height=' + $h0)

# 角标是叠放唯一"看得见数量"的东西：4 件就得写出 4
$badge = @((In-Window $zone $CT::Text '4'))
Note 'pile-badge-shows-count' ($badge.Count -ge 1) ('hits=' + $badge.Count)

# 1) 点角标 → 分区外面长出浮层，成员名一个个排好，分区高度一点不动
if ($badge.Count -ge 1) {
  $null = Focus-Window (Find-Window $titleZone 2500)
  $null = Click-At $badge[0].Current.BoundingRectangle
  $peek = Find-Window $titlePeek 5000
  Note 'panel-opens-from-badge' ($null -ne $peek) $titlePeek
  $seen = 0
  foreach ($n in $names) { if (@(In-Window $peek $CT::Text $n).Count -ge 1) { $seen++ } }
  Note 'panel-lists-each-member' ($seen -eq 4) ('names=' + $seen)
  Note 'panel-has-expand-button' ($null -ne (Button-Named $peek '在分区里展开')) ''
  Note 'panel-has-close-button' ($null -ne (Button-Named $peek '关闭')) ''
  $h1 = Zone-Height $titleZone
  Note 'zone-height-unchanged-by-panel' ($h1 -eq $h0) ('before=' + $h0 + ' after=' + $h1)
  # 关掉它：浮层是独立窗口，不该留残骸
  $null = Invoke-El (Button-Named $peek '关闭')
  Start-Sleep -Milliseconds 800
  Note 'panel-closes-by-button' ($null -eq (Find-Window $titlePeek 1500)) ''
}

# 2) 再点开，这次交给「在分区里展开」：浮层关掉、分区里四件都露出来，高度按原规则长
$badge2 = @((In-Window (Find-Window $titleZone 2500) $CT::Text '4'))
if ($badge2.Count -ge 1) {
  $null = Focus-Window (Find-Window $titleZone 2500)
  $null = Click-At $badge2[0].Current.BoundingRectangle
  $peek2 = Find-Window $titlePeek 5000
  Note 'panel-reopens-on-another-click' ($null -ne $peek2) ''
  $h2 = Zone-Height $titleZone
  $null = Invoke-El (Button-Named $peek2 '在分区里展开')
  Start-Sleep -Milliseconds 1200
  Note 'panel-closes-after-expand' ($null -eq (Find-Window $titlePeek 1500)) ''
  $zone2 = Find-Window $titleZone 2500
  $shown = 0
  foreach ($n in $names) { if (@(In-Window $zone2 $CT::Text $n).Count -ge 1) { $shown++ } }
  Note 'expand-in-zone-reveals-members' ($shown -eq 4) ('names=' + $shown)
  # 展开只是把成员画进同一块 GridView：窗口尺寸是用户设定，不该被改动
  $h3 = Zone-Height $titleZone
  Note 'expand-keeps-window-size' ($h3 -eq $h2) ('before=' + $h2 + ' after=' + $h3)
}

Note 'app-still-running' (-not $app.HasExited) ''
Note 'zone-alive' ($null -ne (Find-Window $titleZone 2500)) ''

Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 600
$results | Set-Content -Path $report -Encoding UTF8
$fails = @($results | Where-Object { $_ -like 'FAIL*' })
Write-Output ('SUMMARY passed={0} failed={1}' -f ($results.Count - $fails.Count), $fails.Count)
