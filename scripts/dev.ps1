param(
  [int]$Port = 48080,
  [ValidateSet("Debug","Release")]
  [string]$Configuration = "Debug",
  [ValidateSet("x64","Win32")]
  [string]$Platform = "x64"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

& "$PSScriptRoot/start_engine.ps1" -Port $Port -Configuration $Configuration
& "$PSScriptRoot/register_tip.ps1" -Configuration $Configuration -Platform $Platform

Write-Host ""
Write-Host "Dev mode ready:"
Write-Host "- engine: 127.0.0.1:$Port"
Write-Host "- TIP registered"
Write-Host ""

