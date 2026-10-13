#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'

$name = 'PhoneUnlockService'
$installDirectory = Join-Path $env:ProgramFiles 'PhoneUnlock'
$secrets = Join-Path $env:ProgramData 'PhoneUnlock\secrets'

if (Get-Service $name -ErrorAction SilentlyContinue) {
    Stop-Service $name -Force -ErrorAction SilentlyContinue
    sc.exe delete $name | Out-Null
}
Remove-Item $secrets -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $installDirectory -Recurse -Force -ErrorAction SilentlyContinue
'Service retiré et secrets effacés.'
