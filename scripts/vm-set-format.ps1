#Requires -RunAsAdministrator
param(
    [Parameter(Mandatory)]
    [ValidateSet(0, 1, 2)]
    [int]$Format
)
$ErrorActionPreference = 'Stop'

$key = 'HKLM:\SOFTWARE\PhoneUnlockProto'
New-Item $key -Force | Out-Null
New-ItemProperty $key -Name SerializationFormat -PropertyType DWord -Value $Format -Force | Out-Null

$labels = @{ 0 = 'CredPack + fournisseur d''identité'; 1 = 'KERB_INTERACTIVE_UNLOCK_LOGON'; 2 = 'CredPack protégé' }
"Format $Format actif : $($labels[$Format]). Pris en compte à la prochaine tentative."
