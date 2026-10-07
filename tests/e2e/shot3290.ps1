# v3.29.0 截图：批量重命名对话框的两张实况（撞名被拦 / 模板干净可保存）
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Windows.Forms, System.Drawing

$AE = [System.Windows.Automation.AutomationElement]
$TS = [System.Windows.Automation.TreeScope]
$AC = [System.Windows.Automation.Condition]
$CT = [System.Windows.Automation.ControlType]

$root = 'D:\workspace\GuoDesk\artifacts'
$run = Join-Path $root 'shot3290'
$data = Join-Path $run 'data'
$src = Join-Path $run 'gd3290_src'
$exe = Join-Path $root 'Release\GuoDesk.exe'
$titleZone = 'GuoDesk · 批量重命名'
$defaultPattern = '{name}_{n}'

if (Test-Path $run) { Remove-Item -Recurse -Force $run }
New-Item -ItemType Directory -Force -Path $data, $src | Out-Null
$noBom = New-Object System.Text.UTF8Encoding($false)
foreach ($n in @('a1.txt', 'a2.txt', 'b1.png', 'notes.md')) { [System.IO.File]::WriteAllText((Join-Path $src $n), "body of $n", $noBom) }
$esc = ($src -replace '\\', '\\')
$layout = '{"version":1,"zones":[{"id":"zr","name":"批量重命名","x":160,"y":120,"width":520,"height":440,"collapsed":false,"viewMode":"list","nameLines":1,"mappedFolder":"' + $esc + '","browseInPlace":true,"entries":[]}],"settings":{"theme":"system","compact":false,"language":"zh","guideDone":true,"labelStyle":0}}'
[System.IO.File]::WriteAllText((Join-Path $data 'layout.json'), $layout, $noBom)

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

function Kind-Cond($kind) { return [System.Windows.Automation.PropertyCondition]::new($AE::ControlTypeProperty, $kind) }
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
  $h = [IntPtr]$w.Current.NativeWindowHandle
  $cur = [U32]::GetCurrentThreadId()
  for ($i = 0; $i -lt 20; $i++) {
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
  return $false
}
function Click-At($r) {
  [void][U32]::SetCursorPos([int]($r.X + $r.Width / 2), [int]($r.Y + $r.Height / 2))
  Start-Sleep -Milliseconds 120
  [void][U32]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
  [void][U32]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 320
}
function Invoke-El($e) {
  if ($null -eq $e) { return $false }
  try { $e.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke(); Start-Sleep -Milliseconds 240; return $true } catch { }
  try { Click-At $e.Current.BoundingRectangle; return $true } catch { return $false }
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
function Get-Value($e) {
  try { return [string]$e.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern).Current.Value } catch { return '' }
}
function Set-Value($e, [string]$text) {
  try { $e.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern).SetValue($text); Start-Sleep -Milliseconds 500; return $true } catch { return $false }
}
function Pattern-Box {
  $zone = Find-Window $titleZone 3000
  foreach ($e in $zone.FindAll($TS::Descendants, (Kind-Cond $CT::Edit))) {
    $v = Get-Value $e
    if ($v -eq $defaultPattern -or $v -eq '同一份' -or $v -eq '归档_{n}') { return $e }
  }
  return $null
}
function Shot([string]$out) {
  $zone = Find-Window $titleZone 3000
  $r = $zone.Current.BoundingRectangle
  $x = [int]($r.X); $y = [int]($r.Y); $w = [int]($r.Width); $h = [int]($r.Height)
  $bmp = New-Object System.Drawing.Bitmap($w, $h)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($x, $y, 0, 0, (New-Object System.Drawing.Size($w, $h)))
  $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
  Write-Output ('shot=' + $out + ' ' + $w + 'x' + $h)
}

$app = Start-Process -FilePath $exe -ArgumentList @('--data-dir', $data) -PassThru
Start-Sleep -Seconds 6
$zone = Find-Window $titleZone 9000
if ($null -eq $zone) { Write-Output 'NO-ZONE'; exit 1 }
[void](Focus-Window $zone)
foreach ($t in $zone.FindAll($TS::Descendants, (Kind-Cond $CT::Text))) {
  if ($t.Current.Name -notlike '*.txt') { continue }
  if ($t.Current.IsOffscreen) { continue }
  Click-At $t.Current.BoundingRectangle
  break
}
[void][U32]::keybd_event(0x11, 0, 0, [UIntPtr]::Zero)
[void][U32]::keybd_event(0x41, 0, 0, [UIntPtr]::Zero)
[void][U32]::keybd_event(0x41, 0, 2, [UIntPtr]::Zero)
[void][U32]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 500

[void](Invoke-El (More-Button (Find-Window $titleZone 2500)))
[void](Invoke-El (Menu-Item '批量重命名选中项…' 4000))
Start-Sleep -Milliseconds 900
Shot (Join-Path $root 'shot3290-default.png')

$box = Pattern-Box
[void](Set-Value $box '同一份')
Start-Sleep -Milliseconds 500
Shot (Join-Path $root 'shot3290-clash.png')

[void](Set-Value $box '报告-{n}-2026年第四季度汇总')
Start-Sleep -Milliseconds 500
Shot (Join-Path $root 'shot3290-longname.png')

$cancel = Menu-Item '取消' 2000
[void](Invoke-El $cancel)
Start-Sleep -Milliseconds 500
Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
