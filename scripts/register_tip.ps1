param(
  [ValidateSet("Debug","Release")]
  [string]$Configuration = "Debug",
  [ValidateSet("x64","Win32")]
  [string]$Platform = "x64"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path "$PSScriptRoot/..").Path
$buildDir = Join-Path $repoRoot "build/tsf_shell/$Platform"

cmake -S (Join-Path $repoRoot "tsf_shell") -B $buildDir -G "Visual Studio 17 2022" -A $Platform | Out-Host
cmake --build $buildDir --config $Configuration | Out-Host

$dll = Join-Path $buildDir "bin/$Configuration/shurufa_tsf_shell.dll"
if (!(Test-Path $dll)) { throw "TIP dll not found: $dll" }

Write-Host "Registering TIP: $dll"
regsvr32 /s $dll
Write-Host "Registered. You may need to re-open apps or sign out/in for TSF list to refresh."

