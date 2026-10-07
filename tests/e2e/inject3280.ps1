$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Drawing
$ae = [System.Windows.Automation.AutomationElement]
$ts = [System.Windows.Automation.TreeScope]
$ct = [System.Windows.Automation.ControlType]
$sig = @'
using System;
using System.Runtime.InteropServices;
public class T {
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
 [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
 [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint f, UIntPtr e);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
}
'@
Add-Type -TypeDefinition $sig | Out-Null
$root = 'D:\workspace\GuoDesk\artifacts'
$exe = Join-Path $root 'Release\GuoDesk.exe'
$report = Join-Path $root 'e2e3280.txt'
$run = Join-Path $env:TEMP ('async' + (Get-Date).Ticks)
$data = Join-Path $run 'data'
$src = Join-Path $run 'files'
if (Test-Path $run) { Remove-Item -Recurse -Force $run }
New-Item -ItemType Directory -Force -Path $data, $src | Out-Null

$results = New-Object System.Collections.Generic.List[string]
function Note([string]$name, [bool]$ok, [string]$detail) {
  $line = '{0} {1} {2}' -f ($(if ($ok) { 'PASS' } else { 'FAIL' })), $name, $detail
  Write-Host $line
  $results.Add($line) | Out-Null
}
function Bail() {
  $results | Set-Content -Path $report -Encoding UTF8
  Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
  exit 1
}

# 25 real images: the shell has to decode and thumbnail every one of them
$noBom = New-Object System.Text.UTF8Encoding($false)
$rnd = New-Object System.Random(11)
$q = [char]34
$ents = @()
for ($k = 1; $k -le 25; $k++) {
  $f = 'shot-' + $k + '.png'
  $bmp = New-Object System.Drawing.Bitmap(1200, 800, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $rect = New-Object System.Drawing.Rectangle(0, 0, 1200, 800)
  $bd = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadWrite, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $buf = New-Object byte[] (1200 * 800 * 4)
  $rnd.NextBytes($buf)
  [System.Runtime.InteropServices.Marshal]::Copy($buf, 0, $bd.Scan0, $buf.Length)
  $bmp.UnlockBits($bd)
  $bmp.Save((Join-Path $src $f), [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  $esc = ($src -replace '\\', '\\')
  $ents += ('{' + $q + 'id' + $q + ':' + $q + 'e' + $k + $q + ',' + $q + 'path' + $q + ':' + $q + $esc + '\\' + $f + $q + ',' + $q + 'stack' + $q + ':' + $q + 's1' + $q + '}')
}
$layout = '{"version":1,"zones":[{"id":"za","name":"async","x":150,"y":120,"width":460,"height":360,"collapsed":false,"viewMode":"grid","tileSize":2,"nameLines":2,"stacks":[{"id":"s1","name":"pile"}],"entries":[' + ($ents -join ',') + ']}],"settings":{"theme":"system","compact":false,"language":"zh","guideDone":true,"labelStyle":0}}'
[System.IO.File]::WriteAllText((Join-Path $data 'layout.json'), $layout, $noBom)

function Top-Windows() {
  return $ae::RootElement.FindAll($ts::Children, [System.Windows.Automation.Condition]::TrueCondition)
}
function Find-Zone() {
  foreach ($w in Top-Windows) { if ($w.Current.Name -like 'GuoDesk *async') { return $w } }
  return $null
}
function Find-Panel($zoneName) {
  foreach ($w in Top-Windows) { $n = $w.Current.Name; if ($n -like 'GuoDesk*' -and $n -ne $zoneName) { return $w } }
  return $null
}
function Click-At($r) {
  [void][T]::SetCursorPos([int]($r.X + $r.Width / 2), [int]($r.Y + $r.Height / 2))
  Start-Sleep -Milliseconds 60
  [void][T]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
  [void][T]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
}
function Press-Escape() {
  [void][T]::keybd_event(0x1B, 0, 0, [UIntPtr]::Zero)
  [void][T]::keybd_event(0x1B, 0, 2, [UIntPtr]::Zero)
}
# 面板里有多少种颜色：空白宫格只有卡片色和几种灰，缩略图真画出来会一下多出上百种
function Color-Spread($w) {
  if ($null -eq $w) { return -1 }
  $r = $w.Current.BoundingRectangle
  $bw = [int]$r.Width; $bh = [int]$r.Height
  $bmp = New-Object System.Drawing.Bitmap($bw, $bh)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen([int]$r.X, [int]$r.Y, 0, 0, (New-Object System.Drawing.Size($bw, $bh)))
  $seen = @{}
  for ($y = 2; $y -lt $bh - 2; $y += 3) {
    for ($x = 2; $x -lt $bw - 2; $x += 3) {
      $c = $bmp.GetPixel($x, $y)
      $key = ((($c.R -shr 3) -shl 6) -bor (($c.G -shr 3) -shl 3) -bor ($c.B -shr 3))
      if (-not $seen.ContainsKey($key)) { $seen[$key] = 1 }
    }
  }
  $g.Dispose(); $bmp.Dispose()
  return $seen.Count
}
function Open-Panel($zoneName, $badgeRect, [int]$limit) {
  $sw = [System.Diagnostics.Stopwatch]::StartNew()
  Click-At $badgeRect
  while ($sw.ElapsedMilliseconds -lt 12000) {
    $p = Find-Panel $zoneName
    if ($null -ne $p) { return @{ ms = $sw.ElapsedMilliseconds; panel = $p } }
    Start-Sleep -Milliseconds 15
  }
  return @{ ms = -1; panel = $null }
}

Get-Process GuoDesk -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 3
$app = Start-Process -FilePath $exe -ArgumentList @('--data-dir', $data) -PassThru
Start-Sleep -Seconds 8
$zone = Find-Zone
Note 'zone-found' ($null -ne $zone) ''
if ($null -eq $zone) { Bail }
$badge = @($zone.FindAll($ts::Descendants, [System.Windows.Automation.PropertyCondition]::new($ae::NameProperty, '25')))
Note 'badge-found' ($badge.Count -ge 1) ('hits=' + $badge.Count)
if ($badge.Count -lt 1) { Bail }
$zt = $zone.Current.Name
[void][T]::SetForegroundWindow([IntPtr]$zone.Current.NativeWindowHandle)
Start-Sleep -Milliseconds 300

# 1) 冷缓存点开 25 张照片：面板要立刻出现，取图不许把 UI 线程按住
$r1 = Open-Panel $zt $badge[0].Current.BoundingRectangle 400
Note 'panel-appears-without-stalling' ($r1.ms -ge 0 -and $r1.ms -le 400) ('ms=' + $r1.ms)

# 2) 图标确实会到：稍后再看面板里有多少种颜色
Start-Sleep -Milliseconds 2500
$spread = Color-Spread (Find-Panel $zt)
Note 'thumbnails-land-after-the-panel-is-up' ($spread -ge 40) ('colors=' + $spread)

# 3) Escape 关掉再开：命中缓存应该更快
Press-Escape
Start-Sleep -Milliseconds 700
Note 'panel-closed-by-escape' ($null -eq (Find-Panel $zt)) ''
$r2 = Open-Panel $zt $badge[0].Current.BoundingRectangle 250
Note 'second-open-uses-the-cache' ($r2.ms -ge 0 -and $r2.ms -le 250) ('ms=' + $r2.ms)

# 4) 反复开关：格子被销毁之后，晚到的回调不能再碰它
for ($i = 0; $i -lt 4; $i++) {
  Press-Escape
  Start-Sleep -Milliseconds 120
  $null = Open-Panel $zt $badge[0].Current.BoundingRectangle 4000
}
Start-Sleep -Milliseconds 1500
$final = Find-Panel $zt
$spread2 = Color-Spread $final
Note 'churn-leaves-a-working-panel' ($null -ne $final -and $spread2 -ge 40) ('colors=' + $spread2)
Note 'app-still-running' (@(Get-Process GuoDesk -ErrorAction SilentlyContinue).Count -ge 1) ''
Note 'zone-alive' ($null -ne (Find-Zone)) ''
Bail
