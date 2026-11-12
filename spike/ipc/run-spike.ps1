param(
  [string]$Mode = "all",
  [string]$Name = "gda-ipc-spike",
  [int]$Port = 39527
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$build = Join-Path $root "build"
$exe = Join-Path $build "ipc_spike_server.exe"
$vcvars = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"

if (-not (Test-Path $exe)) {
  cmd /c "`"$vcvars`" && cmake -S `"$root`" -B `"$build`" -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build `"$build`""
  if ($LASTEXITCODE -ne 0) { throw "build failed" }
}

function Wait-Listening([string]$path) {
  for ($i = 0; $i -lt 100; $i++) {
    if (Test-Path $path) {
      $text = Get-Content $path -Raw
      if ($text -match "LISTENING") { return $true }
      if ($text -match "ERROR") { return $false }
    }
    Start-Sleep -Milliseconds 100
  }
  return $false
}

function Invoke-Case([string]$mode, [string[]]$serverArgs, [string[]]$clientArgs) {
  $out = Join-Path $root "server-$mode.log"
  $err = Join-Path $root "server-$mode.err.log"
  Remove-Item -Force $out, $err -ErrorAction SilentlyContinue
  $process = Start-Process -FilePath $exe -ArgumentList $serverArgs -RedirectStandardOutput $out -RedirectStandardError $err -PassThru -WindowStyle Hidden
  try {
    if (-not (Wait-Listening $out)) {
      throw "server did not listen: $(Get-Content $out -Raw)"
    }
    $clientOut = Join-Path $root "client-$mode.json"
    $clientErr = Join-Path $root "client-$mode.err.log"
    & bun (Join-Path $root "client.ts") @clientArgs 1> $clientOut 2> $clientErr
    $code = $LASTEXITCODE
    $report = Get-Content $clientOut -Raw
    Write-Output "MODE=$mode EXIT=$code"
    Write-Output $report
  } finally {
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
  }
}

if ($Mode -eq "all" -or $Mode -eq "pipe") {
  $pipeName = "$Name-$PID"
  Invoke-Case "pipe" @("--mode", "pipe", "--name", $pipeName) @("--mode", "pipe", "--name", $pipeName)
}

if ($Mode -eq "all" -or $Mode -eq "tcp") {
  Invoke-Case "tcp" @("--mode", "tcp", "--port", "$Port") @("--mode", "tcp", "--port", "$Port")
}
