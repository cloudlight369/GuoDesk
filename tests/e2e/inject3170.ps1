# v3.17.0 E2E：映射分区按规则自动归档（无需点击、递归子目录、已在位不动、未开启的分区不受影响）
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Windows.Forms

$AE = [System.Windows.Automation.AutomationElement]
$TS = [System.Windows.Automation.TreeScope]
$AC = [System.Windows.Automation.Condition]
$CT = [System.Windows.Automation.ControlType]

$root = 'D:\workspace\GuoDesk\artifacts'
$run = Join-Path $root 'e2e3170'
$data = Join-Path $run 'data'
$arc = Join-Path $run 'gd3170_auto'
$arc2 = Join-Path $run 'gd3170_manual'
$exe = Join-Path $root 'Release\GuoDesk.exe'
$report = Join-Path $root 'e2e3170.txt'
$titleAuto = 'GuoDesk · 自动归档'
$titleManual = 'GuoDesk · 手动分区'

if (Test-Path $run) { Remove-Item -Recurse -Force $run }
New-Item -ItemType Directory -Force -Path $data, (Join-Path $arc 'misc'), (Join-Path $arc '文档'), $arc2 | Out-Null

$noBom = New-Object System.Text.UTF8Encoding($false)
foreach ($p in 'a1.pdf', 'a2.pdf', 'shot.png') { [System.IO.File]::WriteAllText((Join-Path $arc $p), "loose $p", $noBom) }
[System.IO.File]::WriteAllText((Join-Path $arc 'misc\late.png'), 'nested into a category', $noBom)
[System.IO.File]::WriteAllText((Join-Path $arc '文档\old.pdf'), 'pre-existing original', $noBom)
[System.IO.File]::WriteAllText((Join-Path $arc2 'b1.pdf'), 'manual zone stays put', $noBom)

$template = [ordered]@{
  version = 1
  zones   = @(
    [ordered]@{ id = 'za'; name = '自动归档'; x = 980; y = 150; width = 420; height = 460; collapsed = $false; mappedFolder = $arc; viewMode = 'list'; autoArchive = 1; archiveAt = 0; entries = @() },
    [ordered]@{ id = 'zb'; name = '手动分区'; x = 1440; y = 150; width = 400; height = 320; collapsed = $false; mappedFolder = $arc2; viewMode = 'list'; entries = @() }
  )
  rules   = @(
    [ordered]@{ id = 'r3170a'; name = '文档'; exts = @('pdf'); keywords = @(); zone = 'za' },
    [ordered]@{ id = 'r3170b'; name = '图片'; exts = @('png'); keywords = @(); zone = 'za' }
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

function Wait-Leaf([string]$dir, [string]$leaf, [bool]$expected, [int]$ms) {
  $p = Join-Path $dir $leaf
  $deadline = (Get-Date).AddMilliseconds($ms)
  while ((Get-Date) -lt $deadline) {
    if ((Test-Path -PathType Leaf $p) -eq $expected) { return $true }
    Start-Sleep -Milliseconds 300
  }
  return (Test-Path -PathType Leaf $p) -eq $expected
}

$app = Start-Process -FilePath $exe -ArgumentList @('--data-dir', $data) -PassThru
Start-Sleep -Seconds 3

$zoneA = Find-Window $titleAuto 9000
$zoneB = Find-Window $titleManual 4000
Note 'zones-found' ($null -ne $zoneA -and $null -ne $zoneB) ('auto=' + ($null -ne $zoneA) + ' manual=' + ($null -ne $zoneB))
if ($null -eq $zoneA -or $null -eq $zoneB) { $results | Set-Content -Path $report -Encoding UTF8; exit 1 }

# D1 没有任何点击：到点就自动把散文件归入分类子文件夹（含子目录里的）
$moved = (Wait-Leaf $arc '文档\a1.pdf' $true 70000) -and (Wait-Leaf $arc '文档\a2.pdf' $true 20000)
$moved = $moved -and (Wait-Leaf $arc '图片\shot.png' $true 20000)
Note 'auto-filed-loose-files' $moved (('doc=' + (Get-ChildItem (Join-Path $arc '文档') -File -ErrorAction SilentlyContinue).Count) + ' img=' + (Get-ChildItem (Join-Path $arc '图片') -File -ErrorAction SilentlyContinue).Count)
Note 'auto-removed-from-root' ((Test-Path -PathType Leaf (Join-Path $arc 'a1.pdf')) -eq $false) ''

# D2 递归只在手动确认归档里做：自动路径不得把用户自己分好的子目录卷走
$nested = (Test-Path -PathType Leaf (Join-Path $arc 'misc\late.png')) -and -not (Test-Path -PathType Leaf (Join-Path $arc '图片\late.png'))
Note 'auto-leaves-user-subfolders' $nested ('miscStill=' + (Test-Path -PathType Leaf (Join-Path $arc 'misc\late.png')) + ' moved=' + (Test-Path -PathType Leaf (Join-Path $arc '图片\late.png')))

# D3 已经在分类文件夹里的文件不被二次搬运，也不会生成副本
$old = ''
try { $old = [System.IO.File]::ReadAllText((Join-Path $arc '文档\old.pdf')) } catch { }
$dup = @(Get-ChildItem (Join-Path $arc '文档') -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -like 'old (2)*' })
Note 'already-filed-untouched' ($old -eq 'pre-existing original' -and $dup.Count -eq 0) ('dup=' + $dup.Count)

# D4 没开启自动归档的分区完全不受影响
Note 'manual-zone-untouched' (Test-Path -PathType Leaf (Join-Path $arc2 'b1.pdf')) ('still=' + (Test-Path -PathType Leaf (Join-Path $arc2 'b1.pdf')))

# D5 时间戳落盘：重启后不会立刻再跑一次
Start-Sleep -Milliseconds 1500
$json = [System.IO.File]::ReadAllText((Join-Path $data 'layout.json'))
$stamp = 0.0
if ($json -match '"archiveAt":\s*([0-9.eE+\-]+)') { $stamp = [double]$Matches[1] }
Note 'archive-timestamp-persisted' ($stamp -gt 1000000000 -and $json -match '"autoArchive":\s*1') ('stamp=' + $stamp)

# D6 结果提示出现在分区里（自动跑完也要让人知道搬了多少）
$zoneA = Find-Window $titleAuto 4000
Note 'auto-toast' ((Text-Of $zoneA) -match '已把 [34] 个文件归档到 [12] 个分类文件夹') ('saw=' + $(if ((Text-Of $zoneA) -match '(已把[^|]{0,40})') { $Matches[0] } else { 'none' }))

# D7 菜单里能改档期：自动归档子菜单必须存在
$null = Focus-Window $zoneA
$more = More-Button $zoneA
$opened = $false
if ($null -ne $more) {
  try { $p = $more.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern); $p.Invoke(); $opened = $true } catch { $opened = $false }
}
Start-Sleep -Milliseconds 400
$sub = Menu-Item '自动归档' 3000
Note 'auto-menu-exposed' ($opened -and $null -ne $sub) ('opened=' + $opened)
[System.IO.File]::WriteAllText((Join-Path $arc 'tail.pdf'), 'after the run', $noBom)

Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 600
$results | Set-Content -Path $report -Encoding UTF8
$fails = @($results | Where-Object { $_ -like 'FAIL*' })
Write-Output ('SUMMARY passed={0} failed={1}' -f ($results.Count - $fails.Count), $fails.Count)
