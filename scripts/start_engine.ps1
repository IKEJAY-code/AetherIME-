param(
  [int]$Port = 48080,
  [ValidateSet("Debug","Release")]
  [string]$Configuration = "Debug"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path "$PSScriptRoot/..").Path
$engineDir = Join-Path $repoRoot "core_engine"
$pidFile = Join-Path $repoRoot ".shurufa_engine.pid"

Push-Location $engineDir
try {
  $env:SHURUFA_ENGINE_PORT = "$Port"
  if ($Configuration -eq "Release") {
    cargo build --release
  } else {
    cargo build
  }

  $exe = if ($Configuration -eq "Release") { "target/release/core_engine.exe" } else { "target/debug/core_engine.exe" }
  if (!(Test-Path $exe)) { throw "engine binary not found: $exe" }

  $p = Start-Process -FilePath $exe -ArgumentList @() -PassThru -WindowStyle Hidden
  Set-Content -Path $pidFile -Value $p.Id -Encoding ascii
  Write-Host "core_engine started pid=$($p.Id) port=$Port"
} finally {
  Pop-Location
}
