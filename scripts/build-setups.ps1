$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$iscc="C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if(!(Test-Path $iscc)){throw "ISCC not found"}
& $iscc "/DSourceDir=$root\artifacts\Release" "/DOutSuffix=" "$root\scripts\guodesk.iss"
Write-Output ("x64 exit="+$LASTEXITCODE)
& $iscc "/DSourceDir=$root\artifacts\Release-ARM64" "/DOutSuffix=-ARM64" "$root\scripts\guodesk.iss"
Write-Output ("arm64 exit="+$LASTEXITCODE)
