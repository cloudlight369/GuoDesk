param([string]$Platform='x64',[string]$Log='build.log')
& "$PSScriptRoot\build.ps1" -Configuration Release -Platform $Platform *>&1 | Out-File -Encoding Unicode "$PSScriptRoot\..\artifacts\$Log"
Write-Output ("EXIT=" + $LASTEXITCODE)
