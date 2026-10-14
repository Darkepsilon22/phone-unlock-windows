#Requires -RunAsAdministrator
param([switch]$AllowPhysicalMachine)
$ErrorActionPreference = 'Stop'

$serviceName = 'PhoneUnlockService'
$serviceAccount = "NT SERVICE\$serviceName"
$providerName = 'PhoneUnlockProtoCP'
$providerClsid = '{512D55BA-432F-42D6-95EF-8071913597AA}'
$installDirectory = Join-Path $env:ProgramFiles 'PhoneUnlock'
$serviceExecutable = Join-Path $installDirectory "$serviceName.exe"
$previousExecutable = "$serviceExecutable.previous"
$providerLibrary = Join-Path $env:windir "System32\$providerName.dll"
$classKey = "HKLM:\SOFTWARE\Classes\CLSID\$providerClsid"
$providerKey = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\$providerClsid"
$dataDirectory = Join-Path $env:ProgramData 'PhoneUnlock'
$secretsDirectory = Join-Path $dataDirectory 'secrets'
$logsDirectory = Join-Path $dataDirectory 'logs'
$stateDirectory = Join-Path $dataDirectory 'state'
$pairingsDirectory = Join-Path $stateDirectory 'pairings'
$tileLogsDirectory = Join-Path $dataDirectory 'tile-logs'
$legacyTileLogsDirectory = Join-Path $env:ProgramData 'PhoneUnlockProto'
$port = 47863
$urlPrefix = "https://+:$port/v1/"
$bindingAddress = "0.0.0.0:$port"
$bindingAppId = '{78ABD9E7-8480-4CFD-B15C-43B5E2DE9C02}'
$httpParameters = 'HKLM:\SYSTEM\CurrentControlSet\Services\HTTP\Parameters'
$certificateSubject = 'CN=PhoneUnlock PC'
$firewallRule = 'PhoneUnlock (réseau local)'
$systemSid = 'S-1-5-18'
$administratorsSid = 'S-1-5-32-544'
$trustedInstallerSid = 'S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464'
$creatorOwnerSid = 'S-1-3-0'
$anyRight = -1
$writeRights = 0x500D0156

function Invoke-Native([string]$description, [scriptblock]$command) {
    $output = & $command 2>&1
    if ($LASTEXITCODE -ne 0) { throw "$description en échec : $output" }
}

function Get-PackageHashes {
    $hashes = @{}
    foreach ($line in Get-Content (Join-Path $PSScriptRoot 'SHA256SUMS.txt')) {
        $hash, $file = $line -split '\s+', 2
        $hashes[$file] = $hash
    }
    $hashes
}

function Assert-FileHash([string]$path, [string]$expected) {
    if (-not $expected -or (Get-FileHash $path -Algorithm SHA256).Hash -ne $expected) {
        throw "Empreinte SHA-256 incorrecte : $path"
    }
}

function Assert-RestrictedAccess([string]$path, [string[]]$allowedSids, [int]$rights) {
    foreach ($rule in (Get-Acl $path).GetAccessRules($true, $true, [Security.Principal.SecurityIdentifier])) {
        $granted = [int]$rule.FileSystemRights -band $rights
        if ($rule.AccessControlType -eq 'Allow' -and $granted -and $allowedSids -notcontains $rule.IdentityReference.Value) {
            throw "Droits inattendus sur $path pour $($rule.IdentityReference.Value)"
        }
    }
}

function Assert-TrustedFolder([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return }
    if ([IO.File]::GetAttributes($path) -band [IO.FileAttributes]::ReparsePoint) {
        throw "Refusé : $path est un lien vers un autre emplacement. Vérifie-le, supprime-le, puis relance l'installation."
    }
    $owner = (Get-Acl -LiteralPath $path).GetOwner([Security.Principal.SecurityIdentifier]).Value
    if ($owner -notin $systemSid, $administratorsSid) {
        throw "Refusé : $path n'a pas été créé par un administrateur (propriétaire $owner). Vérifie son contenu, supprime-le, puis relance l'installation."
    }
}

function Set-FolderAccess([string]$path, [string[]]$grants, [string[]]$allowedSids) {
    New-Item -ItemType Directory -Force $path | Out-Null
    Invoke-Native "ACL de $path" { icacls $path /inheritance:r /grant:r @grants }
    Assert-RestrictedAccess $path $allowedSids $anyRight
}

function Add-TlsBinding {
    $binding = @("ipport=$bindingAddress", "certhash=$($certificate.Thumbprint)", "appid=$bindingAppId",
        'clientcertnegotiation=enable', 'verifyclientcertrevocation=disable', 'disablelegacytls=enable')
    $output = netsh http add sslcert @binding disabletls12=enable 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "TLS 1.2 ne peut pas être désactivé sur ce Windows : $output"
        Invoke-Native 'Liaison TLS' { netsh http add sslcert @binding }
    }
}

function Test-HttpsListening {
    $deadline = (Get-Date).AddSeconds(15)
    while ((Get-Date) -lt $deadline) {
        $listeners = Get-NetTCPConnection -LocalPort $port -State Listen -ErrorAction SilentlyContinue
        if ($listeners | Where-Object OwningProcess -eq 4) { return $true }
        Start-Sleep -Milliseconds 500
    }
    $false
}

if (-not [Environment]::Is64BitProcess) { throw 'Lance ce script depuis PowerShell 64 bits.' }
if ([Environment]::OSVersion.Version.Build -lt 22000) { throw 'Windows 11 est requis.' }

$model = (Get-CimInstance Win32_ComputerSystem).Model
$isTestMachine = $model -match 'VirtualBox'
if (-not $isTestMachine -and -not $AllowPhysicalMachine) {
    throw "Refusé : installation réservée à la VM de test (modèle détecté : $model)."
}

$sourceExecutable = Join-Path $PSScriptRoot "$serviceName.exe"
$sourceLibrary = Join-Path $PSScriptRoot "$providerName.dll"
$version = (Get-Item $sourceExecutable).VersionInfo.ProductVersion
if ($version -like '*-test' -and -not $isTestMachine) {
    throw 'Refusé : cette compilation contient la commande de test authorize, réservée à la VM.'
}

$smartAppControl = (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy' -ErrorAction SilentlyContinue).VerifiedAndReputablePolicyState
if ($smartAppControl -eq 1) {
    throw 'Smart App Control est actif : il bloquerait la tuile, qui n''est pas signée par une autorité reconnue. Installation arrêtée, sans rien modifier.'
}
if ($smartAppControl -eq 2) {
    Write-Warning 'Smart App Control est en évaluation : s''il s''active, la tuile ne se chargera plus. Les autres options de connexion resteront disponibles.'
}
$passwordless = (Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\PasswordLess\Device' -ErrorAction SilentlyContinue).DevicePasswordLessBuildVersion
if ($passwordless -eq 2) {
    Write-Warning '« Hello uniquement » est activé : Windows refusera le déverrouillage par téléphone (rapport L1). Ce script ne modifie pas ce réglage.'
}

$hashes = Get-PackageHashes
Assert-FileHash $sourceExecutable $hashes["$serviceName.exe"]
Assert-FileHash $sourceLibrary $hashes["$providerName.dll"]

foreach ($path in $dataDirectory, $secretsDirectory, $logsDirectory, $stateDirectory, $pairingsDirectory, $tileLogsDirectory) {
    Assert-TrustedFolder $path
}

$existingService = Get-Service $serviceName -ErrorAction SilentlyContinue
if (-not $existingService) {
    $leftovers = $secretsDirectory, $stateDirectory | Where-Object { Test-Path $_ }
    if ($leftovers) {
        throw "Refusé : données d'une installation précédente présentes ($($leftovers -join ', ')). Lance uninstall.ps1, puis relance l'installation."
    }
}
if ($existingService -and (Test-Path $serviceExecutable)) {
    $installedVersion = (Get-Item $serviceExecutable).VersionInfo.ProductVersion
    if (-not $installedVersion) { $installedVersion = 'une version sans numéro' }
    "Mise à jour de $installedVersion vers $version"
}
else {
    "Installation de la version $version"
}

$undo = New-Object 'System.Collections.Generic.Stack[scriptblock]'
try {
    if ($existingService -and $existingService.Status -ne 'Stopped') {
        Stop-Service $serviceName -Force
        $undo.Push({ Start-Service $serviceName })
    }

    if (Test-Path $serviceExecutable) {
        Copy-Item $serviceExecutable $previousExecutable -Force
        $undo.Push({ Move-Item $previousExecutable $serviceExecutable -Force })
    }
    else {
        if (-not (Test-Path $installDirectory)) {
            $undo.Push({ Remove-Item $installDirectory -Recurse -Force -ErrorAction SilentlyContinue })
            New-Item -ItemType Directory $installDirectory | Out-Null
        }
        $undo.Push({ Remove-Item $serviceExecutable -Force -ErrorAction SilentlyContinue })
    }
    Copy-Item $sourceExecutable $serviceExecutable -Force
    Assert-FileHash $serviceExecutable $hashes["$serviceName.exe"]
    Assert-RestrictedAccess $installDirectory @($systemSid, $administratorsSid, $trustedInstallerSid, $creatorOwnerSid) $writeRights

    if (-not $existingService) {
        $undo.Push({ sc.exe delete $serviceName | Out-Null })
        Invoke-Native 'Création du service' { sc.exe create $serviceName binPath= "`"$serviceExecutable`"" start= auto obj= $serviceAccount DisplayName= 'PhoneUnlock' }
        Invoke-Native 'Type de SID du service' { sc.exe sidtype $serviceName unrestricted }
        sc.exe description $serviceName 'Déverrouillage Windows par téléphone (prototype)' | Out-Null
    }
    $serviceSid = (New-Object Security.Principal.NTAccount $serviceAccount).Translate([Security.Principal.SecurityIdentifier]).Value

    $newDataFolders = $secretsDirectory, $stateDirectory, $tileLogsDirectory | Where-Object { -not (Test-Path $_) }
    if ($newDataFolders) {
        $undo.Push({ Remove-Item $newDataFolders -Recurse -Force -ErrorAction SilentlyContinue })
    }
    $administration = "*${systemSid}:(OI)(CI)F", "*${administratorsSid}:(OI)(CI)F"
    Set-FolderAccess $dataDirectory ($administration + "${serviceAccount}:(RX)") @($systemSid, $administratorsSid, $serviceSid)
    foreach ($path in $secretsDirectory, $logsDirectory, $stateDirectory) {
        Set-FolderAccess $path ($administration + "${serviceAccount}:(OI)(CI)M") @($systemSid, $administratorsSid, $serviceSid)
    }
    Set-FolderAccess $tileLogsDirectory $administration @($systemSid, $administratorsSid)
    New-Item -ItemType Directory -Force $pairingsDirectory | Out-Null

    $certificate = Get-ChildItem Cert:\LocalMachine\My | Where-Object Subject -eq $certificateSubject | Select-Object -First 1
    if ($certificate) {
        'Certificat du PC conservé : les téléphones associés restent reconnus.'
    }
    else {
        $certificate = New-SelfSignedCertificate -Subject $certificateSubject -KeyAlgorithm ECDSA_nistP256 `
            -KeyExportPolicy NonExportable -CertStoreLocation Cert:\LocalMachine\My -NotAfter (Get-Date).AddYears(10) `
            -KeyUsage DigitalSignature -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.1') `
            -Provider 'Microsoft Software Key Storage Provider'
        $undo.Push({ Remove-Item "Cert:\LocalMachine\My\$($certificate.Thumbprint)" -Force -ErrorAction SilentlyContinue })
    }

    $boundCertificate = (Get-ItemProperty "$httpParameters\SslBindingInfo\$bindingAddress" -ErrorAction SilentlyContinue).SslCertHash
    if ($boundCertificate -and [BitConverter]::ToString($boundCertificate).Replace('-', '') -eq $certificate.Thumbprint) {
        'Liaison TLS conservée.'
    }
    else {
        $undo.Push({ netsh http delete sslcert ipport=$bindingAddress 2>&1 | Out-Null })
        netsh http delete sslcert ipport=$bindingAddress 2>&1 | Out-Null
        Add-TlsBinding
    }

    $reservedUrls = (Get-ItemProperty "$httpParameters\UrlAclInfo" -ErrorAction SilentlyContinue).PSObject.Properties.Name
    if ($reservedUrls -notcontains $urlPrefix) {
        $undo.Push({ netsh http delete urlacl url=$urlPrefix 2>&1 | Out-Null })
        netsh http delete urlacl url=$urlPrefix 2>&1 | Out-Null
        Invoke-Native 'Réservation URL' { netsh http add urlacl url=$urlPrefix user=$serviceAccount }
    }

    if (-not (Get-NetFirewallRule -DisplayName $firewallRule -ErrorAction SilentlyContinue)) {
        $undo.Push({ Get-NetFirewallRule -DisplayName $firewallRule -ErrorAction SilentlyContinue | Remove-NetFirewallRule })
        New-NetFirewallRule -DisplayName $firewallRule -Direction Inbound -Protocol TCP -LocalPort $port `
            -RemoteAddress LocalSubnet -Action Allow -Profile Any | Out-Null
    }

    $undo.Push({ Stop-Service $serviceName -Force -ErrorAction SilentlyContinue })
    Start-Service $serviceName
    if (-not (Test-HttpsListening)) { throw "Le service ne répond pas sur le port $port." }

    if (Test-Path $providerLibrary) {
        Get-ChildItem "$providerLibrary.*.old" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
        $previousLibrary = "$providerLibrary.$(Get-Date -Format yyyyMMddHHmmss).old"
        Move-Item $providerLibrary $previousLibrary
        $undo.Push({ Remove-Item $providerLibrary -Force -ErrorAction SilentlyContinue; Move-Item $previousLibrary $providerLibrary })
    }
    else {
        $undo.Push({ Remove-Item $providerLibrary -Force -ErrorAction SilentlyContinue })
    }
    Copy-Item $sourceLibrary $providerLibrary -Force
    Assert-FileHash $providerLibrary $hashes["$providerName.dll"]

    if (-not (Test-Path $providerKey)) {
        $undo.Push({
            Remove-Item $providerKey -Recurse -Force -ErrorAction SilentlyContinue
            Remove-Item $classKey -Recurse -Force -ErrorAction SilentlyContinue
        })
    }
    New-Item $classKey -Value $providerName -Force | Out-Null
    New-Item "$classKey\InprocServer32" -Value "$providerName.dll" -Force | Out-Null
    New-ItemProperty "$classKey\InprocServer32" -Name ThreadingModel -Value Apartment -Force | Out-Null
    New-Item $providerKey -Value $providerName -Force | Out-Null
}
catch {
    Write-Warning "Échec : $($_.Exception.Message)"
    Write-Warning 'Annulation des modifications.'
    while ($undo.Count -gt 0) {
        $action = $undo.Pop()
        try { & $action }
        catch { Write-Warning "Annulation incomplète : $($_.Exception.Message)" }
    }
    throw 'Installation annulée.'
}

Remove-Item $previousExecutable -Force -ErrorAction SilentlyContinue
if (Test-Path -LiteralPath $legacyTileLogsDirectory) { cmd.exe /c rmdir /s /q "$legacyTileLogsDirectory" }
Get-Service $serviceName | Format-Table Name, Status, StartType -AutoSize
"Version $version installée. Empreinte du certificat : $($certificate.Thumbprint)"
'Verrouille la session (Windows + L) pour voir la tuile Téléphone.'
