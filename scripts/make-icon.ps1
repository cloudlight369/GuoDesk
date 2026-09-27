$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$src = 'D:\workspace\GuoDesk\vibe_images\guodesk-icon_1790519946364_f7789d69.png'
$outDir = 'D:\workspace\GuoDesk\artifacts\icon'
New-Item -ItemType Directory -Force $outDir | Out-Null

$bmp = New-Object System.Drawing.Bitmap($src)

# find bounding box of non-near-white pixels
$rect = New-Object System.Drawing.Rectangle(0, 0, $bmp.Width, $bmp.Height)
$data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$minX = $bmp.Width; $minY = $bmp.Height; $maxX = 0; $maxY = 0
$stride = $data.Stride
$bytes = New-Object byte[] ($stride * $bmp.Height)
[System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
$bmp.UnlockBits($data)
for ($y = 0; $y -lt $bmp.Height; $y++) {
    $row = $y * $stride
    for ($x = 0; $x -lt $bmp.Width; $x++) {
        $o = $row + $x * 4
        $b = $bytes[$o]; $g = $bytes[$o + 1]; $r = $bytes[$o + 2]
        if (-not ($r -gt 243 -and $g -gt 243 -and $b -gt 243)) {
            if ($x -lt $minX) { $minX = $x }
            if ($x -gt $maxX) { $maxX = $x }
            if ($y -lt $minY) { $minY = $y }
            if ($y -gt $maxY) { $maxY = $y }
        }
    }
}
Write-Host ("content bbox: $minX,$minY - $maxX,$maxY")
$cx = [int](($minX + $maxX) / 2); $cy = [int](($minY + $maxY) / 2)
$side = [Math]::Max($maxX - $minX, $maxY - $minY)
$half = [int]($side / 2) + 2
$cropX = [Math]::Max(0, [Math]::Min($bmp.Width - 2 * $half, $cx - $half))
$cropY = [Math]::Max(0, [Math]::Min($bmp.Height - 2 * $half, $cy - $half))

# crop square and apply rounded-corner alpha mask
$size = 2 * $half
$master = New-Object System.Drawing.Bitmap($size, $size)
$g = [System.Drawing.Graphics]::FromImage($master)
$g.DrawImage($bmp, (New-Object System.Drawing.Rectangle(0, 0, $master.Width, $master.Height)), (New-Object System.Drawing.Rectangle($cropX, $cropY, $master.Width, $master.Height)), [System.Drawing.GraphicsUnit]::Pixel)
$g.Dispose()
$bmp.Dispose()

$mask = New-Object System.Drawing.Bitmap($master.Width, $master.Height)
$mg = [System.Drawing.Graphics]::FromImage($mask)
$mg.Clear([System.Drawing.Color]::Transparent)
$radius = [int]($master.Width * 0.225)
$path = New-Object System.Drawing.Drawing2D.GraphicsPath
$d = $radius * 2
$path.AddArc(0, 0, $d, $d, 180, 90)
$path.AddArc($master.Width - $d - 1, 0, $d, $d, 270, 90)
$path.AddArc($master.Width - $d - 1, $master.Height - $d - 1, $d, $d, 0, 90)
$path.AddArc(0, $master.Height - $d - 1, $d, $d, 90, 90)
$path.CloseFigure()
$mg.FillPath((New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)), $path)
$mg.Dispose()

$mb = $mask.LockBits((New-Object System.Drawing.Rectangle(0, 0, $mask.Width, $mask.Height)), [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$maskBytes = New-Object byte[] ($mb.Stride * $mask.Height)
[System.Runtime.InteropServices.Marshal]::Copy($mb.Scan0, $maskBytes, 0, $maskBytes.Length)
$mask.UnlockBits($mb)

$mb2 = $master.LockBits((New-Object System.Drawing.Rectangle(0, 0, $master.Width, $master.Height)), [System.Drawing.Imaging.ImageLockMode]::ReadWrite, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$masterBytes = New-Object byte[] ($mb2.Stride * $master.Height)
[System.Runtime.InteropServices.Marshal]::Copy($mb2.Scan0, $masterBytes, 0, $masterBytes.Length)
for ($i = 3; $i -lt $masterBytes.Length; $i += 4) {
    $j = $i - 3
    if ($maskBytes[$j] -eq 0) { $masterBytes[$i] = 0 }
}
[System.Runtime.InteropServices.Marshal]::Copy($masterBytes, 0, $mb2.Scan0, $masterBytes.Length)
$master.UnlockBits($mb2)
$mask.Dispose()

$master.Save("$outDir\master.png", [System.Drawing.Imaging.ImageFormat]::Png)

foreach ($s in 256, 128, 64, 48, 32, 24, 16) {
    $small = New-Object System.Drawing.Bitmap($s, $s)
    $sg = [System.Drawing.Graphics]::FromImage($small)
    $sg.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $sg.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $sg.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $sg.DrawImage($master, (New-Object System.Drawing.Rectangle(0, 0, $s, $s)))
    $sg.Dispose()
    $small.Save("$outDir\s$s.png", [System.Drawing.Imaging.ImageFormat]::Png)
    $small.Dispose()
}
$master.Dispose()
Write-Host 'icon sizes done'
