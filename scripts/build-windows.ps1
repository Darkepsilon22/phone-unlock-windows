param([switch]$TestCommands)
$ErrorActionPreference = 'Stop'

$root = Resolve-Path (Join-Path $PSScriptRoot '..')
$source = Join-Path $root 'windows'
$build = Join-Path $root 'build\windows'
$deploy = Join-Path $root 'build\deploy'
$vcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { 'E:\dev\vcpkg' }
$toolchain = Join-Path $vcpkgRoot 'scripts\buildsystems\vcpkg.cmake'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'MSVC introuvable.' }
$cmakeBin = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
$cmake = Join-Path $cmakeBin 'cmake.exe'
$ctest = Join-Path $cmakeBin 'ctest.exe'

& $cmake -S $source -B $build -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_TOOLCHAIN_FILE=$toolchain" -DVCPKG_TARGET_TRIPLET=x64-windows-static "-DPHONEUNLOCK_TEST_COMMANDS=$(if ($TestCommands) { 'ON' } else { 'OFF' })"
if ($LASTEXITCODE -ne 0) { throw 'Configuration CMake en échec.' }
& $cmake --build $build --config Release
if ($LASTEXITCODE -ne 0) { throw 'Compilation en échec.' }
& $ctest --test-dir $build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests en échec.' }

if (Test-Path $deploy) { Remove-Item $deploy -Recurse -Force }
New-Item -ItemType Directory -Force $deploy | Out-Null
Copy-Item (Join-Path $build 'credential-provider\Release\PhoneUnlockProtoCP.dll') $deploy -Force
Copy-Item (Join-Path $build 'service\Release\PhoneUnlockService.exe') $deploy -Force
Copy-Item (Join-Path $PSScriptRoot 'vm-*.ps1') $deploy -Force
Get-ChildItem $deploy
