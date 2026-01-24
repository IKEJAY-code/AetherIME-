Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path "$PSScriptRoot/..").Path
$pidFile = Join-Path $repoRoot ".shurufa_engine.pid"

if (!(Test-Path $pidFile)) {
  Write-Host "engine pid file not found: $pidFile"
  exit 0
}

$pid = (Get-Content $pidFile -Raw).Trim()
if ($pid -eq "") { exit 0 }

try {
  Stop-Process -Id ([int]$pid) -Force
  Write-Host "core_engine stopped pid=$pid"
} catch {
  Write-Host "failed to stop core_engine pid=$pid: $($_.Exception.Message)"
} finally {
  Remove-Item $pidFile -ErrorAction SilentlyContinue
}

