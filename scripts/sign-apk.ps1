param(
    [string]$KeyStore = (Join-Path $env:USERPROFILE '.phoneunlock\release.jks'),
    [string]$KeyAlias = 'phoneunlock'
)
$ErrorActionPreference = 'Stop'

$root = Resolve-Path (Join-Path $PSScriptRoot '..')
$android = Join-Path $root 'android'
$packages = Join-Path $root 'build\package'
if (-not (Test-Path $KeyStore)) { throw "Magasin de clés introuvable : $KeyStore (voir docs/install.md)." }

$sdk = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { Join-Path $env:LOCALAPPDATA 'Android\Sdk' }
$buildTools = Get-ChildItem (Join-Path $sdk 'build-tools') -Directory |
    Sort-Object { [version]$_.Name } | Select-Object -Last 1
if (-not $buildTools) { throw "Outils de compilation Android introuvables dans $sdk." }
$zipalign = Join-Path $buildTools.FullName 'zipalign.exe'
$apksigner = Join-Path $buildTools.FullName 'apksigner.bat'

$versionName = [regex]::Match((Get-Content (Join-Path $android 'app\build.gradle.kts') -Raw), 'versionName = "([^"]+)"').Groups[1].Value
if (-not $versionName) { throw 'versionName introuvable dans app/build.gradle.kts.' }

Push-Location $android
try {
    & .\gradlew.bat assembleRelease --console=plain
    if ($LASTEXITCODE -ne 0) { throw 'Compilation Android en échec.' }
}
finally {
    Pop-Location
}

$unsigned = Join-Path $android 'app\build\outputs\apk\release\app-release-unsigned.apk'
$aligned = Join-Path $packages "PhoneUnlock-$versionName-aligned.apk"
$signed = Join-Path $packages "PhoneUnlock-$versionName.apk"
New-Item -ItemType Directory -Force $packages | Out-Null

& $zipalign -f 4 $unsigned $aligned
if ($LASTEXITCODE -ne 0) { throw 'Alignement en échec.' }
try {
    & $apksigner sign --ks $KeyStore --ks-key-alias $KeyAlias --out $signed $aligned
    if ($LASTEXITCODE -ne 0) { throw 'Signature en échec.' }
}
finally {
    Remove-Item $aligned -Force -ErrorAction SilentlyContinue
}

& $apksigner verify --print-certs $signed
if ($LASTEXITCODE -ne 0) { throw 'Vérification de la signature en échec.' }
Remove-Item "$signed.idsig" -Force -ErrorAction SilentlyContinue

"APK : $signed"
"SHA-256 : $((Get-FileHash $signed -Algorithm SHA256).Hash)"
