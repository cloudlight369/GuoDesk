# v3.15.0 E2E：映射分区拖入即落盘（默认复制 / Shift 移动 / 磁贴层放行 / 普通分区仍按引用）
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Windows.Forms

$AE = [System.Windows.Automation.AutomationElement]
$TS = [System.Windows.Automation.TreeScope]
$AC = [System.Windows.Automation.Condition]
$CT = [System.Windows.Automation.ControlType]

$root = 'D:\workspace\GuoDesk\artifacts'
$run = Join-Path $root 'e2e3150'
$data = Join-Path $run 'data'
$src = Join-Path $run 'gd3150_src'
$mList = Join-Path $run 'gd3150_list'
$mGrid = Join-Path $run 'gd3150_grid'
$exe = Join-Path $root 'Release\GuoDesk.exe'
$report = Join-Path $root 'e2e3150.txt'
$titleList = 'GuoDesk · 拖入列表'
$titleGrid = 'GuoDesk · 拖入磁贴'
$titleRef = 'GuoDesk · 拖入引用'
$expTitle = Split-Path $src -Leaf

# 关掉所有指向本用例产物目录的资源管理器窗口：它们会盖住拖放坐标，合成拖动可能从其导航窗格拖走整个文件夹，还会占住目录导致清理失败
$sh = New-Object -ComObject Shell.Application
foreach ($w in $sh.Windows()) {
  try {
    $p = $w.LocationFolder.Self.Path
    if ($p -and $p -like ($root + '\e2e*')) { $w.Quit() }
  } catch { }
}
Start-Sleep -Milliseconds 800
$removed = $false
for ($i = 0; $i -lt 10 -and -not $removed; $i++) {
  try { if (Test-Path $run) { Remove-Item -Recurse -Force $run }; $removed = $true }
  catch { Start-Sleep -Milliseconds 500 }
}
New-Item -ItemType Directory -Force -Path $data, $src, $mList, $mGrid | Out-Null

$noBom = New-Object System.Text.UTF8Encoding($false)
foreach ($n in 'drag_a.txt', 'drag_b.txt', 'drag_c.txt') {
  [System.IO.File]::WriteAllText((Join-Path $src $n), "payload for $n", $noBom)
}
[System.IO.File]::WriteAllText((Join-Path $mGrid 'existing.txt'), 'already here', $noBom)

$template = [ordered]@{
  version  = 1
  zones    = @(
    [ordered]@{ id = 'zl'; name = '拖入列表'; x = 980; y = 150; width = 380; height = 430; collapsed = $false; mappedFolder = $mList; viewMode = 'list'; entries = @() },
    [ordered]@{ id = 'zg'; name = '拖入磁贴'; x = 1400; y = 150; width = 420; height = 430; collapsed = $false; mappedFolder = $mGrid; viewMode = 'grid'; entries = @() },
    [ordered]@{ id = 'zr'; name = '拖入引用'; x = 980; y = 620; width = 380; height = 260; collapsed = $false; entries = @() }
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
 [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int ht, bool r);
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
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

# 在资源管理器窗口里按名称前缀找到文件行的屏幕矩形（首屏可能还在枚举，需要轮询）
function Item-Rect($win, [string]$like, [int]$ms) {
  $deadline = (Get-Date).AddMilliseconds($ms)
  while ((Get-Date) -lt $deadline) {
    # 优先取名称标签：整行的矩形在资源管理器里会横向偏移，按行取点会落到导航窗格上
    foreach ($kind in @($CT::Text, $CT::DataItem, $CT::ListItem, $CT::File)) {
      foreach ($e in $win.FindAll($TS::Descendants, (Kind-Cond $kind))) {
        if ($e.Current.Name -notlike $like) { continue }
        $r = $e.Current.BoundingRectangle
        if ($r.Width -le 20 -or $r.Height -le 8) { continue }
        if ($e.Current.IsOffscreen) { continue }
        return $r
      }
    }
    Start-Sleep -Milliseconds 350
  }
  return $null
}

function Names-Of($win) {
  $seen = New-Object System.Collections.Generic.List[string]
  foreach ($kind in @($CT::DataItem, $CT::ListItem, $CT::Text)) {
    foreach ($e in $win.FindAll($TS::Descendants, (Kind-Cond $kind))) {
      if (-not $e.Current.Name) { continue }
      $seen.Add($e.Current.Name) | Out-Null
      if ($seen.Count -ge 10) { break }
    }
    if ($seen.Count -ge 10) { break }
  }
  return ($seen -join ',')
}

function Drag-From-To($sx, $sy, $tx, $ty, [bool]$shift) {
  # 每次拖动前把源窗口提到最前，避免另一个同名窗口先接住鼠标按下
  $src0 = Get-Variable -Name expWin -ValueOnly -ErrorAction SilentlyContinue
  if ($null -ne $src0) { $null = Focus-Window $src0 }
  [void][U32]::SetCursorPos($sx, $sy)
  Start-Sleep -Milliseconds 160
  [void][U32]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 140
  [void][U32]::SetCursorPos($sx + 14, $sy + 8)
  Start-Sleep -Milliseconds 140
  [void][U32]::SetCursorPos($sx + 30, $sy + 16)
  Start-Sleep -Milliseconds 160
  if ($shift) { [void][U32]::keybd_event(0x10, 0, 0, [UIntPtr]::Zero) }
  $steps = 24
  for ($i = 1; $i -le $steps; $i++) {
    $x = [int]($sx + ($tx - $sx) * $i / $steps)
    $y = [int]($sy + ($ty - $sy) * $i / $steps)
    [void][U32]::SetCursorPos($x, $y)
    Start-Sleep -Milliseconds 45
  }
  [void][U32]::SetCursorPos($tx + 3, $ty + 2)
  Start-Sleep -Milliseconds 140
  [void][U32]::SetCursorPos($tx, $ty)
  Start-Sleep -Milliseconds 300
  [void][U32]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 200
  if ($shift) { [void][U32]::keybd_event(0x10, 0, 2, [UIntPtr]::Zero) }
}

function Wait-Leaf($dir, [string]$leaf, [bool]$expected, [int]$ms) {
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

$zoneL = Find-Window $titleList 9000
$zoneG = Find-Window $titleGrid 4000
$zoneR = Find-Window $titleRef 4000
Note 'zones-found' ($null -ne $zoneL -and $null -ne $zoneG -and $null -ne $zoneR) ('list=' + ($null -ne $zoneL) + ' grid=' + ($null -ne $zoneG) + ' ref=' + ($null -ne $zoneR))
if ($null -eq $zoneL -or $null -eq $zoneG -or $null -eq $zoneR) { $results | Set-Content -Path $report -Encoding UTF8; exit 1 }

# 上一轮或前一个版本用例遗留的资源管理器窗口会盖住拖放坐标，合成拖动会从它的导航窗格拖走整个文件夹，先全部关掉
$stale = $true
$clearDeadline = (Get-Date).AddSeconds(6)
while ($stale -and (Get-Date) -lt $clearDeadline) {
  $stale = $false
  foreach ($c in $AE::RootElement.FindAll($TS::Children, $AC::TrueCondition)) {
    if ($c.Current.Name -notmatch '^(src|gd3\d{3}_)') { continue }
    [void][U32]::SendMessage([IntPtr]$c.Current.NativeWindowHandle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
    $stale = $true
  }
  if ($stale) { Start-Sleep -Milliseconds 500 }
}
Start-Sleep -Milliseconds 900
$exp = Start-Process explorer.exe -ArgumentList $src -PassThru
$expWin = $null
$deadline = (Get-Date).AddSeconds(15)
while ((Get-Date) -lt $deadline) {
  foreach ($w in $AE::RootElement.FindAll($TS::Children, $AC::TrueCondition)) {
    if ($w.Current.Name -notlike ($expTitle + '*')) { continue }
    foreach ($li in $w.FindAll($TS::Descendants, (Kind-Cond $CT::ListItem))) {
      if ($li.Current.Name -like 'drag_a*') { $expWin = $w; break }
    }
    if ($null -ne $expWin) { break }
  }
  if ($null -ne $expWin) { break }
  Start-Sleep -Milliseconds 400
}
Note 'explorer-opened' ($null -ne $expWin) $expTitle
if ($null -eq $expWin) { $results | Set-Content -Path $report -Encoding UTF8; exit 1 }
[void][U32]::MoveWindow([IntPtr]$expWin.Current.NativeWindowHandle, 30, 150, 620, 430, $true)
Start-Sleep -Milliseconds 400
# 资源管理器启动时可能恢复上一轮的窗口，那些窗口更宽，会先接住合成的鼠标按下，导致拖走的是它的导航窗格节点
foreach ($c in $AE::RootElement.FindAll($TS::Children, $AC::TrueCondition)) {
  if ($c.Current.Name -notmatch '^(src|gd3\d{3}_)') { continue }
  if ([IntPtr]$c.Current.NativeWindowHandle -eq [IntPtr]$expWin.Current.NativeWindowHandle) { continue }
  [void][U32]::SendMessage([IntPtr]$c.Current.NativeWindowHandle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
}
Start-Sleep -Milliseconds 600
$null = Focus-Window $expWin

$rl = $zoneL.Current.BoundingRectangle
$rg = $zoneG.Current.BoundingRectangle
$rr = $zoneR.Current.BoundingRectangle
$dropList = @{ x = [int]($rl.X + $rl.Width / 2); y = [int]($rl.Y + $rl.Height * 0.55) }
$dropRef = @{ x = [int]($rr.X + $rr.Width / 2); y = [int]($rr.Y + $rr.Height * 0.5) }

# E1 默认拖入 = 复制进映射文件夹
$itA = Item-Rect $expWin 'drag_a*' 10000
Note 'item-a-found' ($null -ne $itA) ''
if ($null -ne $itA) {
  Drag-From-To ([int]($itA.X + $itA.Width / 2)) ([int]($itA.Y + $itA.Height / 2)) $dropList.x $dropList.y $false
  $copied = Wait-Leaf $mList 'drag_a.txt' $true 8000
  Note 'drop-copies-into-folder' ($copied -and (Test-Path -PathType Leaf (Join-Path $src 'drag_a.txt'))) ('srcKept=' + (Test-Path -PathType Leaf (Join-Path $src 'drag_a.txt')))
  Note 'copy-toast' (Wait-Text $zoneL '已复制' 6000) ''
  Note 'list-reflects-copy' (Wait-Text $zoneL 'drag_a\.txt' 4000) ''
}

# E2 磁贴分区的拖入要落在条目磁贴上（回归：子元素不得吞掉映射分区的拖放）
$itB = Item-Rect $expWin 'drag_b*' 5000
$tile = Item-Rect $zoneG 'existing*' 5000
Note 'tile-target-found' ($null -ne $itB -and $null -ne $tile) ('srcRect=' + $(if ($null -ne $itB) { $itB.ToString() } else { Names-Of $expWin }) + ' tileRect=' + $(if ($null -ne $tile) { $tile.ToString() } else { Names-Of $zoneG }))
if ($null -ne $itB -and $null -ne $tile) {
  Drag-From-To ([int]($itB.X + $itB.Width / 2)) ([int]($itB.Y + $itB.Height / 2)) ([int]($tile.X + $tile.Width / 2)) ([int]($tile.Y + $tile.Height / 2)) $false
  $okTile = Wait-Leaf $mGrid 'drag_b.txt' $true 8000
  $zoneG = Find-Window $titleGrid 3000
  Note 'drop-onto-tile-works' $okTile ('toast=' + ($(if ((Text-Of $zoneG) -match '(已复制|没有新增副本|复制未完成|失败)[^|]{0,24}') { $Matches[0] } else { 'none' })))
}

# E2b 同一磁贴分区改投空白处，用于区分“磁贴层吞事件”与“坐标未命中”
if (-not $okTile) {
  $itB2 = Item-Rect $expWin 'drag_b*' 3000
  if ($null -ne $itB2 -and (Test-Path -PathType Leaf (Join-Path $src 'drag_b.txt'))) {
    Drag-From-To ([int]($itB2.X + $itB2.Width / 2)) ([int]($itB2.Y + $itB2.Height / 2)) ([int]($rg.X + $rg.Width - 40)) ([int]($rg.Y + $rg.Height * 0.4)) $false
    $blank = Wait-Leaf $mGrid 'drag_b.txt' $true 8000
    Note 'drop-onto-blank-grid-works' $blank ('srcStill=' + (Test-Path -PathType Leaf (Join-Path $src 'drag_b.txt')))
  }
}

# E3 Shift 拖入 = 移动（源文件消失）
$itC = Item-Rect $expWin 'drag_c*' 5000
if ($null -ne $itC) {
  Drag-From-To ([int]($itC.X + $itC.Width / 2)) ([int]($itC.Y + $itC.Height / 2)) $dropList.x ([int]($dropList.y - 24)) $true
  $moved = Wait-Leaf $mList 'drag_c.txt' $true 8000
  Note 'shift-drop-moves' ($moved -and -not (Test-Path -PathType Leaf (Join-Path $src 'drag_c.txt'))) ('srcGone=' + (-not (Test-Path -PathType Leaf (Join-Path $src 'drag_c.txt'))))
}

# E4 普通分区仍是按引用添加，且不落盘
$zoneR = Find-Window $titleRef 3000
if (Test-Path -PathType Leaf (Join-Path $src 'drag_a.txt')) {
  $itA2 = Item-Rect $expWin 'drag_a*' 5000
  if ($null -ne $itA2) { Drag-From-To ([int]($itA2.X + $itA2.Width / 2)) ([int]($itA2.Y + $itA2.Height / 2)) $dropRef.x $dropRef.y $false }
  Start-Sleep -Milliseconds 1200
  $persisted = ([System.IO.File]::ReadAllText((Join-Path $data 'layout.json'))) -match 'drag_a\.txt'
  Note 'normal-zone-still-by-reference' ($persisted -and -not (Test-Path -PathType Leaf (Join-Path $mGrid 'drag_a.txt'))) ('persisted=' + $persisted)
}

# E5 落盘结果核对 + 窗口存活
Note 'mapped-content' ((Test-Path -PathType Leaf (Join-Path $mList 'drag_a.txt')) -and (Test-Path -PathType Leaf (Join-Path $mGrid 'drag_b.txt'))) (('list=' + (Get-ChildItem $mList -File -ErrorAction SilentlyContinue).Count) + ' grid=' + (Get-ChildItem $mGrid -File -ErrorAction SilentlyContinue).Count)
Note 'zones-alive' ($null -ne (Find-Window $titleList 2500) -and $null -ne (Find-Window $titleGrid 2500)) ''

$layoutNow = [System.IO.File]::ReadAllText((Join-Path $data 'layout.json'))
Note 'normal-zone-persisted' ($layoutNow -match 'drag_a\.txt') ''

foreach ($c in $AE::RootElement.FindAll($TS::Children, $AC::TrueCondition)) {
  if ($c.Current.Name -notmatch '^(src|gd3\d{3}_)') { continue }
  [void][U32]::SendMessage([IntPtr]$c.Current.NativeWindowHandle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
}
Start-Sleep -Milliseconds 500
Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 600
$results | Set-Content -Path $report -Encoding UTF8
