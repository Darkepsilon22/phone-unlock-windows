$ErrorActionPreference = 'Stop'

$root = Resolve-Path (Join-Path $PSScriptRoot '..')
$source = Join-Path $root 'windows\credential-provider'
$build = Join-Path $root 'build\credential-provider'
$deploy = Join-Path $root 'build\deploy'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'MSVC introuvable.' }
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'

& $cmake -S $source -B $build -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'Configuration CMake en échec.' }
& $cmake --build $build --config Release
if ($LASTEXITCODE -ne 0) { throw 'Compilation en échec.' }

New-Item -ItemType Directory -Force $deploy | Out-Null
Copy-Item (Join-Path $build 'Release\PhoneUnlockProtoCP.dll') $deploy -Force
Copy-Item (Join-Path $PSScriptRoot 'vm-install-cp.ps1'), (Join-Path $PSScriptRoot 'vm-uninstall-cp.ps1') $deploy -Force
Get-ChildItem $deploy
