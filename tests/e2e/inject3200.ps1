# v3.20.0 E2E：归档规则按分区生效（甲分区的规则不得整理乙分区的文件）+ 删分区自动解绑
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Windows.Forms

$AE = [System.Windows.Automation.AutomationElement]
$TS = [System.Windows.Automation.TreeScope]
$AC = [System.Windows.Automation.Condition]
$CT = [System.Windows.Automation.ControlType]

$root = 'D:\workspace\GuoDesk\artifacts'
$run = Join-Path $root 'e2e3200'
$data = Join-Path $run 'data'
$dirA = Join-Path $run 'gd3200_a'
$dirB = Join-Path $run 'gd3200_b'
$exe = Join-Path $root 'Release\GuoDesk.exe'
$report = Join-Path $root 'e2e3200.txt'
$titleA = 'GuoDesk · 甲区'
$titleB = 'GuoDesk · 乙区'
$archiveItem = '按规则归档此文件夹…'

if (Test-Path $run) { Remove-Item -Recurse -Force $run }
New-Item -ItemType Directory -Force -Path $data, $dirA, $dirB | Out-Null

$noBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText((Join-Path $dirA 'a.pdf'), 'A pdf', $noBom)
[System.IO.File]::WriteAllText((Join-Path $dirA 'a.png'), 'A png', $noBom)
[System.IO.File]::WriteAllText((Join-Path $dirB 'b.pdf'), 'B pdf', $noBom)
[System.IO.File]::WriteAllText((Join-Path $dirB 'b.png'), 'B png', $noBom)

# 乙区的 pdf 规则排在最前：一旦作用域失效，甲区的 a.pdf 会被塞进「乙区文档」，断言立刻看得出来
$template = [ordered]@{
  version = 1
  zones   = @(
    [ordered]@{ id = 'za'; name = '甲区'; x = 980; y = 150; width = 400; height = 420; collapsed = $false; mappedFolder = $dirA; viewMode = 'list'; entries = @() },
    [ordered]@{ id = 'zb'; name = '乙区'; x = 1420; y = 150; width = 400; height = 420; collapsed = $false; mappedFolder = $dirB; viewMode = 'list'; entries = @() }
  )
  rules   = @(
    [ordered]@{ id = 'r3200b'; name = '乙区文档'; exts = @('pdf'); keywords = @(); zone = 'zb'; minSize = 0; maxSize = 0; olderThan = 0 },
    [ordered]@{ id = 'r3200a'; name = '甲区文档'; exts = @('pdf'); keywords = @(); zone = 'za'; minSize = 0; maxSize = 0; olderThan = 0 },
    [ordered]@{ id = 'r3200g'; name = '全局图片'; exts = @('png'); keywords = @(); zone = ''; minSize = 0; maxSize = 0; olderThan = 0 }
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
  Start-Sleep -Milliseconds 160
  [void][U32]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 70
  [void][U32]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 260
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

$app = Start-Process -FilePath $exe -ArgumentList @('--data-dir', $data) -PassThru
Start-Sleep -Seconds 5

$winA = Find-Window $titleA 9000
$winB = Find-Window $titleB 4000
Note 'zones-found' ($null -ne $winA -and $null -ne $winB) ('a=' + ($null -ne $winA) + ' b=' + ($null -ne $winB))
if ($null -eq $winA -or $null -eq $winB) { $results | Set-Content -Path $report -Encoding UTF8; exit 1 }

# 甲区预览：只该看见自己的规则 + 全局规则，并且明说有几条被排除
$null = Open-And-Pick $winA $archiveItem
$startA = Text-Of (Find-Window $titleA 2500)
Note 'preview-says-skipped' ($startA -match '绑定在其他分区') ''
Note 'preview-lists-own-rule' ($startA -match '甲区文档' -and $startA -match '全局图片') ''
Note 'preview-hides-other-zone-rule' (-not ($startA -match '乙区文档')) ''
$go = Button-Like (Find-Window $titleA 2500) '移动 *' 6000
Note 'preview-offers-move' ($null -ne $go) ('button=' + $(if ($null -ne $go) { 'found' } else { 'none' }))
$null = Invoke-El $go
Start-Sleep -Milliseconds 1200

$deadline = (Get-Date).AddSeconds(20)
while ((Get-Date) -lt $deadline) {
  if ((Test-Path (Join-Path $dirA '甲区文档\a.pdf')) -and (Test-Path (Join-Path $dirA '全局图片\a.png'))) { break }
  Start-Sleep -Milliseconds 400
}
Note 'own-rule-filed-here' (Test-Path (Join-Path $dirA '甲区文档\a.pdf')) ''
Note 'global-rule-applies-everywhere' (Test-Path (Join-Path $dirA '全局图片\a.png')) ''
Note 'foreign-category-never-created' (-not (Test-Path (Join-Path $dirA '乙区文档'))) ''
Note 'source-loose-files-gone' (-not (Test-Path (Join-Path $dirA 'a.pdf'))) ''

# 乙区从没被碰过：它的 pdf 归它自己的规则管
Note 'other-zone-untouched' ((Test-Path (Join-Path $dirB 'b.pdf')) -and (Test-Path (Join-Path $dirB 'b.png'))) ''
$null = Open-And-Pick $winB $archiveItem
$startB = Text-Of (Find-Window $titleB 2500)
Note 'other-zone-sees-own-rule' ($startB -match '乙区文档' -and -not ($startB -match '甲区文档')) ''
Press-Esc
Start-Sleep -Milliseconds 600

# 删掉甲区：它名下的规则必须解绑成全局，乙区的规则不受影响
$null = Open-And-Pick $winA '删除分区（保留原文件）*'
Start-Sleep -Seconds 2
$after = Layout-Text
$unbound = [regex]::IsMatch($after, '"id":\s*"r3200a".*?"zone":\s*""', 'Singleline')
$kept = [regex]::IsMatch($after, '"id":\s*"r3200b".*?"zone":\s*"zb"', 'Singleline')
Note 'deleted-zone-rules-unbound' ($unbound -and $kept) ('unbound=' + $unbound + ' otherKept=' + $kept)
Note 'survivor-alive' ($null -ne (Find-Window $titleB 4000)) ''
Note 'deleted-zone-window-gone' ($null -eq (Find-Window $titleA 1200)) ''

Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 600
$results | Set-Content -Path $report -Encoding UTF8
$fails = @($results | Where-Object { $_ -like 'FAIL*' })
Write-Output ('SUMMARY passed={0} failed={1}' -f ($results.Count - $fails.Count), $fails.Count)
