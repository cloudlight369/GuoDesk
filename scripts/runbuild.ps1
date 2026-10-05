param([string]$Platform='x64',[string]$Log='build.log')
& "$PSScriptRoot\build.ps1" -Configuration Release -Platform $Platform *>&1 | Out-File -Encoding Unicode "$PSScriptRoot\..\artifacts\$Log"
$log=Get-Content -Raw "$PSScriptRoot\..\artifacts\$Log"
$ok=[bool]($log -and $log -notmatch 'error [CD]\d{4}' -and $log -notmatch 'Build FAILED')
Write-Output ("EXIT=" + [int](-not $ok))
if($ok){Write-Output 'BUILD-OK'}else{Write-Output 'BUILD-FAILED'}
