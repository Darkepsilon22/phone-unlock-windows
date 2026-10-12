#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'

$clsid = '{512D55BA-432F-42D6-95EF-8071913597AA}'
$target = Join-Path $env:windir 'System32\PhoneUnlockProtoCP.dll'

Remove-Item "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\$clsid" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item "HKLM:\SOFTWARE\Classes\CLSID\$clsid" -Recurse -Force -ErrorAction SilentlyContinue

try {
    Remove-Item $target -Force
    'Désinstallé.'
}
catch {
    'Fournisseur désactivé. La DLL est encore chargée : redémarre pour la supprimer.'
}
