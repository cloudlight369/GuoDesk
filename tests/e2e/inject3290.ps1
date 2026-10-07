# v3.29.0 E2E：批量重命名要能预览、拦住不合规的名字，并且真的改到盘上
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Windows.Forms

$AE = [System.Windows.Automation.AutomationElement]
$TS = [System.Windows.Automation.TreeScope]
$AC = [System.Windows.Automation.Condition]
$CT = [System.Windows.Automation.ControlType]

$root = 'D:\workspace\GuoDesk\artifacts'
$run = Join-Path $root 'e2e3290'
$data = Join-Path $run 'data'
$src = Join-Path $run 'gd3290_src'
$exe = Join-Path $root 'Release\GuoDesk.exe'
$report = Join-Path $root 'e2e3290.txt'
$titleZone = 'GuoDesk · 批量重命名'
$defaultPattern = '{name}_{n}'

if (Test-Path $run) { Remove-Item -Recurse -Force $run }
New-Item -ItemType Directory -Force -Path $data, $src | Out-Null
$noBom = New-Object System.Text.UTF8Encoding($false)
$seed = @('a1.txt', 'a2.txt', 'b1.png', 'notes.md')
foreach ($n in $seed) { [System.IO.File]::WriteAllText((Join-Path $src $n), "body of $n", $noBom) }
$esc = ($src -replace '\\', '\\')
$layout = '{"version":1,"zones":[{"id":"zr","name":"批量重命名","x":180,"y":140,"width":460,"height":400,"collapsed":false,"viewMode":"list","nameLines":1,"mappedFolder":"' + $esc + '","browseInPlace":true,"entries":[]}],"settings":{"theme":"system","compact":false,"language":"zh","guideDone":true,"labelStyle":0}}'
[System.IO.File]::WriteAllText((Join-Path $data 'layout.json'), $layout, $noBom)

$results = New-Object System.Collections.Generic.List[string]
function Note([string]$name, [bool]$ok, [string]$detail) {
  $line = '{0} {1} {2}' -f ($(if ($ok) { 'PASS' } else { 'FAIL' })), $name, $detail
  Write-Output $line
  $results.Add($line) | Out-Null
}
function Bail {
  [System.IO.File]::WriteAllLines($report, $results, $noBom)
  Get-Process GuoDesk -ErrorAction SilentlyContinue | Stop-Process -Force
  exit 1
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
  Start-Sleep -Milliseconds 120
  [void][U32]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
  [void][U32]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 320
  return $true
}
function Invoke-El($e) {
  if ($null -eq $e) { return $false }
  try {
    $p = $e.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
    $p.Invoke()
    Start-Sleep -Milliseconds 240
    return $true
  } catch { }
  try { return (Click-At $e.Current.BoundingRectangle) } catch { return $false }
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
function Press-Esc {
  [void][U32]::keybd_event(0x1B, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 40
  [void][U32]::keybd_event(0x1B, 0, 2, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 300
}
function Send-CtrlA {
  [void][U32]::keybd_event(0x11, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 40
  [void][U32]::keybd_event(0x41, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 40
  [void][U32]::keybd_event(0x41, 0, 2, [UIntPtr]::Zero)
  [void][U32]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 450
}
function Get-Value($e) {
  try {
    $p = $e.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    return [string]$p.Current.Value
  } catch { return '' }
}
function Set-Value($e, [string]$text) {
  try {
    $p = $e.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    $p.SetValue($text)
    Start-Sleep -Milliseconds 500
    return $true
  } catch { return $false }
}
function Dialog-Edits {
  $zone = Find-Window $titleZone 2500
  if ($null -eq $zone) { return 99 }
  return @($zone.FindAll($TS::Descendants, (Kind-Cond $CT::Edit))).Count
}
# 焦点在 TextBox 里时 Esc 会被输入框吃掉，对话框根本不关；后面所有失败都是这一个原因引出来的。
# 显式点「取消」，再等 Edit 数目掉回只剩标题框那一个
function Dismiss-Dialog {
  $cancel = Menu-Item '取消' 1500
  if ($null -ne $cancel) { [void](Invoke-El $cancel) } else { Press-Esc }
  $deadline = (Get-Date).AddSeconds(4)
  while ((Get-Date) -lt $deadline) {
    if ((Dialog-Edits) -le 1) { return $true }
    Press-Esc
    Start-Sleep -Milliseconds 300
  }
  return ((Dialog-Edits) -le 1)
}
function Click-Named([string]$name, [bool]$ctrl) {
  $zone = Find-Window $titleZone 2500
  if ($null -eq $zone) { return $false }
  foreach ($e in $zone.FindAll($TS::Descendants, (Kind-Cond $CT::Text))) {
    if ($e.Current.Name -ne $name) { continue }
    if ($e.Current.IsOffscreen) { continue }
    if ($ctrl) { [void][U32]::keybd_event(0x11, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60 }
    [void](Click-At $e.Current.BoundingRectangle)
    if ($ctrl) { [void][U32]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero); Start-Sleep -Milliseconds 260 }
    return $true
  }
  return $false
}
function Texts($win) {
  $out = New-Object System.Collections.Generic.List[string]
  if ($null -eq $win) { return ,$out }
  foreach ($e in $win.FindAll($TS::Descendants, (Kind-Cond $CT::Text))) {
    if ($e.Current.IsOffscreen) { continue }
    $out.Add($e.Current.Name) | Out-Null
  }
  return ,$out
}
function Count-Text([string]$like) {
  $zone = Find-Window $titleZone 2500
  $n = 0
  foreach ($t in (Texts $zone)) { if ($t -like $like) { $n++ } }
  return $n
}
# 对话框里有三个 Edit（模板/起始号/补零位数），分区头部还有一个改名用的 Edit：
# 按"值是不是纯数字"把两个数字框摘出来，模板框按它自己的默认值认，别靠文档序数下标
function Dialog-Boxes {
  $zone = Find-Window $titleZone 4000
  if ($null -eq $zone) { return @{ pat = $null; start = $null; pad = $null; edits = 0 } }
  $pat = $null; $start = $null; $pad = $null
  $edits = @($zone.FindAll($TS::Descendants, (Kind-Cond $CT::Edit)))
  foreach ($e in $edits) {
    $v = Get-Value $e
    if ($v -eq $defaultPattern) { $pat = $e; continue }
    if ($v -match '^\d+$') {
      if ($null -eq $start) { $start = $e } else { $pad = $e }
    }
  }
  return @{ pat = $pat; start = $start; pad = $pad; edits = $edits.Count }
}

$app = Start-Process -FilePath $exe -ArgumentList @('--data-dir', $data) -PassThru
Start-Sleep -Seconds 6
$zone = Find-Window $titleZone 9000
Note 'zone-found' ($null -ne $zone) $titleZone
if ($null -eq $zone) { Bail }

$listed = (Get-ChildItem $src -File).Count
$shown = Count-Text '*.txt'
Note 'zone-lists-mapped-files' ($listed -eq 4 -and $shown -ge 2) ('disk=' + $listed + ' listed=' + $shown)

$first = $null
foreach ($t in $zone.FindAll($TS::Descendants, (Kind-Cond $CT::Text))) {
  if ($t.Current.Name -notlike '*.txt') { continue }
  if ($t.Current.IsOffscreen) { continue }
  $first = $t; break
}
if ($null -eq $first) { Note 'row-found' $false ''; Bail }
[void](Focus-Window $zone)
[void](Click-At $first.Current.BoundingRectangle)
Send-CtrlA
Note 'select-all-reports-four' ((Count-Text '已选 4*') -ge 1) ('status=' + (Count-Text '已选 4*'))

$more = More-Button (Find-Window $titleZone 2500)
if (-not (Invoke-El $more)) { Note 'menu-opened' $false ''; Bail }
$pick = Menu-Item '批量重命名选中项…' 4000
Note 'menu-offers-batch-rename' ($null -ne $pick) ''
if ($null -eq $pick) { Press-Esc; Bail }
[void](Invoke-El $pick)
Start-Sleep -Milliseconds 900

$b = Dialog-Boxes
Note 'dialog-boxes-found' (($null -ne $b.pat) -and ($null -ne $b.start) -and ($null -ne $b.pad)) ('edits=' + $b.edits)
$preview = Count-Text '*→*'
Note 'preview-lines-are-shown' ($preview -ge 4) ('rows=' + $preview)

if ($null -ne $b.pat) {
  [void](Set-Value $b.pat '同一份')
  $clash = Count-Text '*已经有别的文件在用*'
  $save = Menu-Item '保存' 2500
  $enabled = $true
  if ($null -ne $save) { $enabled = $save.Current.IsEnabled }
  Note 'collision-is-named' ($clash -ge 1) ('rows=' + $clash)
  Note 'save-is-held-back' ((-not $enabled) -and ($clash -ge 1)) ('enabled=' + $enabled)
}
$held = Dismiss-Dialog
Note 'dialog-dismisses-on-cancel' $held ('edits=' + (Dialog-Edits))
Note 'nothing-changed-when-held' (Test-Path (Join-Path $src 'a1.txt')) ''

# 只选两行：a1.txt 改叫 a2.txt 会撞上"没被选中的 a2.txt"。RenamePlan 只看这一批，
# 这种盘上冲突是 DeskWindow 里 markTaken 补的——不补就是预览说能改、点保存才发现改不了
[void](Click-Named 'a1.txt' $false)
[void](Click-Named 'b1.png' $true)
$pair = Find-Window $titleZone 2500
[void](Invoke-El (More-Button $pair))
$pick3 = Menu-Item '批量重命名选中项…' 4000
if ($null -eq $pick3) { Note 'menu-opens-for-pair' $false ''; Bail }
[void](Invoke-El $pick3)
Start-Sleep -Milliseconds 900
$b4 = Dialog-Boxes
Note 'pair-dialog-opened' ($null -ne $b4.pat) ('edits=' + $b4.edits)
[void](Set-Value $b4.pat 'a2')
$clash2 = Count-Text '*已经有别的文件在用*'
$save2 = Menu-Item '保存' 2500
$held2 = ($clash2 -ge 1) -and ($null -ne $save2) -and (-not $save2.Current.IsEnabled)
Note 'ondisk-clash-is-named' $held2 ('rows=' + $clash2 + ' enabled=' + $(if ($null -ne $save2) { $save2.Current.IsEnabled } else { 'no-button' }))
[void](Dismiss-Dialog)
Note 'pair-left-untouched' ((Test-Path (Join-Path $src 'a1.txt')) -and (Test-Path (Join-Path $src 'a2.txt'))) ''

# 上面那一段故意只选两行，这里要把四个都选回来再验证"干净模板真的落盘"
[void](Click-Named 'a1.txt' $false)
Send-CtrlA
$more2 = More-Button (Find-Window $titleZone 2500)
[void](Invoke-El $more2)
$pick2 = Menu-Item '批量重命名选中项…' 4000
if ($null -eq $pick2) { Note 'menu-reopened' $false ''; Bail }
[void](Invoke-El $pick2)
Start-Sleep -Milliseconds 900
$b3 = Dialog-Boxes
Note 'dialog-reopened' (($null -ne $b3.pat) -and ($null -ne $b3.start) -and ($null -ne $b3.pad)) ('edits=' + $b3.edits)
[void](Set-Value $b3.pat '归档_{n}')
[void](Set-Value $b3.start '1')
[void](Set-Value $b3.pad '3')
$bad = Count-Text '*已经有别的文件在用*'
$go = Menu-Item '保存' 2500
$ready = ($bad -eq 0) -and ($null -ne $go) -and $go.Current.IsEnabled
Note 'clean-pattern-enables-save' $ready ('bad=' + $bad + ' found=' + ($null -ne $go))
if (-not $ready) { Press-Esc; Bail }
[void](Invoke-El $go)
Start-Sleep -Milliseconds 1800

$names = @((Get-ChildItem $src -File | ForEach-Object { $_.Name }) | Sort-Object)
$want = @('归档_001.txt', '归档_002.txt', '归档_003.png', '归档_004.md')
$hit = 0
foreach ($w in $want) { if ($names -contains $w) { $hit++ } }
Note 'disk-holds-new-names' ($hit -eq 4) ('got=' + ($names -join ','))
$gone = 0
foreach ($s in $seed) { if (Test-Path (Join-Path $src $s)) { $gone++ } }
Note 'old-names-are-gone' ($gone -eq 0) ('left=' + $gone)
Note 'zone-reflects-rename' ((Count-Text '归档_*') -ge 4) ('rows=' + (Count-Text '归档_*'))
Note 'app-still-running' (@(Get-Process GuoDesk -ErrorAction SilentlyContinue).Count -ge 1) ''
Note 'zone-alive' ($null -ne (Find-Window $titleZone 2500)) ''

Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 600
[System.IO.File]::WriteAllLines($report, $results, $noBom)
