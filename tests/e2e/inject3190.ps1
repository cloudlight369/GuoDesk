# v3.19.0 E2E：叠放缩略图宫格（3×3 画得出 9 张、菜单切回单图只剩 1 张）
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Windows.Forms

$AE = [System.Windows.Automation.AutomationElement]
$TS = [System.Windows.Automation.TreeScope]
$AC = [System.Windows.Automation.Condition]
$CT = [System.Windows.Automation.ControlType]

$root = 'D:\workspace\GuoDesk\artifacts'
$run = Join-Path $root 'e2e3190'
$data = Join-Path $run 'data'
$src = Join-Path $run 'gd3190_src'
$exe = Join-Path $root 'Release\GuoDesk.exe'
$report = Join-Path $root 'e2e3190.txt'
$titleZone = 'GuoDesk · 宫格叠放'

if (Test-Path $run) { Remove-Item -Recurse -Force $run }
New-Item -ItemType Directory -Force -Path $data, $src | Out-Null

$noBom = New-Object System.Text.UTF8Encoding($false)
$entries = @()
for ($i = 1; $i -le 9; $i++) {
  $f = Join-Path $src ("item{0}.txt" -f $i)
  [System.IO.File]::WriteAllText($f, "content $f", $noBom)
  $entries += [ordered]@{ id = ("e{0}" -f $i); path = $f; stack = 's1' }
}

$template = [ordered]@{
  version = 1
  zones   = @([ordered]@{
    id = 'zg'; name = '宫格叠放'; x = 980; y = 150; width = 420; height = 460; collapsed = $false
    viewMode = 'grid'; tileSize = 2; stackGrid = 1
    stacks = @([ordered]@{ id = 's1'; name = '九件套' })
    entries = $entries
  })
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
  # 长菜单里的项 Invoke 会抛（弹层虚拟化了），退化成按矩形点一下
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

# 叠放缩略图只有 Image 元素，宫格画几张就有几个：用数量反证渲染分支
function Image-Count($win) {
  $n = 0
  foreach ($e in $win.FindAll($TS::Descendants, (Kind-Cond $CT::Image))) { if (-not $e.Current.IsOffscreen) { $n++ } }
  return $n
}

$app = Start-Process -FilePath $exe -ArgumentList @('--data-dir', $data) -PassThru
Start-Sleep -Seconds 4

$zone = Find-Window $titleZone 9000
Note 'zone-found' ($null -ne $zone) $titleZone
if ($null -eq $zone) { $results | Set-Content -Path $report -Encoding UTF8; exit 1 }

$icons = 0
$deadline = (Get-Date).AddSeconds(12)
while ((Get-Date) -lt $deadline) {
  $icons = Image-Count $zone
  if ($icons -ge 9) { break }
  Start-Sleep -Milliseconds 400
}
Note 'grid-draws-nine-thumbs' ($icons -ge 9) ('images=' + $icons)
$badge = (Text-Of $zone) -match '\|9\|'
Note 'badge-counts-stack' $badge ('text=' + $(if ((Text-Of $zone) -match '\|(9)\|') { 'has-9' } else { 'no-9' }))

# 菜单项是循环切换：3×3 → 4×4 → 5×5 → 单图，逐档核对配置与缩略图张数
function Toggle-Grid($win) {
  Press-Esc
  $w = Find-Window $titleZone 2500
  if ($null -eq $w) { return $false }
  $null = Focus-Window $w
  if (-not (Invoke-El (More-Button $w))) { return $false }
  Start-Sleep -Milliseconds 350
  $item = Menu-Item '叠放缩略图：*' 4000
  if ($null -eq $item) { return $false }
  return (Invoke-El $item)
}

function Grid-Mode() {
  $j = [System.IO.File]::ReadAllText((Join-Path $data 'layout.json'))
  if ($j -match '"stackGrid":\s*(\d+)') { return [int]$Matches[1] }
  return -1
}

$opened = Invoke-El (More-Button $zone)
$menuText = ''
if ($opened) { $menuText = Text-Of (Find-Window $titleZone 1500) }
$toggle = Menu-Item '叠放缩略图：*' 4000
Note 'menu-offers-grid-toggle' ($null -ne $toggle) ('label=' + $(if ($null -ne $toggle) { $toggle.Current.Name } else { 'none' }))
if ($null -ne $toggle) { [void](Invoke-El $toggle) }
Start-Sleep -Milliseconds 900
Note 'grid-cycles-to-4x4' ((Grid-Mode) -eq 2) ('mode=' + (Grid-Mode))
if (-not (Toggle-Grid $zone)) { Note 'second-toggle-opened' $false '' }
Start-Sleep -Milliseconds 900
Note 'grid-cycles-to-5x5' ((Grid-Mode) -eq 3) ('mode=' + (Grid-Mode))
if (-not (Toggle-Grid $zone)) { Note 'third-toggle-opened' $false '' }
Start-Sleep -Milliseconds 900
$icons2 = Image-Count (Find-Window $titleZone 2500)
Note 'single-mode-one-thumb' ((Grid-Mode) -eq 0 -and $icons2 -le 2) ('mode=' + (Grid-Mode) + ' images=' + $icons2)
if (-not (Toggle-Grid $zone)) { Note 'fourth-toggle-opened' $false '' }
Start-Sleep -Milliseconds 900
$icons3 = Image-Count (Find-Window $titleZone 2500)
Note 'grid-mode-returns' ((Grid-Mode) -eq 1 -and $icons3 -ge 9) ('mode=' + (Grid-Mode) + ' images=' + $icons3)

Note 'zone-alive' ($null -ne (Find-Window $titleZone 2500)) ''

Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 600
$results | Set-Content -Path $report -Encoding UTF8
$fails = @($results | Where-Object { $_ -like 'FAIL*' })
Write-Output ('SUMMARY passed={0} failed={1}' -f ($results.Count - $fails.Count), $fails.Count)
