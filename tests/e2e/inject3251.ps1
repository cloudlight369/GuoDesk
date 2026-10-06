# v3.25.1 E2E：叠放浮层的诚实性——超过 25 件要说清楚，成员被移走要点出来而不是把程序带走
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Windows.Forms
$AE = [System.Windows.Automation.AutomationElement]
$TS = [System.Windows.Automation.TreeScope]
$AC = [System.Windows.Automation.Condition]
$CT = [System.Windows.Automation.ControlType]
$root = 'D:\workspace\GuoDesk\artifacts'
$run = Join-Path $root 'e2e3251'
$data = Join-Path $run 'data'
$src = Join-Path $run 'files'
$exe = Join-Path $root 'Release\GuoDesk.exe'
$report = Join-Path $root 'e2e3251.txt'
$titleZone = 'GuoDesk · 摊开这一叠'
$titlePeek = 'GuoDesk 叠放浮层'
if (Test-Path $run) { Remove-Item -Recurse -Force $run }
New-Item -ItemType Directory -Force -Path $data, $src | Out-Null
$noBom = New-Object System.Text.UTF8Encoding($false)
$names = @('peek-1.txt', 'peek-2.txt', 'peek-3.txt', 'peek-4.txt')
$big = @()
for ($i = 1; $i -le 26; $i++) { $big += ('big-' + $i + '.txt') }
foreach ($n in ($names + $big)) { [System.IO.File]::WriteAllText((Join-Path $src $n), $n, $noBom) }
$esc = ($src -replace '\\', '\\')
function Entry([string]$id, [string]$file, [string]$stack) {
  $q = [char]34
  return '{' + $q + 'id' + $q + ':' + $q + $id + $q + ',' + $q + 'path' + $q + ':' + $q + $esc + '\\' + $file + $q + ',' + $q + 'stack' + $q + ':' + $q + $stack + $q + '}'
}
$ents = @()
for ($i = 0; $i -lt 4; $i++) { $ents += (Entry ('p' + ($i + 1)) $names[$i] 's1') }
for ($i = 0; $i -lt 26; $i++) { $ents += (Entry ('q' + ($i + 1)) $big[$i] 's2') }
$layout = '{"version":1,"zones":[{"id":"zp","name":"摊开这一叠","x":150,"y":120,"width":460,"height":360,"collapsed":false,"viewMode":"grid","tileSize":2,"nameLines":2,"stacks":[{"id":"s1","name":"取件"},{"id":"s2","name":"大件"}],"entries":[' + ($ents -join ',') + ']}],"settings":{"theme":"system","compact":false,"language":"zh","guideDone":true,"labelStyle":0}}'
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
# 角标只是画在磁贴上的 Border，没有 InvokePattern，只能按矩形中心合成鼠标点击
function Open-Pile([string]$count) {
  $z = Find-Window $titleZone 2500
  $b = @(In-Window $z $CT::Text $count)
  if ($b.Count -lt 1) { return $false }
  $null = Focus-Window $z
  $null = Click-At $b[0].Current.BoundingRectangle
  return ($null -ne (Find-Window $titlePeek 5000))
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
Note 'both-badges-show-counts' (@(In-Window $zone $CT::Text '4').Count -ge 1 -and @(In-Window $zone $CT::Text '26').Count -ge 1) ''

# 1) 26 件的一叠：面板只能摊 25 格，标题必须承认是"前 25 项"
Note 'big-pile-opens' (Open-Pile '26') ''
$bigWin = Find-Window $titlePeek 2000
$head = @(In-Window $bigWin $CT::Text '*25*26*')
Note 'panel-admits-the-truncation' ($head.Count -ge 1) ('head=' + $(if ($head.Count) { $head[0].Current.Name } else { 'none' }))
$cells = 0
foreach ($n in $big) {
  $stem = $n.Substring(0, $n.Length - 4)
  if (@(In-Window $bigWin $CT::Text ($stem + '*')).Count -ge 1) { $cells++ }
}
Note 'panel-shows-exactly-25-cells' ($cells -eq 25) ('cells=' + $cells)
$null = Invoke-El (Button-Named (Find-Window $titlePeek 2000) '关闭')
Start-Sleep -Milliseconds 800
Note 'panel-closes-by-button' ($null -eq (Find-Window $titlePeek 1500)) ''

# 2) 成员被移走之后再点开：这一格要标出来，点它要说明"打不开"，程序必须活着
Remove-Item -LiteralPath (Join-Path $src 'big-3.txt') -Force
Note 'missing-pile-reopens' (Open-Pile '26') ''
$mw = Find-Window $titlePeek 2000
$warn = @(In-Window $mw $CT::Text '*big-3*')
Note 'missing-member-still-listed' ($warn.Count -ge 1) ('hits=' + $warn.Count)
if ($warn.Count -ge 1) { $null = Click-At $warn[0].Current.BoundingRectangle }
Start-Sleep -Milliseconds 1500
Note 'click-on-missing-survives' (-not $app.HasExited) ''
$mw2 = Find-Window $titlePeek 2500
Note 'panel-stays-open-to-explain' ($null -ne $mw2) ''
$explained = $false
foreach ($e in (In-Window $mw2 $CT::Text '*big-3.txt*')) {
  # 提示行换过内容：从"单击打开…"变成点名这个文件打不开
  if ($e.Current.Name -like '*「big-3.txt」*') { $explained = $true }
}
Note 'panel-explains-the-failure' $explained ''
$null = Invoke-El (Button-Named (Find-Window $titlePeek 2000) '关闭')
Start-Sleep -Milliseconds 800

Note 'app-still-running' (-not $app.HasExited) ''
Note 'zone-alive' ($null -ne (Find-Window $titleZone 2500)) ''

Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 600
$results | Set-Content -Path $report -Encoding UTF8
$fails = @($results | Where-Object { $_ -like 'FAIL*' })
Write-Output ('SUMMARY passed={0} failed={1}' -f ($results.Count - $fails.Count), $fails.Count)
