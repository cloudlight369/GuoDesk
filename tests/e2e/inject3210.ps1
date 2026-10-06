# v3.21.0 E2E：性能三档（旧的 performance 布尔必须迁成"省电"，切回"完整特效"宫格要回来）
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Windows.Forms

$AE = [System.Windows.Automation.AutomationElement]
$TS = [System.Windows.Automation.TreeScope]
$AC = [System.Windows.Automation.Condition]
$CT = [System.Windows.Automation.ControlType]

$root = 'D:\workspace\GuoDesk\artifacts'
$run = Join-Path $root 'e2e3210'
$data = Join-Path $run 'data'
$src = Join-Path $run 'gd3210_src'
$exe = Join-Path $root 'Release\GuoDesk.exe'
$report = Join-Path $root 'e2e3210.txt'
$titleZone = 'GuoDesk · 性能叠放'
$titleSettings = 'GuoDesk 设置'

if (Test-Path $run) { Remove-Item -Recurse -Force $run }
New-Item -ItemType Directory -Force -Path $data, $src | Out-Null

$noBom = New-Object System.Text.UTF8Encoding($false)
$entries = @()
for ($i = 1; $i -le 9; $i++) {
  $f = Join-Path $src ("note{0}.txt" -f $i)
  [System.IO.File]::WriteAllText($f, "payload $f", $noBom)
  $entries += [ordered]@{ id = ("n{0}" -f $i); path = $f; stack = 's1' }
}

# 故意只写旧键 performance:true —— 升级后应当等于"省电"，叠放宫格必须退回单图
$layout = '{"version":1,"zones":[{"id":"zp","name":"性能叠放","x":980,"y":150,"width":420,"height":460,"collapsed":false,"viewMode":"grid","tileSize":2,"stackGrid":3,"stacks":[{"id":"s1","name":"九件套"}],"entries":['
$parts = @()
foreach ($e in $entries) { $parts += ('{"id":"' + $e.id + '","path":"' + ($e.path -replace '\\', '\\') + '","stack":"s1"}') }
$layout += ($parts -join ',')
$layout += ']}],"settings":{"theme":"system","compact":false,"language":"zh","guideDone":true,"performance":true}}'
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

function Image-Count($win) {
  $n = 0
  foreach ($e in $win.FindAll($TS::Descendants, (Kind-Cond $CT::Image))) { if (-not $e.Current.IsOffscreen) { $n++ } }
  return $n
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

$zone = Find-Window $titleZone 9000
Note 'zone-found' ($null -ne $zone) $titleZone
if ($null -eq $zone) { $results | Set-Content -Path $report -Encoding UTF8; exit 1 }

# 旧 performance:true 迁成"省电"：叠放宫格必须退回单图（哪怕分区自己写着 stackGrid:3）
Start-Sleep -Seconds 2
$powerImages = Image-Count (Find-Window $titleZone 2500)
Note 'legacy-flag-becomes-power-saver' ($powerImages -ge 1 -and $powerImages -le 2) ('images=' + $powerImages)

function Open-Menu($win) {
  Press-Esc
  $w = Find-Window ($win.Current.Name) 2500
  if ($null -eq $w) { return $false }
  $null = Focus-Window $w
  return (Invoke-El (More-Button $w))
}

# 省电档下分区菜单必须说实话，而不是继续显示"5×5 宫格（点击切换）"
$null = Open-Menu $zone
$honest = Menu-Item '叠放缩略图：省电模式下不显示' 4000
Note 'power-saver-menu-honest' ($null -ne $honest) ('label=' + $(if ($null -ne $honest) { $honest.Current.Name } else { 'none' }))
Press-Esc

# 设置页的下拉在 UIA 里没有可见文本（WinUI ComboBox 的选中项不进子树），所以按 AutomationId 找、按 SelectionItem 读
$tiers = @('完整特效', '精简', '省电')
function Perf-Combo($win) {
  foreach ($c in $win.FindAll($TS::Descendants, (Kind-Cond $CT::ComboBox))) { if ($c.Current.AutomationId -eq 'perfTier') { return $c } }
  return $null
}
function Perf-Selected($win) {
  $c = Perf-Combo $win
  if ($null -eq $c) { return '<missing>' }
  try { $c.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern).Expand() } catch { return '<locked>' }
  Start-Sleep -Milliseconds 700
  $hit = '<none>'
  foreach ($w in $AE::RootElement.FindAll($TS::Children, $AC::TrueCondition)) {
    foreach ($li in $w.FindAll($TS::Descendants, (Kind-Cond $CT::ListItem))) {
      if ($tiers -notcontains $li.Current.Name) { continue }
      try { if ($li.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern).Current.IsSelected) { $hit = $li.Current.Name } } catch { }
    }
  }
  Press-Esc
  return $hit
}

$null = Open-And-Pick $zone '设置'
$setWin = Find-Window $titleSettings 9000
Note 'settings-window' ($null -ne $setWin) ''
if ($null -eq $setWin) { $results | Set-Content -Path $report -Encoding UTF8; exit 1 }

Note 'perf-combo-exists' ($null -ne (Perf-Combo $setWin)) ''
$migrated = Perf-Selected $setWin
Note 'legacy-maps-to-power-saver-in-ui' ($migrated -eq '省电') ('selected=' + $migrated)

$c = Perf-Combo $setWin
try { $c.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern).Expand() } catch { }
Start-Sleep -Milliseconds 700
$full = Menu-Item '完整特效' 4000
Note 'perf-offer-full-effects' ($null -ne $full) ''
$null = Invoke-El $full
Start-Sleep -Milliseconds 1500

$after = Layout-Text
Note 'tier-persisted-as-number' ($after -match '"perfTier":\s*0') ('perfTier0=' + ($after -match '"perfTier":\s*0'))
$back = Find-Window $titleZone 3000
$fullImages = if ($null -ne $back) { Image-Count $back } else { -1 }
Note 'full-effects-restores-grid' ($fullImages -ge 9) ('images=' + $fullImages)
$null = Open-Menu (Find-Window $titleZone 2500)
$toggle = Menu-Item '叠放缩略图：*（点击切换）' 4000
Note 'full-effects-menu-offers-toggle' ($null -ne $toggle) ('label=' + $(if ($null -ne $toggle) { $toggle.Current.Name } else { 'none' }))
Press-Esc
$readBack = Perf-Selected (Find-Window $titleSettings 2500)
Note 'tier-reads-back-in-ui' ($readBack -eq '完整特效') ('selected=' + $readBack)

# v3.22.0 的标签底板走同一个设置页，顺手验它的下拉与落盘
$setWin2 = Find-Window $titleSettings 2500
$lsCombo = $null
foreach ($c in $setWin2.FindAll($TS::Descendants, (Kind-Cond $CT::ComboBox))) { if ($c.Current.AutomationId -eq 'labelStyle') { $lsCombo = $c; break } }
Note 'label-combo-exists' ($null -ne $lsCombo) ''
if ($null -ne $lsCombo) {
  try { $lsCombo.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern).Expand() } catch { }
  Start-Sleep -Milliseconds 600
  $hi = Menu-Item '高对比深底' 4000
  $null = Invoke-El $hi
  Start-Sleep -Milliseconds 1200
  Note 'label-style-persisted' ((Layout-Text) -match '"labelStyle":\s*2') ''
}

Press-Esc
Note 'zone-alive' ($null -ne (Find-Window $titleZone 2500)) ''

Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 600
$results | Set-Content -Path $report -Encoding UTF8
$fails = @($results | Where-Object { $_ -like 'FAIL*' })
Write-Output ('SUMMARY passed={0} failed={1}' -f ($results.Count - $fails.Count), $fails.Count)
