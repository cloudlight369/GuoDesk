# v3.16.0 E2E：映射分区「按规则归档」预览 + 真实移动到分类子文件夹 + 未映射/无匹配守卫
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Windows.Forms

$AE = [System.Windows.Automation.AutomationElement]
$TS = [System.Windows.Automation.TreeScope]
$AC = [System.Windows.Automation.Condition]
$CT = [System.Windows.Automation.ControlType]

$root = 'D:\workspace\GuoDesk\artifacts'
$run = Join-Path $root 'e2e3160'
$data = Join-Path $run 'data'
$arc = Join-Path $run 'gd3160_arc'
$arc2 = Join-Path $run 'gd3160_clash'
$plain = Join-Path $run 'gd3160_plain'
$exe = Join-Path $root 'Release\GuoDesk.exe'
$report = Join-Path $root 'e2e3160.txt'
$titleArc = 'GuoDesk · 归档测试'
$titleClash = 'GuoDesk · 归档冲突'
$titlePlain = 'GuoDesk · 未映射'
$archiveItem = '按规则归档此文件夹…'

if (Test-Path $run) { Remove-Item -Recurse -Force $run }
New-Item -ItemType Directory -Force -Path $data, $arc, $plain, (Join-Path $arc2 '文档') | Out-Null

$noBom = New-Object System.Text.UTF8Encoding($false)
foreach ($n in 'doc1.pdf', 'doc2.pdf', 'photo.png', 'keep.txt') {
  [System.IO.File]::WriteAllText((Join-Path $arc $n), "payload for $n", $noBom)
}
[System.IO.File]::WriteAllText((Join-Path $plain 'loose.txt'), 'nothing mapped', $noBom)
# 冲突分区：分类里已有同名文件（不得改成 "doc1 (2).pdf" 副本），根目录里还有个和分类同名的散文件（整组必须跳过）
foreach ($n in 'doc1.pdf', 'doc2.pdf', 'photo.png') {
  [System.IO.File]::WriteAllText((Join-Path $arc2 $n), "clash payload for $n", $noBom)
}
[System.IO.File]::WriteAllText((Join-Path $arc2 '图片'), 'a file named like a category', $noBom)
[System.IO.File]::WriteAllText((Join-Path $arc2 '文档\doc1.pdf'), 'pre-existing original', $noBom)

$template = [ordered]@{
  version = 1
  zones   = @(
    [ordered]@{ id = 'za'; name = '归档测试'; x = 980; y = 150; width = 400; height = 460; collapsed = $false; mappedFolder = $arc; viewMode = 'list'; entries = @() },
    [ordered]@{ id = 'zc'; name = '归档冲突'; x = 1420; y = 150; width = 400; height = 400; collapsed = $false; mappedFolder = $arc2; viewMode = 'list'; entries = @() },
    [ordered]@{ id = 'zb'; name = '未映射'; x = 980; y = 640; width = 380; height = 260; collapsed = $false; entries = @() }
  )
  rules   = @(
    [ordered]@{ id = 'r3160a'; name = '文档'; exts = @('pdf'); keywords = @(); zone = ''; minSize = 0; maxSize = 0; olderThan = 0 },
    [ordered]@{ id = 'r3160b'; name = '图片'; exts = @(); keywords = @('photo'); zone = ''; minSize = 0; maxSize = 0; olderThan = 0 }
  )
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

function Click-At($r) {
  $x = [int]($r.X + $r.Width / 2); $y = [int]($r.Y + $r.Height / 2)
  [void][U32]::SetCursorPos($x, $y)
  Start-Sleep -Milliseconds 140
  [void][U32]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 70
  [void][U32]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 200
}

function Invoke-El($e) {
  if ($null -eq $e) { return $false }
  try {
    $p = $e.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
    $p.Invoke()
    Start-Sleep -Milliseconds 200
    return $true
  } catch { }
  try { Click-At $e.Current.BoundingRectangle; return $true } catch { return $false }
}

function Type-Into($e, [string]$v) {
  try {
    $r = $e.Current.BoundingRectangle
    if ($r.Width -le 2) { return $false }
  } catch { return $false }
  Click-At $e.Current.BoundingRectangle
  Start-Sleep -Milliseconds 200
  try {
    [System.Windows.Forms.SendKeys]::SendWait($v)
    Start-Sleep -Milliseconds 160
    return $true
  } catch { return $false }
}

# WinUI 的 TextBox 在不同 SDK 上暴露的 ValuePattern 方法名不一样，逐个试探，最后退化成点击+键入
function Set-Value($e, [string]$v) {
  if ($null -eq $e) { return $false }
  foreach ($m in @('SetValue', 'SetProgrammaticValue')) {
    try {
      $p = $e.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
      $p.$m($v)
      Start-Sleep -Milliseconds 120
      return $true
    } catch { }
  }
  try {
    $q = $e.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    $q.Value = $v
    Start-Sleep -Milliseconds 120
    return $true
  } catch { }
  return (Type-Into $e $v)
}

# 分区标题栏最右边的那个动作按钮就是「更多操作」，它没有可读的自动化名称，只能按位置认
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

function Menu-Item([string]$name, [int]$ms) {
  $deadline = (Get-Date).AddMilliseconds($ms)
  while ((Get-Date) -lt $deadline) {
    foreach ($k in @($CT::ListItem, $CT::MenuItem, $CT::Button)) {
      foreach ($w in $AE::RootElement.FindAll($TS::Children, $AC::TrueCondition)) {
        foreach ($e in $w.FindAll($TS::Descendants, (Kind-Cond $k))) {
          if ($e.Current.Name -ne $name) { continue }
          if ($e.Current.IsOffscreen) { continue }
          return $e
        }
      }
    }
    Start-Sleep -Milliseconds 200
  }
  return $null
}

function Button-Like($win, [string]$like, [int]$ms) {
  $deadline = (Get-Date).AddMilliseconds($ms)
  while ((Get-Date) -lt $deadline) {
    $fallback = $null
    foreach ($e in $win.FindAll($TS::Descendants, (Kind-Cond $CT::Button))) {
      if ($e.Current.Name -notlike $like) { continue }
      if (-not $e.Current.IsOffscreen) { return $e }
      if ($null -eq $fallback) { $fallback = $e }
    }
    if ($null -ne $fallback) { return $fallback }
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

# 上一轮的菜单项元素在弹层关闭后就作废了，直接 Invoke 只会点到空白处，所以每次都要先收起菜单再重新打开
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

function Wait-Notify($win, [string]$pattern, [int]$ms) {
  $deadline = (Get-Date).AddMilliseconds($ms)
  while ((Get-Date) -lt $deadline) {
    if ((Text-Of $win) -match $pattern) { return $true }
    Start-Sleep -Milliseconds 200
  }
  return $false
}

function Wait-Leaf([string]$dir, [string]$leaf, [bool]$expected, [int]$ms) {
  $p = Join-Path $dir $leaf
  $deadline = (Get-Date).AddMilliseconds($ms)
  while ((Get-Date) -lt $deadline) {
    if ((Test-Path -PathType Leaf $p) -eq $expected) { return $true }
    Start-Sleep -Milliseconds 200
  }
  return (Test-Path -PathType Leaf $p) -eq $expected
}

function Wait-Text($win, [string]$pattern, [int]$ms) {
  $deadline = (Get-Date).AddMilliseconds($ms)
  while ((Get-Date) -lt $deadline) {
    if ((Text-Of $win) -match $pattern) { return $true }
    Start-Sleep -Milliseconds 250
  }
  return $false
}

$app = Start-Process -FilePath $exe -ArgumentList @('--data-dir', $data) -PassThru
Start-Sleep -Seconds 3

$zoneA = Find-Window $titleArc 9000
$zoneB = Find-Window $titlePlain 4000
Note 'zones-found' ($null -ne $zoneA -and $null -ne $zoneB) ('arc=' + ($null -ne $zoneA) + ' plain=' + ($null -ne $zoneB))
if ($null -eq $zoneA -or $null -eq $zoneB) { $results | Set-Content -Path $report -Encoding UTF8; exit 1 }

# A1 归档预览：分类行与「移动 N 个文件」按钮都要出现，默认按钮必须是取消
$opened = Open-And-Pick $zoneA $archiveItem
Note 'menu-item-present' $opened ''
if (-not $opened) { $results | Set-Content -Path $report -Encoding UTF8; exit 1 }
$primary = Button-Like $zoneA '移动 *' 6000
$preview = Text-Of $zoneA
Note 'preview-groups' ($null -ne $primary -and $preview -match '文档' -and $preview -match '图片') ('button=' + $(if ($null -ne $primary) { $primary.Current.Name } else { 'none' }))
Note 'preview-keeps-unmatched' ($preview -match 'keep\.txt|不匹配规则') ''

# A2 确认归档：文件被移入分类子文件夹，根目录不再留有同名散文件
if ($null -ne $primary) {
  [void](Invoke-El $primary)
  $got = ''
  $spin = (Get-Date).AddSeconds(14)
  while ((Get-Date) -lt $spin) {
    $t = Text-Of (Find-Window $titleArc 1500)
    if ($t -match '(正在归档 \d+ 项|已把[^|]*归档到[^|]*|归档未完成[^|]*|没有文件被移动[^|]*)[^|]{0,30}') {
      $got = $Matches[0]
      if ($got -notmatch '正在归档') { break }
    }
    Start-Sleep -Milliseconds 120
  }
  $moved = (Wait-Leaf $arc 'doc1.pdf' $false 9000) -and (Wait-Leaf $arc '文档\doc1.pdf' $true 9000)
  $moved = $moved -and (Wait-Leaf $arc '文档\doc2.pdf' $true 9000) -and (Wait-Leaf $arc '图片\photo.png' $true 9000)
  Note 'archive-moves-by-rule' ($moved -and (Test-Path -PathType Leaf (Join-Path $arc 'keep.txt'))) ('keepStill=' + (Test-Path -PathType Leaf (Join-Path $arc 'keep.txt')))
  Note 'archive-toast' ($got -match '已把 3 个文件归档到 2 个分类文件夹') ('saw=' + $got)
  Note 'archive-content' ((Get-ChildItem (Join-Path $arc '文档') -File -ErrorAction SilentlyContinue).Count -eq 2 -and (Get-ChildItem (Join-Path $arc '图片') -File -ErrorAction SilentlyContinue).Count -eq 1) ''
}

# A3 再点一次：只剩不匹配的散文件，应当直接提示而不是弹出空对话框
$zoneA = Find-Window $titleArc 4000
$picked = Open-And-Pick $zoneA $archiveItem
$again = $null
if ($picked) { $null = Wait-Notify $zoneA '都没匹配上规则' 6000 }
$again = Text-Of (Find-Window $titleArc 2500)
if ($again -notmatch '都没匹配上规则') {
  $picked = Open-And-Pick $zoneA $archiveItem
  if ($picked) { $null = Wait-Notify $zoneA '都没匹配上规则' 6000 }
  $again = Text-Of (Find-Window $titleArc 2500)
}
Note 'no-match-guard' ($again -match '都没匹配上规则') ('picked=' + $picked + ' toast=' + $(if ($again -match '(1 个文件都没匹配上规则)[^|]{0,20}') { $Matches[0] } else { 'none' }))
Note 'no-dialog-on-empty' (-not ((Button-Like $zoneA '移动 *' 400))) ''

# A4 未映射分区：菜单项存在但只提示，不动任何文件
$zoneB = Find-Window $titlePlain 4000
$pickedB = Open-And-Pick $zoneB $archiveItem
Note 'plain-menu-item' $pickedB ''
if ($pickedB) {
  $zoneB = Find-Window $titlePlain 3000
  Note 'plain-guard-toast' (Wait-Notify $zoneB '没有映射文件夹' 6000) ''
  Note 'plain-files-untouched' (Test-Path -PathType Leaf (Join-Path $plain 'loose.txt')) ''
}

# A5 设置页规则编辑器：新增带尺寸/时间门槛的规则（回归：新控件不得让对话框打不开）
$zoneA = Find-Window $titleArc 4000
$openedSettings = Open-And-Pick $zoneA '设置'
Note 'settings-menu-item' $openedSettings ''
if ($openedSettings) {
  $setWin = Find-Window 'GuoDesk 设置' 9000
  Note 'settings-window' ($null -ne $setWin) ''
  if ($null -ne $setWin) {
    $addBtn = Button-Like $setWin '添加规则' 6000
    Note 'add-rule-button' ($null -ne $addBtn) ''
    $before = New-Object System.Collections.Generic.List[string]
    foreach ($e in $setWin.FindAll($TS::Descendants, (Kind-Cond $CT::Edit))) { $before.Add(($e.GetRuntimeId() -join ',')) }
    if ($null -ne $addBtn) { [void](Invoke-El $addBtn) }
    $fields = $null
    $spin = (Get-Date).AddSeconds(8)
    while ((Get-Date) -lt $spin) {
      $new = @($setWin.FindAll($TS::Descendants, (Kind-Cond $CT::Edit)) | Where-Object { -not $before.Contains(($_.GetRuntimeId() -join ',')) })
      if ($new.Count -ge 6) { $fields = $new; break }
      Start-Sleep -Milliseconds 250
    }
    Note 'rule-dialog-fields' ($null -ne $fields) ('newEdits=' + $(if ($null -ne $fields) { $fields.Count } else { 0 }))
    if ($null -ne $fields) {
      $want = @( @(0, 'GateRule'), @(1, 'zip'), @(3, '1024'), @(5, '30') )
      $bad = @()
      foreach ($w in $want) { if (-not (Set-Value $fields[$w[0]] $w[1])) { $bad += $w[0] } }
      Note 'gate-fields-fill' ($bad.Count -eq 0) ('failedIdx=' + ($bad -join ','))
      $save = Button-Like $setWin '保存' 5000
      Note 'rule-save-button' ($null -ne $save) ''
      if ($null -ne $save) { [void](Invoke-El $save) }
      $shown = Wait-Notify $setWin '不小于 1024 KB' 6000
      $summ = Text-Of $setWin
      Note 'gate-summary-shown' $shown ('row=' + $(if ($summ -match '(GateRule[^|]{0,60})') { $Matches[0] } elseif ($summ -match '(不小于[^|]{0,20})') { $Matches[0] } else { 'none' }))
      Start-Sleep -Milliseconds 900
      $json = [System.IO.File]::ReadAllText((Join-Path $data 'layout.json'))
      Note 'gate-persisted' ($json -match '"minSize":\s*1024' -and $json -match '"olderThan":\s*30') ('min=' + $(if ($json -match '"minSize":(\d+)') { $Matches[1] } else { 'x' }))
    }
  }
}

# C 冲突归档：分类里已有同名文件不能改成副本；根目录里和分类同名的文件要让整组跳过
$zoneC = Find-Window $titleClash 4000
Note 'clash-zone-found' ($null -ne $zoneC) ''
if ($null -ne $zoneC) {
  $openedC = Open-And-Pick $zoneC $archiveItem
  $primaryC = Button-Like $zoneC '移动 *' 6000
  Note 'clash-preview' ($openedC -and $null -ne $primaryC) ('button=' + $(if ($null -ne $primaryC) { $primaryC.Current.Name } else { 'none' }))
  if ($null -ne $primaryC) {
    [void](Invoke-El $primaryC)
    $gotC = ''
    $spin = (Get-Date).AddSeconds(16)
    while ((Get-Date) -lt $spin) {
      $t = Text-Of (Find-Window $titleClash 1500)
      if ($t -match '(正在归档 \d+ 项|已把[^|]*归档到[^|]*|归档未完成[^|]*|没有文件被移动[^|]*)[^|]{0,140}') { $gotC = $Matches[0]; if ($gotC -notmatch '正在归档') { break } }
      Start-Sleep -Milliseconds 120
    }
    $doc1Kept = (Test-Path -PathType Leaf (Join-Path $arc2 'doc1.pdf'))
    $original = ''
    try { $original = [System.IO.File]::ReadAllText((Join-Path $arc2 '文档\doc1.pdf')) } catch { }
    $dup = @(Get-ChildItem (Join-Path $arc2 '文档') -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -like 'doc1 (2)*' })
    Note 'clash-no-duplicate' ($doc1Kept -and $original -eq 'pre-existing original' -and $dup.Count -eq 0) ('rootKept=' + $doc1Kept + ' dup=' + $dup.Count)
    Note 'clash-category-file-blocked' ((Test-Path -PathType Leaf (Join-Path $arc2 '图片')) -and (Test-Path -PathType Leaf (Join-Path $arc2 'photo.png'))) ('stillFile=' + (Test-Path -PathType Leaf (Join-Path $arc2 '图片')))
    Note 'clash-others-moved' ((Test-Path -PathType Leaf (Join-Path $arc2 '文档\doc2.pdf')) -and -not (Test-Path -PathType Leaf (Join-Path $arc2 'doc2.pdf'))) ''
    Note 'clash-toast-honest' ($gotC -match '已把 1 个文件归档到 1 个分类文件夹' -and $gotC -match '跳过 1 项' -and $gotC -match '项没动') ('saw=' + $gotC)
  }
}

Note 'zones-alive' ($null -ne (Find-Window $titleArc 2500) -and $null -ne (Find-Window $titlePlain 2500)) ''

Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 600
$results | Set-Content -Path $report -Encoding UTF8
$fails = @($results | Where-Object { $_ -like 'FAIL*' })
Write-Output ('SUMMARY passed={0} failed={1}' -f ($results.Count - $fails.Count), $fails.Count)
