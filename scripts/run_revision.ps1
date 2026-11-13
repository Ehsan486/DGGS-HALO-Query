# Revision runs for the journal review (2026-11).  Builds the project and
# repeats the halo measurements of the revised manuscript.
#
# Output goes to results_local\ by default (-OutDir).  The records that the
# paper reports are in results_rev\ and are not touched by this script.
#
#   .\scripts\run_revision.ps1                 everything (build, tests, A, D, B)
#   .\scripts\run_revision.ps1 -Stage A        fair timing matrix only
#   .\scripts\run_revision.ps1 -Quick          short smoke run of every stage
#
# Stages
#   BUILD  configure and compile (CUDA), run the unit tests
#   F      global ring validation of every implementation, both deliveries
#   A      81 configurations x (serial flat, multithreaded flat, GPU) in one
#          process each, wall clock around the complete call, fresh container
#          and reused buffer delivery, order alternating; -Sessions passes
#   D      variants of the GPU implementation, same timer
#   B      representation study on one CPU thread
# A finished CSV is skipped (the run resumes); -Force redoes everything.
param(
  [ValidateSet('ALL','BUILD','F','A','D','B')][string]$Stage = 'ALL',
  [string]$DatasetRoot = "$PSScriptRoot\..\datasets\processed\real_world_layers",
  [string]$OutDir = 'results_local',
  [string]$BuildDir = 'build',
  [int]$CpuThreads = [Environment]::ProcessorCount,
  [int]$Sessions = 3,
  [int]$Warmup = 10,
  [int]$Repeats = 30,
  [int]$RunTimeoutMinutes = 30,
  [string]$CudaArch = '89',
  [switch]$Quick,
  [switch]$Force
)
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path "$PSScriptRoot\..").Path
$Build = if ([IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $Root $BuildDir }
$Out = if ([IO.Path]::IsPathRooted($OutDir)) { $OutDir } else { Join-Path $Root $OutDir }
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$Status = Join-Path $Out 'STATUS.txt'
function Set-Status { param([string]$Text)
  $line = "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')  $Text"
  Add-Content -LiteralPath $Status -Value $line
  Write-Host "### $line"
}
if ($Quick) { $Warmup = 2; $Repeats = 5; $Sessions = 1 }
try { Start-Transcript -LiteralPath (Join-Path $Out "run_revision_$(Get-Date -Format 'yyyyMMdd_HHmmss').log") | Out-Null } catch {}
Set-Status "START stage=$Stage quick=$Quick sessions=$Sessions warmup=$Warmup repeats=$Repeats threads=$CpuThreads"

# ---------------------------------------------------------------- BUILD
if ($Stage -in @('ALL','BUILD')) {
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
  $nvcc = Get-Command nvcc.exe -ErrorAction SilentlyContinue
  if (-not $nvcc) { throw 'nvcc.exe is missing from PATH. Install/activate the CUDA Toolkit.' }
  $cfg = @('-S', $Root, '-B', $Build, '-G', 'Ninja Multi-Config',
    '-DBAFE_ENABLE_CUDA=ON', '-DBAFE_ENABLE_H3=OFF', '-DBAFE_ENABLE_S2=OFF', '-DBAFE_REQUIRE_CUDA=ON',
    "-DCMAKE_CUDA_COMPILER=$($nvcc.Source)", "-DCMAKE_CUDA_HOST_COMPILER=$((Get-Command cl.exe).Source)",
    "-DCMAKE_CUDA_ARCHITECTURES=$CudaArch", '-DCMAKE_CUDA_FLAGS=-Xcompiler=/Zc:preprocessor')
  & cmake @cfg
  if ($LASTEXITCODE) { Set-Status 'FAILED configure'; throw 'Configuration failed.' }
  & cmake --build $Build --config Release --parallel 8 --target bafe_realworld_halo bafe_surface_tests bafe_representation_ablation bafe_global_validation
  if ($LASTEXITCODE) { Set-Status 'FAILED compile'; throw 'Compilation failed.' }
  & (Join-Path $Build 'bafe\Release\bafe_surface_tests.exe')
  if ($LASTEXITCODE) { Set-Status 'FAILED unit tests'; throw 'Correctness tests failed.' }
  # Environment record for the manuscript.
  $envFile = Join-Path $Out 'environment.txt'
  "date: $(Get-Date -Format o)" | Set-Content -LiteralPath $envFile
  try { (Get-CimInstance Win32_Processor | Select-Object -First 1 Name, NumberOfCores, NumberOfLogicalProcessors | Format-List | Out-String) | Add-Content -LiteralPath $envFile } catch {}
  try { "memory_bytes: $((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory)" | Add-Content -LiteralPath $envFile } catch {}
  try { (Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, BuildNumber | Format-List | Out-String) | Add-Content -LiteralPath $envFile } catch {}
  try { (& nvidia-smi --query-gpu=name,memory.total,driver_version --format=csv | Out-String) | Add-Content -LiteralPath $envFile } catch {}
  try { (& nvcc --version | Out-String) | Add-Content -LiteralPath $envFile } catch {}
  try { ('power plan: ' + (& powercfg /getactivescheme | Out-String)) | Add-Content -LiteralPath $envFile } catch {}
  'process control: timing programs call bafe_exp::stabilize_process() (high priority class, power throttling off); see include/bafe_exp/process_control.hpp' | Add-Content -LiteralPath $envFile
  try { (& cl.exe 2>&1 | Select-Object -First 1 | Out-String) | Add-Content -LiteralPath $envFile } catch {}
  Set-Status 'BUILD done (unit tests passed)'
}

$Exe = Join-Path $Build 'Release\bafe_realworld_halo.exe'
$Repr = Join-Path $Build 'Release\bafe_representation_ablation.exe'
$Gval = Join-Path $Build 'Release\bafe_global_validation.exe'
foreach ($e in @($Exe, $Repr, $Gval)) { if (-not (Test-Path -LiteralPath $e)) { Set-Status "FAILED missing $e"; throw "Missing executable: $e" } }
$DatasetRoot = (Resolve-Path $DatasetRoot).Path
$Datasets = @{
  buildings = Join-Path $DatasetRoot 'calgary_building_representative_points.csv'
  streets   = Join-Path $DatasetRoot 'calgary_street_samples.csv'
  boundary  = Join-Path $DatasetRoot 'calgary_boundary_samples.csv'
}
$Full = @('--cuda-all-opt', '--cuda-all-opt-except', 'lazy')   # the GPU implementation of the paper

function Invoke-Exe { param([string]$File, [string[]]$Arguments, [string]$Csv, [string]$Label)
  if ((-not $Force) -and (Test-Path -LiteralPath $Csv) -and ((Get-Item -LiteralPath $Csv).Length -gt 0)) {
    Write-Host "== ${Label}: already done, skipping"
    return
  }
  $quoted = $Arguments | ForEach-Object { if ("$_" -match '\s') { '"' + $_ + '"' } else { "$_" } }
  Write-Host ">> $Label"
  $proc = Start-Process -FilePath $File -ArgumentList $quoted -NoNewWindow -PassThru
  $null = $proc.Handle
  if (-not $proc.WaitForExit($RunTimeoutMinutes * 60000)) {
    try { $proc.Kill() } catch {}
    if (Test-Path -LiteralPath $Csv) { Remove-Item -LiteralPath $Csv -Force }
    throw "Timed out after $RunTimeoutMinutes min: $Label"
  }
  if ($proc.ExitCode -ne 0) {
    if (Test-Path -LiteralPath $Csv) { Remove-Item -LiteralPath $Csv -Force }
    throw "Run failed (exit $($proc.ExitCode)): $Label"
  }
}
function Merge-Csv { param([string]$Pattern, [string]$Target)
  $rows = @(); Get-ChildItem -Path $Out -Filter $Pattern | Where-Object { $_.Name -ne $Target } | Sort-Object Name | ForEach-Object { $rows += Import-Csv -LiteralPath $_.FullName }
  $rows | Export-Csv -LiteralPath (Join-Path $Out $Target) -NoTypeInformation
  Write-Host "Merged: $Target ($($rows.Count) rows)"
}
function Halo-Args { param([hashtable]$Config, [string]$Backends, [string]$Variant, [string]$Csv, [string]$Delivery, [string[]]$Extra)
  return @('--input', $Datasets[$Config.Dataset], '--level', $Config.Level, '--hops', $Config.Hops,
    '--max-points', $Config.Count, '--backend', $Backends, '--cpu-threads', $CpuThreads,
    '--warmup', $Warmup, '--repeats', $Repeats, '--scenario', $Config.Dataset,
    '--variant', $Variant, '--timer', 'wall', '--delivery', $Delivery, '--csv', $Csv) + $Extra
}

$Representative = @(
  @{ Name='buildings_n100000_L28_h8'; Dataset='buildings'; Count=100000; Level=28; Hops=8 },
  @{ Name='streets_n100000_L20_h8';   Dataset='streets';   Count=100000; Level=20; Hops=8 },
  @{ Name='buildings_n10000_L28_h8';  Dataset='buildings'; Count=10000;  Level=28; Hops=8 },
  @{ Name='buildings_n100000_L12_h4'; Dataset='buildings'; Count=100000; Level=12; Hops=4 },
  @{ Name='boundary_n1000_L12_h1';    Dataset='boundary';  Count=1000;   Level=12; Hops=1 }
)
$Levels = if ($Quick) { @(12, 28) } else { @(12, 20, 28) }
$HopList = if ($Quick) { @(1, 8) } else { @(1, 4, 8) }
$Counts = if ($Quick) { @(1000, 100000) } else { @(1000, 10000, 100000) }

# ---------------------------------------------------------------- F
if ($Stage -in @('ALL','F')) {
  Invoke-Exe -File $Gval -Arguments @('--cpu-threads', $CpuThreads, '--csv', (Join-Path $Out 'F_global_validation.csv')) -Csv (Join-Path $Out 'F_global_validation.csv') -Label 'F global ring validation'
  Set-Status 'F done (global ring validation passed)'
}

# ---------------------------------------------------------------- A
if ($Stage -in @('ALL','A')) {
  for ($s = 1; $s -le $Sessions; $s++) {
    $index = 0
    foreach ($d in @('buildings','streets','boundary')) {
      foreach ($l in $Levels) { foreach ($h in $HopList) { foreach ($n in $Counts) {
        $c = @{ Name="${d}_n${n}_L${l}_h${h}"; Dataset=$d; Count=$n; Level=$l; Hops=$h }
        # The order of the implementations alternates from one configuration
        # to the next (and the pattern is shifted in even sessions).
        $order = if ((($index + $s) % 2) -eq 0) { 'flat,atomic,gpu' } else { 'gpu,atomic,flat' }
        $csv = Join-Path $Out "a_s${s}_$($c.Name).csv"
        $calls = Join-Path $Out "a_s${s}_$($c.Name)_calls.txt"
        $a = (Halo-Args -Config $c -Backends $order -Variant 'gpu_implementation' -Csv $csv -Delivery 'both' -Extra ($Full + @('--samples-csv', $calls)))
        Invoke-Exe -File $Exe -Arguments $a -Csv $csv -Label "A session $s $($c.Name) [$order]"
        $index++
      } } }
    }
    Merge-Csv "a_s${s}_*.csv" "A_session${s}_matrix.csv"
    $rows = @(); Get-ChildItem -Path $Out -Filter "a_s${s}_*_calls.txt" | Sort-Object Name | ForEach-Object { $rows += Import-Csv -LiteralPath $_.FullName }
    $rows | Export-Csv -LiteralPath (Join-Path $Out "A_session${s}_calls.csv") -NoTypeInformation
    Set-Status "A session $s done"
  }
}

# ---------------------------------------------------------------- D
if ($Stage -in @('ALL','D')) {
  $Variants = @(
    @{ Name='reference';             Extra=$Full; Delivery='both' },
    @{ Name='kernel_per_ring';       Extra=($Full + @('--cuda-schedule','host')); Delivery='both' },
    @{ Name='thread_append';         Extra=($Full + @('--cuda-no-warp-append')); Delivery='both' },
    @{ Name='masked_hash';           Extra=($Full + @('--cuda-hash-mode','1')); Delivery='both'; OnlyLevels=@(12) },
    @{ Name='morton_hash';           Extra=($Full + @('--cuda-hash-mode','2')); Delivery='both' },
    @{ Name='load_0.25';             Extra=($Full + @('--cuda-load-target','0.25')); Delivery='both' },
    @{ Name='load_0.7';              Extra=($Full + @('--cuda-load-target','0.7')); Delivery='both' },
    @{ Name='scatter_clear';         Extra=@('--cuda-all-opt'); Delivery='both' },
    @{ Name='pageable_transfer';     Extra=@('--cuda-all-opt','--cuda-all-opt-except','lazy,pinned'); Delivery='fresh' },
    @{ Name='no_arrival_skip';       Extra=@('--cuda-all-opt','--cuda-all-opt-except','lazy,arrival'); Delivery='both' },
    @{ Name='no_read_before_cas';    Extra=@('--cuda-all-opt','--cuda-all-opt-except','lazy,cas'); Delivery='both' },
    @{ Name='preflight_every_call';  Extra=@('--cuda-all-opt','--cuda-all-opt-except','lazy,preflight'); Delivery='both' },
    @{ Name='separate_control_clears'; Extra=@('--cuda-all-opt','--cuda-all-opt-except','lazy,fused'); Delivery='both' },
    @{ Name='no_mechanisms';         Extra=@(); Delivery='fresh' },
    @{ Name='realized_capacity';     Extra=($Full + @('--oracle-capacity')); Delivery='both' }
  )
  foreach ($c in $Representative) {
    foreach ($v in $Variants) {
      if ($v.ContainsKey('OnlyLevels') -and ($v.OnlyLevels -notcontains $c.Level)) { continue }
      $csv = Join-Path $Out "d_$($v.Name)_$($c.Name).csv"
      try {
        $a = (Halo-Args -Config $c -Backends 'flat,gpu' -Variant $v.Name -Csv $csv -Delivery $v.Delivery -Extra ($v.Extra + @('--reference-repeats','3')))
        Invoke-Exe -File $Exe -Arguments $a -Csv $csv -Label "D $($v.Name) $($c.Name)"
      } catch { Write-Warning "Variant $($v.Name) failed on $($c.Name): $_" }
    }
  }
  Merge-Csv 'd_*.csv' 'D_controlled_variants.csv'
  Set-Status 'D done'
}

# ---------------------------------------------------------------- B
if ($Stage -in @('ALL','B')) {
  $bw = if ($Quick) { 2 } else { 5 }
  $br = if ($Quick) { 5 } else { 30 }
  $Cases = @(
    @{ Name='compact_L10';   Extra=@('--level','10','--pattern','compact') },
    @{ Name='compact_L16';   Extra=@('--level','16','--pattern','compact') },
    @{ Name='compact_L28';   Extra=@('--level','28','--pattern','compact') },
    @{ Name='scattered_L10'; Extra=@('--level','10','--pattern','scattered') },
    @{ Name='scattered_L28'; Extra=@('--level','28','--pattern','scattered') },
    @{ Name='buildings_L20'; Extra=@('--level','20','--points',$Datasets['buildings']) },
    @{ Name='buildings_L28'; Extra=@('--level','28','--points',$Datasets['buildings']) }
  )
  foreach ($t in @('tight','auto')) {
    foreach ($c in $Cases) {
      $csv = Join-Path $Out "b_${t}_$($c.Name).csv"
      try {
        Invoke-Exe -File $Repr -Arguments (@('--hops','4','--cells','100000','--warmup',$bw,'--repeats',$br,'--table',$t,'--csv',$csv) + $c.Extra) -Csv $csv -Label "B $t $($c.Name)"
      } catch { Write-Warning "Representation case $($c.Name) ($t) failed: $_" }
    }
  }
  Merge-Csv 'b_*.csv' 'B_representation.csv'
  Set-Status 'B done'
}
Set-Status "FINISHED stage=$Stage"
try { Stop-Transcript | Out-Null } catch {}
Write-Host "Done. Results in $Out"
