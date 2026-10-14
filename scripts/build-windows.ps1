param([switch]$TestCommands)
$ErrorActionPreference = 'Stop'

$root = Resolve-Path (Join-Path $PSScriptRoot '..')
$source = Join-Path $root 'windows'
$build = Join-Path $root 'build\windows'
$packages = Join-Path $root 'build\package'
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

$binaries = @(
    (Join-Path $build 'service\Release\PhoneUnlockService.exe'),
    (Join-Path $build 'credential-provider\Release\PhoneUnlockProtoCP.dll')
)
$version = (Get-Item $binaries[0]).VersionInfo.ProductVersion
$package = Join-Path $packages "PhoneUnlock-$version"
if (Test-Path $package) { Remove-Item $package -Recurse -Force }
New-Item -ItemType Directory -Force $package | Out-Null
Copy-Item $binaries $package -Force
Copy-Item (Join-Path $PSScriptRoot 'install.ps1'), (Join-Path $PSScriptRoot 'uninstall.ps1') $package -Force

$sums = foreach ($binary in $binaries) {
    $name = Split-Path $binary -Leaf
    "$((Get-FileHash (Join-Path $package $name) -Algorithm SHA256).Hash)  $name"
}
Set-Content (Join-Path $package 'SHA256SUMS.txt') $sums -Encoding Ascii

"Paquet : $package"
$sums
