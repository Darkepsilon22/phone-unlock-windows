#Requires -RunAsAdministrator
param([switch]$Force)
$ErrorActionPreference = 'Stop'

$name = 'PhoneUnlockService'
$account = "NT SERVICE\$name"
$installDirectory = Join-Path $env:ProgramFiles 'PhoneUnlock'
$executable = Join-Path $installDirectory "$name.exe"
$dataDirectory = Join-Path $env:ProgramData 'PhoneUnlock'

$model = (Get-CimInstance Win32_ComputerSystem).Model
if ($model -notmatch 'VirtualBox' -and -not $Force) {
    throw "Refusé : installation réservée à la VM de test (modèle détecté : $model)."
}

if (Get-Service $name -ErrorAction SilentlyContinue) {
    Stop-Service $name -Force -ErrorAction SilentlyContinue
}

New-Item -ItemType Directory -Force $installDirectory | Out-Null
Copy-Item (Join-Path $PSScriptRoot "$name.exe") $executable -Force

if (-not (Get-Service $name -ErrorAction SilentlyContinue)) {
    sc.exe create $name binPath= "`"$executable`"" start= auto obj= $account DisplayName= 'PhoneUnlock' | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Création du service en échec ($LASTEXITCODE)." }
    sc.exe sidtype $name unrestricted | Out-Null
    sc.exe description $name 'Déverrouillage Windows par téléphone (prototype)' | Out-Null
}

foreach ($folder in 'secrets', 'logs') {
    $path = Join-Path $dataDirectory $folder
    New-Item -ItemType Directory -Force $path | Out-Null
    icacls $path /inheritance:r /grant:r "*S-1-5-18:(OI)(CI)F" "*S-1-5-32-544:(OI)(CI)F" "${account}:(OI)(CI)M" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "ACL de $path en échec." }
}

Start-Service $name
Get-Service $name | Format-Table Name, Status, StartType -AutoSize
