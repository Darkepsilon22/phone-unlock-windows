#Requires -RunAsAdministrator
param([switch]$Force)
$ErrorActionPreference = 'Stop'

$name = 'PhoneUnlockService'
$account = "NT SERVICE\$name"
$installDirectory = Join-Path $env:ProgramFiles 'PhoneUnlock'
$executable = Join-Path $installDirectory "$name.exe"
$dataDirectory = Join-Path $env:ProgramData 'PhoneUnlock'
$port = 47863
$urlPrefix = "https://+:$port/v1/"
$bindingAppId = '{78ABD9E7-8480-4CFD-B15C-43B5E2DE9C02}'
$certificateSubject = 'CN=PhoneUnlock PC'
$firewallRule = 'PhoneUnlock (réseau local)'

$model = (Get-CimInstance Win32_ComputerSystem).Model
if ($model -notmatch 'VirtualBox' -and -not $Force) {
    throw "Refusé : installation réservée à la VM de test (modèle détecté : $model)."
}

function Invoke-Native([string]$description, [scriptblock]$command) {
    $output = & $command 2>&1
    if ($LASTEXITCODE -ne 0) { throw "$description en échec : $output" }
}

if (Get-Service $name -ErrorAction SilentlyContinue) {
    Stop-Service $name -Force -ErrorAction SilentlyContinue
}

New-Item -ItemType Directory -Force $installDirectory | Out-Null
Copy-Item (Join-Path $PSScriptRoot "$name.exe") $executable -Force

if (-not (Get-Service $name -ErrorAction SilentlyContinue)) {
    Invoke-Native 'Création du service' { sc.exe create $name binPath= "`"$executable`"" start= auto obj= $account DisplayName= 'PhoneUnlock' }
    Invoke-Native 'Type de SID du service' { sc.exe sidtype $name unrestricted }
    sc.exe description $name 'Déverrouillage Windows par téléphone (prototype)' | Out-Null
}

foreach ($folder in 'secrets', 'logs', 'state') {
    $path = Join-Path $dataDirectory $folder
    New-Item -ItemType Directory -Force $path | Out-Null
    Invoke-Native "ACL de $path" { icacls $path /inheritance:r /grant:r "*S-1-5-18:(OI)(CI)F" "*S-1-5-32-544:(OI)(CI)F" "${account}:(OI)(CI)M" }
}
New-Item -ItemType Directory -Force (Join-Path $dataDirectory 'state\pairings') | Out-Null

$certificate = Get-ChildItem Cert:\LocalMachine\My | Where-Object Subject -eq $certificateSubject | Select-Object -First 1
if (-not $certificate) {
    $certificate = New-SelfSignedCertificate -Subject $certificateSubject -KeyAlgorithm ECDSA_nistP256 `
        -KeyExportPolicy NonExportable -CertStoreLocation Cert:\LocalMachine\My -NotAfter (Get-Date).AddYears(10) `
        -KeyUsage DigitalSignature -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.1') `
        -Provider 'Microsoft Software Key Storage Provider'
}

netsh http delete sslcert ipport=0.0.0.0:$port 2>&1 | Out-Null
$binding = @("ipport=0.0.0.0:$port", "certhash=$($certificate.Thumbprint)", "appid=$bindingAppId",
    'clientcertnegotiation=enable', 'verifyclientcertrevocation=disable', 'disablelegacytls=enable')
$output = netsh http add sslcert @binding disabletls12=enable 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Warning "TLS 1.2 ne peut pas être désactivé sur ce Windows : $output"
    Invoke-Native 'Liaison TLS' { netsh http add sslcert @binding }
}

netsh http delete urlacl url=$urlPrefix 2>&1 | Out-Null
Invoke-Native 'Réservation URL' { netsh http add urlacl url=$urlPrefix user=$account }

Get-NetFirewallRule -DisplayName $firewallRule -ErrorAction SilentlyContinue | Remove-NetFirewallRule
New-NetFirewallRule -DisplayName $firewallRule -Direction Inbound -Protocol TCP -LocalPort $port `
    -RemoteAddress LocalSubnet -Action Allow -Profile Any | Out-Null

Start-Service $name
Get-Service $name | Format-Table Name, Status, StartType -AutoSize
"Empreinte du certificat : $($certificate.Thumbprint)"
