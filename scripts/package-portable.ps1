param([string]$Version = '1.0.0', [ValidateSet('x64','ARM64')][string]$Platform = 'x64')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$src = Join-Path $root 'artifacts\Release'
$suffix = ''
if ($Platform -ne 'x64') { $src = Join-Path $root ("artifacts\Release-" + $Platform); $suffix = "-" + $Platform }
if (!(Test-Path (Join-Path $src 'GuoDesk.exe'))) { throw 'Build first: ./scripts/build.ps1' }

$stage = Join-Path $env:TEMP ('GuoDesk-portable-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $stage 'GuoDesk') -Force | Out-Null
Copy-Item -Path (Join-Path $src '*') -Destination (Join-Path $stage 'GuoDesk') -Recurse
Get-ChildItem $stage -Recurse -Include *.pdb, *.exp, *.lib -File | Remove-Item -Force

$out = Join-Path $root ("artifacts\installer\GuoDesk-$Version$suffix-portable.zip")
if (Test-Path $out) { Remove-Item $out -Force }
# bsdtar writes spec-compliant '/' separators; Compress-Archive writes '\' which some extractors flatten
& "$env:SystemRoot\System32\tar.exe" -a -c -f $out -C $stage GuoDesk
if ($LASTEXITCODE) { throw "tar failed: $LASTEXITCODE" }
Remove-Item $stage -Recurse -Force
Write-Host "Created $out ($([math]::Round((Get-Item $out).Length / 1MB, 1)) MB)"
