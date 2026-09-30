param([ValidateSet('Debug','Release')][string]$Configuration='Release',[ValidateSet('x64','ARM64')][string]$Platform='x64')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$locator=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (!(Test-Path $locator)) { throw 'Install Visual Studio C++ Build Tools first.' }
$msbuild=& $locator -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'MSBuild with C++ tools was not found.' }
& $msbuild "$root/GuoDesk.sln" /restore /m /p:Configuration=$Configuration /p:Platform=$Platform /p:TargetPlatformVersion=10.0.26100.0 /verbosity:minimal
if ($LASTEXITCODE) { throw "Build failed: $LASTEXITCODE" }
$outDir="$root/artifacts/$Configuration"
if ($Platform -ne 'x64') { $outDir="$root/artifacts/$Configuration-$Platform" }
Copy-Item "$root/src/GuoDesk/app.ico" "$outDir/guodesk.ico" -Force
