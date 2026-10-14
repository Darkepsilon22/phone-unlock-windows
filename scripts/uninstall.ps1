#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'

$serviceName = 'PhoneUnlockService'
$providerClsid = '{512D55BA-432F-42D6-95EF-8071913597AA}'
$providerLibrary = Join-Path $env:windir 'System32\PhoneUnlockProtoCP.dll'
$installDirectory = Join-Path $env:ProgramFiles 'PhoneUnlock'
$dataDirectory = Join-Path $env:ProgramData 'PhoneUnlock'
$port = 47863

Remove-Item "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\$providerClsid" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item "HKLM:\SOFTWARE\Classes\CLSID\$providerClsid" -Recurse -Force -ErrorAction SilentlyContinue
Get-ChildItem "$providerLibrary.*.old" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
Remove-Item $providerLibrary -Force -ErrorAction SilentlyContinue
$libraryLoaded = Test-Path $providerLibrary

if (Get-Service $serviceName -ErrorAction SilentlyContinue) {
    Stop-Service $serviceName -Force -ErrorAction SilentlyContinue
    sc.exe delete $serviceName | Out-Null
}
netsh http delete sslcert ipport=0.0.0.0:$port 2>&1 | Out-Null
netsh http delete urlacl url="https://+:$port/v1/" 2>&1 | Out-Null
Get-NetFirewallRule -DisplayName 'PhoneUnlock (réseau local)' -ErrorAction SilentlyContinue | Remove-NetFirewallRule
Get-ChildItem Cert:\LocalMachine\My | Where-Object Subject -eq 'CN=PhoneUnlock PC' | Remove-Item
Remove-Item (Join-Path $dataDirectory 'secrets'), (Join-Path $dataDirectory 'state') -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $installDirectory -Recurse -Force -ErrorAction SilentlyContinue
$legacyTileLogsDirectory = Join-Path $env:ProgramData 'PhoneUnlockProto'
if (Test-Path -LiteralPath $legacyTileLogsDirectory) { cmd.exe /c rmdir /s /q "$legacyTileLogsDirectory" }

'Tuile, service, liaison TLS, pare-feu, certificat, associations et secrets retirés.'
if ($libraryLoaded) {
    'La DLL de la tuile est encore chargée, mais déjà désactivée : redémarre pour la supprimer.'
}
