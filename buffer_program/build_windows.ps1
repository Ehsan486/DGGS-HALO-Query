param([switch]$CpuOnly)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $build = if ($CpuOnly) { 'build' } else { 'build_cuda' }
    $cuda = if ($CpuOnly) { 'OFF' } else { 'ON' }
    cmake -S . -B $build -G 'Visual Studio 18 2026' -A x64 "-DBAFE_ENABLE_CUDA=$cuda" -DCMAKE_CUDA_ARCHITECTURES=89
    if ($LASTEXITCODE -ne 0) { throw 'Configuration failed.' }
    cmake --build $build --config Release -j 8
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    ctest --test-dir $build -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed; do not use this build.' }
    Write-Host "Verified executable: $PSScriptRoot\$build\Release\atlas_buffer.exe"
} finally { Pop-Location }
