# Configure, compile and test the project on Windows.
#
#   powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1            CUDA build
#   powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -CpuOnly   without CUDA
#   powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -All       every program
#
# Needs the Visual Studio C++ build tools, CMake and Ninja, and for the CUDA
# build the CUDA toolkit (nvcc.exe on PATH).  The script locates Visual Studio.
# -CudaArch selects the compute capability (default 89, RTX 40 series).
param(
  [string]$CudaArch = '89',
  [string]$BuildDir = 'build',
  [switch]$CpuOnly,
  [switch]$All
)
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path "$PSScriptRoot\..").Path
$Build = if ([IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $Root $BuildDir }
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
  $where = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
  if (-not (Test-Path -LiteralPath $where)) { throw 'Install Visual Studio C++ Build Tools first.' }
  $vs = & $where -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if (-not $vs) { throw 'No Visual Studio C++ installation found.' }
  & (Join-Path $vs 'Common7/Tools/Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
}
foreach ($tool in @('cmake', 'ninja', 'cl.exe')) {
  if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "Required tool missing: $tool" }
}
$cuda = -not $CpuOnly
$cfg = @('-S', $Root, '-B', $Build, '-G', 'Ninja Multi-Config',
  "-DBAFE_ENABLE_CUDA=$(if ($cuda) { 'ON' } else { 'OFF' })",
  '-DBAFE_ENABLE_H3=OFF', '-DBAFE_ENABLE_S2=OFF', '-DBAFE_REQUIRE_CUDA=ON')
if ($cuda) {
  $nvcc = Get-Command nvcc.exe -ErrorAction SilentlyContinue
  if (-not $nvcc) { throw 'nvcc.exe is missing from PATH. Install or activate the CUDA Toolkit, or use -CpuOnly.' }
  $cfg += @("-DCMAKE_CUDA_COMPILER=$($nvcc.Source)", "-DCMAKE_CUDA_HOST_COMPILER=$((Get-Command cl.exe).Source)",
    "-DCMAKE_CUDA_ARCHITECTURES=$CudaArch", '-DCMAKE_CUDA_FLAGS=-Xcompiler=/Zc:preprocessor')
}
& cmake @cfg
if ($LASTEXITCODE) { throw 'Configuration failed.' }
if ($All) {
  & cmake --build $Build --config Release --parallel 8
} else {
  & cmake --build $Build --config Release --parallel 8 --target bafe_realworld_halo bafe_surface_tests bafe_representation_ablation bafe_global_validation halo_example
}
if ($LASTEXITCODE) { throw 'Compilation failed.' }
& (Join-Path $Build 'bafe\Release\bafe_surface_tests.exe')
if ($LASTEXITCODE) { throw 'Correctness tests failed.' }
& (Join-Path $Build 'Release\halo_example.exe')
if ($LASTEXITCODE) { throw 'The example program failed.' }
Write-Host "Build and tests finished. Programs are in $Build\Release"
