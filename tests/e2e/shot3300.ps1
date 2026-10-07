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

Add-Type -AssemblyName System.Drawing
function Shot-Peek([string]$out) {
  $w = Find-Window $titlePeek 3000
  if ($null -eq $w) { Write-Output ('missing-window=' + $out); return }
  $r = $w.Current.BoundingRectangle
  $x = [int]($r.X) - 4; $y = [int]($r.Y) - 30; $w2 = [int]($r.Width) + 8; $h2 = [int]($r.Height) + 36
  $bmp = New-Object System.Drawing.Bitmap($w2, $h2)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($x, $y, 0, 0, (New-Object System.Drawing.Size($w2, $h2)))
  $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
  Write-Output ('shot=' + $out + ' ' + $w2 + 'x' + $h2)
}
$app = Start-Process -FilePath $exe -ArgumentList @('--data-dir', $data) -PassThru
Start-Sleep -Seconds 6
$zone = Find-Window $titleBig 9000
$badge = @(In-Window $zone $CT::Text '30')
$null = Focus-Window $zone
$null = Click-At $badge[0].Current.BoundingRectangle
Start-Sleep -Milliseconds 1500
Shot-Peek (Join-Path $root 'shot3300-page1.png')
$nx = Button-Named (Find-Window $titlePeek 2000) '▶'
$null = Click-At $nx.Current.BoundingRectangle
Start-Sleep -Milliseconds 1000
Shot-Peek (Join-Path $root 'shot3300-page2.png')
$null = Invoke-El (Button-Named (Find-Window $titlePeek 2000) '关闭')
Start-Sleep -Milliseconds 900
$zoneB = Find-Window $titleSmall 6000
$b2 = @(In-Window $zoneB $CT::Text '4')
$null = Focus-Window $zoneB
$null = Click-At $b2[0].Current.BoundingRectangle
Start-Sleep -Milliseconds 1400
Shot-Peek (Join-Path $root 'shot3300-small.png')
Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
Write-Output 'SHOT-DONE'
