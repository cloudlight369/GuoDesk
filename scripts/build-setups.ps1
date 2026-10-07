$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$iscc="C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if(!(Test-Path $iscc)){throw "ISCC not found"}
& $iscc "/DSourceDir=$root\artifacts\Release" "/DOutSuffix=" "$root\scripts\guodesk.iss"
Write-Output ("x64 exit="+$LASTEXITCODE)
# ARM64 不再打包（用户 2026-10-07 指示"不管ARM"）：Release 只发 x64 两件。
# 要恢复的话，把 artifacts\Release-ARM64 那一条 ISCC 调用加回来即可。
