# v3.14.0 E2E：空格快速预览翻页/步进、异步粘贴进度与取消、删除到回收站、保留设备名重命名
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Windows.Forms

$AE = [System.Windows.Automation.AutomationElement]
$TS = [System.Windows.Automation.TreeScope]
$AC = [System.Windows.Automation.Condition]
$CT = [System.Windows.Automation.ControlType]

$root = 'D:\workspace\GuoDesk\artifacts'
$run = Join-Path $root 'e2e3140'
$data = Join-Path $run 'data'
$src = Join-Path $run 'src'
$tgt = Join-Path $run 'target'
$exe = Join-Path $root 'Release\GuoDesk.exe'
$report = Join-Path $root 'e2e3140.txt'
$zoneTitle = 'GuoDesk · 预览测试'
$prevTitle = 'GuoDesk 快速预览'
$bulk = 400

if (Test-Path $run) { Remove-Item -Recurse -Force $run }
New-Item -ItemType Directory -Force -Path $data, $src, $tgt | Out-Null

$noBom = New-Object System.Text.UTF8Encoding($false)
$long = (@(1..220 | ForEach-Object { "line $_ : the quick brown fox jumps over the lazy dog 0123456789" }) -join "`r`n")
[System.IO.File]::WriteAllText((Join-Path $tgt 'a_long.txt'), $long, $noBom)
[System.IO.File]::WriteAllText((Join-Path $tgt 'b.txt'), 'second entry for stepping', $noBom)
[System.IO.File]::WriteAllText((Join-Path $tgt 'c.txt'), 'third entry', $noBom)
New-Item -ItemType Directory -Force -Path (Join-Path $tgt 'sub') | Out-Null
[System.IO.File]::WriteAllText((Join-Path $tgt 'sub\inner.txt'), 'inner', $noBom)
foreach ($i in 1..$bulk) { [System.IO.File]::WriteAllText((Join-Path $src ("bulk{0:d3}.txt" -f $i)), "bulk $i", $noBom) }

$template = [ordered]@{
  version  = 1
  zones    = @([ordered]@{ id = 'z3140'; name = '预览测试'; x = 260; y = 200; width = 380; height = 520; collapsed = $false; mappedFolder = $tgt; viewMode = 'list'; entries = @() })
  settings = [ordered]@{ theme = 'system'; compact = $false; language = 'zh'; guideDone = $true; snapshots = $false }
}
[System.IO.File]::WriteAllText((Join-Path $data 'layout.json'), ($template | ConvertTo-Json -Depth 8), $noBom)

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

function Find-Named-Kind($scope, $kind, [string]$name, [int]$ms) {
  $cond = [System.Windows.Automation.PropertyCondition]::new($kind, $name)
  $deadline = (Get-Date).AddMilliseconds($ms)
  while ((Get-Date) -lt $deadline) {
    $hit = $scope.FindFirst($TS::Descendants, $cond)
    if ($null -ne $hit) { return $hit }
    Start-Sleep -Milliseconds 120
  }
  return $null
}

function Any-Kind($scope, $kind, [string]$name) {
  foreach ($e in $scope.FindAll($TS::Descendants, (Kind-Cond $kind))) {
    if ($null -eq $name -or $e.Current.Name -eq $name) { return $e }
  }
  return $null
}

function Visible-Kind($scope, $kind, [string]$name) {
  foreach ($e in $scope.FindAll($TS::Descendants, (Kind-Cond $kind))) {
    if ($e.Current.IsOffscreen) { continue }
    if ($null -eq $name -or $e.Current.Name -eq $name) { return $true }
  }
  return $false
}

function Text-Of($el) {
  if ($null -eq $el) { return '' }
  $sb = New-Object System.Text.StringBuilder
  foreach ($e in $el.FindAll($TS::Descendants, $AC::TrueCondition)) { [void]$sb.Append('|').Append($e.Current.Name) }
  return $sb.ToString()
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
    Start-Sleep -Milliseconds 80
    if ([U32]::GetForegroundWindow() -eq $h) { Start-Sleep -Milliseconds 150; return $true }
    Start-Sleep -Milliseconds 60
  }
  return $false
}

function Click-FirstRow($w) {
  foreach ($e in $w.FindAll($TS::Descendants, (Kind-Cond $CT::Text))) {
    $n = $e.Current.Name
    if (-not $n -or $n -match '[\\|]' -or $n.Length -gt 60) { continue }
    if (-not (Test-Path -PathType Leaf (Join-Path $tgt $n))) { continue }
    if ($e.Current.IsOffscreen) { continue }
    $r = $e.Current.BoundingRectangle
    if ($r.Width -le 0 -or $r.Height -le 0) { continue }
    [void][U32]::SetCursorPos([int]($r.X + $r.Width / 2), [int]($r.Y + $r.Height / 2))
    Start-Sleep -Milliseconds 80
    [void][U32]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    [void][U32]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 350
    return $n
  }
  return $null
}

function Click-Row($w, [string]$name) {
  $hit = $null
  foreach ($e in $w.FindAll($TS::Descendants, (Kind-Cond $CT::Text))) {
    if ($e.Current.Name -eq $name) { $hit = $e; break }
  }
  if ($null -eq $hit) { return $false }
  $r = $hit.Current.BoundingRectangle
  if ($r.Width -le 0 -or $r.Height -le 0) { return $false }
  [void][U32]::SetCursorPos([int]($r.X + $r.Width / 2), [int]($r.Y + $r.Height / 2))
  Start-Sleep -Milliseconds 80
  [void][U32]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
  [void][U32]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 350
  return $true
}

function Pick-Row([ref]$w, [int]$ms) {
  $deadline = (Get-Date).AddMilliseconds($ms)
  while ((Get-Date) -lt $deadline) {
    $z = Find-Window $zoneTitle 2000
    if ($null -ne $z) {
      $w.Value = $z
      $null = Focus-Window $z
      $n = Click-FirstRow $z
      if ($n) { return $n }
    }
    Start-Sleep -Milliseconds 400
  }
  return $null
}

Start-Process -FilePath $exe -ArgumentList @('--data-dir', $data)
Start-Sleep -Seconds 3

$zone = Find-Window $zoneTitle 9000
Note 'zone-window' ($null -ne $zone) $zoneTitle
if ($null -eq $zone) { $results | Set-Content -Path $report -Encoding UTF8; exit 1 }
$null = Focus-Window $zone

# E1 点击首行 -> Space 打开预览
$clicked = Click-Row $zone 'a_long.txt'
[System.Windows.Forms.SendKeys]::SendWait(' '); Start-Sleep -Milliseconds 900
$prev = Find-Window $prevTitle 6000
Note 'preview-open' ($null -ne $prev) ('rowClicked=' + $clicked)
if ($null -ne $prev) {
  $t = Text-Of $prev
  Note 'preview-content' ($t.Length -gt 0) ('len=' + $t.Length)
  $null = Focus-Window $prev
  [System.Windows.Forms.SendKeys]::SendWait('{PGDN}'); Start-Sleep -Milliseconds 250
  [System.Windows.Forms.SendKeys]::SendWait('{PGDN}'); Start-Sleep -Milliseconds 250
  [System.Windows.Forms.SendKeys]::SendWait('{PGUP}'); Start-Sleep -Milliseconds 250
  Note 'preview-paging' ($null -ne (Find-Window $prevTitle 1500)) ''
  [System.Windows.Forms.SendKeys]::SendWait('{DOWN}'); Start-Sleep -Milliseconds 900
  $p2 = Find-Window $prevTitle 1500
  Note 'preview-step' ($null -ne $p2 -and (Text-Of $p2) -ne $t) ''
  [System.Windows.Forms.SendKeys]::SendWait('{ESC}'); Start-Sleep -Milliseconds 800
  Note 'preview-close' ($null -eq (Find-Window $prevTitle 1200)) ''
  Note 'zone-alive-after-preview' ($null -ne (Find-Window $zoneTitle 2000)) ''
}

# E5 异步粘贴：CF_HDROP 剪贴板 -> 映射分区，进度/取消条可见
$files = @(Get-ChildItem $src -File | ForEach-Object { $_.FullName })
$coll = New-Object System.Collections.Specialized.StringCollection
foreach ($f in $files) { [void]$coll.Add($f) }
[System.Windows.Forms.Clipboard]::SetFileDropList($coll)
Start-Sleep -Milliseconds 250
$zone = Find-Window $zoneTitle 3000
$act = Focus-Window $zone
$null = Click-Row $zone 'b.txt'
[System.Windows.Forms.SendKeys]::SendWait('^v')
$seenProgress = $false
$deadline = (Get-Date).AddSeconds(12)
while ((Get-Date) -lt $deadline) {
  if ((Visible-Kind $zone $CT::ProgressBar $null) -or (Visible-Kind $zone $CT::Button '取消')) { $seenProgress = $true; break }
  Start-Sleep -Milliseconds 10
}
Note 'paste-progress-visible' $seenProgress ('clipboard=' + $files.Count + ' active=' + $act)
[System.Windows.Forms.SendKeys]::SendWait('^v'); Start-Sleep -Milliseconds 500
$want = 3 + $bulk
$copied = @(Get-ChildItem $tgt -File).Count
$deadline = (Get-Date).AddSeconds(120)
while ($copied -lt $want -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 400; $copied = @(Get-ChildItem $tgt -File).Count }
Note 'paste-result' ($copied -eq $want) ('files=' + $copied + ' want=' + $want)
$zone = Find-Window $zoneTitle 4000
Note 'zone-alive-after-paste' ($null -ne $zone) ''

# E6 选中一个可见行 -> Delete 到回收站 -> 确认框 -> 该文件消失、只少一项
$zone = Find-Window $zoneTitle 3000
$rowName = Pick-Row ([ref]$zone) 15000
$before = @(Get-ChildItem $tgt -File).Count
[System.Windows.Forms.SendKeys]::SendWait('{DELETE}')
$dlgBtn = $null
$deadline = (Get-Date).AddSeconds(8)
while ((Get-Date) -lt $deadline) {
  foreach ($e in $zone.FindAll($TS::Descendants, (Kind-Cond $CT::Button))) {
    if ($e.Current.Name -eq '移到回收站' -and -not $e.Current.IsOffscreen) { $dlgBtn = $e; break }
  }
  if ($null -ne $dlgBtn) { break }
  Start-Sleep -Milliseconds 150
}
Note 'delete-confirm-dialog' ($null -ne $dlgBtn) ('row=' + $rowName)
if ($null -ne $dlgBtn) {
  $inv = $dlgBtn.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
  $inv.Invoke()
  Start-Sleep -Milliseconds 2500
  $after = @(Get-ChildItem $tgt -File).Count
  Note 'delete-result' ($rowName -and (-not (Test-Path (Join-Path $tgt $rowName))) -and ($after -eq ($before - 1))) ('files=' + $after + ' before=' + $before)
  $z = Find-Window $zoneTitle 3000
  Note 'zone-alive-after-delete' ($null -ne $z) ''
  $zone = $z
}

# E7 保留设备名重命名应被拒绝
if ($null -ne $zone) {
  $null = Focus-Window $zone
  $rowName = Pick-Row ([ref]$zone) 12000
  $before = @(Get-ChildItem $tgt -File).Count
  [System.Windows.Forms.SendKeys]::SendWait('{F2}'); Start-Sleep -Milliseconds 900
  $ed = $null
  foreach ($e in $zone.FindAll($TS::Descendants, (Kind-Cond $CT::Edit))) {
    if ($e.Current.Name -ne '分区名称' -and -not $e.Current.IsOffscreen) { $ed = $e; break }
  }
  if ($null -ne $ed) {
    $vp = $ed.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    $vp.SetValue('nul.txt')
    Start-Sleep -Milliseconds 200
    [System.Windows.Forms.SendKeys]::SendWait('{ENTER}'); Start-Sleep -Milliseconds 1400
    $still = @(Get-ChildItem $tgt -File).Count
    Note 'rename-reserved-rejected' ((Test-Path (Join-Path $tgt $rowName)) -and ($still -eq $before)) ('row=' + $rowName + ' files=' + $still + ' before=' + $before)
    Note 'zone-alive-after-rename' ($null -ne (Find-Window $zoneTitle 2500)) ''
  }
  else { Note 'rename-editor-found' $false ('row=' + $rowName) }
}

Get-Process GuoDesk -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
$results | Set-Content -Path $report -Encoding UTF8
$fails = @($results | Where-Object { $_ -like 'FAIL*' })
Write-Output ('SUMMARY passed={0} failed={1}' -f ($results.Count - $fails.Count), $fails.Count)
