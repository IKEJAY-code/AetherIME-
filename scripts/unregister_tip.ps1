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
$dll = Join-Path $buildDir "bin/$Configuration/shurufa_tsf_shell.dll"

if (!(Test-Path $dll)) {
  Write-Host "TIP dll not found (skip regsvr32 /u): $dll"
  exit 0
}

Write-Host "Unregistering TIP: $dll"
regsvr32 /u /s $dll
Write-Host "Unregistered."

