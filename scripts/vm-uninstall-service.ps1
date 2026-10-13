#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'

$name = 'PhoneUnlockService'
$installDirectory = Join-Path $env:ProgramFiles 'PhoneUnlock'
$dataDirectory = Join-Path $env:ProgramData 'PhoneUnlock'
$port = 47863

if (Get-Service $name -ErrorAction SilentlyContinue) {
    Stop-Service $name -Force -ErrorAction SilentlyContinue
    sc.exe delete $name | Out-Null
}
netsh http delete sslcert ipport=0.0.0.0:$port 2>&1 | Out-Null
netsh http delete urlacl url="https://+:$port/v1/" 2>&1 | Out-Null
Get-NetFirewallRule -DisplayName 'PhoneUnlock (réseau local)' -ErrorAction SilentlyContinue | Remove-NetFirewallRule
Get-ChildItem Cert:\LocalMachine\My | Where-Object Subject -eq 'CN=PhoneUnlock PC' | Remove-Item
Remove-Item (Join-Path $dataDirectory 'secrets'), (Join-Path $dataDirectory 'state') -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $installDirectory -Recurse -Force -ErrorAction SilentlyContinue
'Service, liaison TLS, pare-feu, certificat, associations et secrets retirés.'
