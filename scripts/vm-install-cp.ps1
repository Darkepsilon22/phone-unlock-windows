#Requires -RunAsAdministrator
param([switch]$Force)
$ErrorActionPreference = 'Stop'

$clsid = '{512D55BA-432F-42D6-95EF-8071913597AA}'
$name = 'PhoneUnlockProtoCP'
$source = Join-Path $PSScriptRoot "$name.dll"
$target = Join-Path $env:windir "System32\$name.dll"

$model = (Get-CimInstance Win32_ComputerSystem).Model
if ($model -notmatch 'VirtualBox' -and -not $Force) {
    throw "Refusé : installation réservée à la VM de test (modèle détecté : $model)."
}

Copy-Item $source $target -Force

$classKey = "HKLM:\SOFTWARE\Classes\CLSID\$clsid"
New-Item $classKey -Value $name -Force | Out-Null
New-Item "$classKey\InprocServer32" -Value "$name.dll" -Force | Out-Null
New-ItemProperty "$classKey\InprocServer32" -Name ThreadingModel -Value Apartment -Force | Out-Null

$providerKey = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\$clsid"
New-Item $providerKey -Value $name -Force | Out-Null

"Installé. Verrouille la session (Windows + L) pour voir la tuile Téléphone."
