param([ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$locator=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (!(Test-Path $locator)) { throw 'Install Visual Studio C++ Build Tools first.' }
$msbuild=& $locator -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'MSBuild with C++ tools was not found.' }
& $msbuild "$root/GuoDesk.sln" /restore /m /p:Configuration=$Configuration /p:Platform=x64 /verbosity:minimal
if ($LASTEXITCODE) { throw "Build failed: $LASTEXITCODE" }
