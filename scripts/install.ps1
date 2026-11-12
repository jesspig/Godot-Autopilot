param(
  [string]$Version = "",
  [string]$Prefix = "",
  [string]$Archive = "",
  [switch]$NoPath,
  [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$GithubBase = if ($env:GDA_GITHUB_BASE) { $env:GDA_GITHUB_BASE.TrimEnd("/") } else { "https://github.com/jesspig/Godot-Autopilot/releases" }
$AtomGitBase = if ($env:GDA_ATOMGIT_BASE) { $env:GDA_ATOMGIT_BASE.TrimEnd("/") } else { "" }

function Get-Platform {
  $arch = $env:PROCESSOR_ARCHITECTURE
  if ($arch -eq "AMD64" -or $arch -eq "x86_64") { return "windows-x64" }
  throw "unsupported Windows architecture: $arch"
}

function Invoke-Step([scriptblock]$block) {
  if ($DryRun) { Write-Output "[dry-run] $($block.ToString().Trim())"; return }
  & $block
}

$platform = Get-Platform
if ($Version -eq "") { $Version = $env:GDA_VERSION }
if ($Version -eq "") {
  if ($Archive -ne "" -and (Test-Path (Join-Path $Archive "VERSION"))) {
    $Version = (Get-Content (Join-Path $Archive "VERSION") -Raw).Trim()
  } else {
    throw "a version is required (-Version or GDA_VERSION)"
  }
}
if ($Prefix -eq "") { $Prefix = Join-Path $HOME ".godot-autopilot\bin" }
$asset = "gda-$Version-$platform.zip"
$cacheDir = Join-Path $HOME ".godot-autopilot\cache\cli\$Version\$platform"
$sumsPath = Join-Path $cacheDir "SHA256SUMS"
$assetPath = Join-Path $cacheDir $asset

function Assert-Checksum([string]$file, [string]$sums) {
  if (-not (Test-Path $sums)) { Write-Warning "SHA256SUMS missing; skipping checksum"; return }
  $line = Get-Content $sums | Where-Object {
    $parts = $_ -split '\s+'
    $parts.Count -ge 2 -and ($parts[0] -eq $asset -or $parts[-1] -eq $asset)
  } | Select-Object -First 1
  if (-not $line) { throw "$asset is not listed in SHA256SUMS" }
  $parts = $line -split '\s+'
  $expected = if ($parts[0] -eq $asset) { $parts[1] } else { $parts[0] }
  $actual = (Get-FileHash $file -Algorithm SHA256).Hash.ToLower()
  if ($expected.ToLower() -ne $actual) { throw "checksum mismatch for $asset" }
}

New-Item -ItemType Directory -Force -Path $cacheDir | Out-Null
if ($Archive -ne "") {
  $source = if (Test-Path $Archive -PathType Container) { Join-Path $Archive $asset } else { $Archive }
  if (-not (Test-Path $source)) { throw "archive not found: $source" }
  Copy-Item -Force $source $assetPath
  $archiveSums = if (Test-Path (Join-Path $Archive "SHA256SUMS")) { Join-Path $Archive "SHA256SUMS" } else { $sumsPath }
  Assert-Checksum $assetPath $archiveSums
} else {
  $urls = @("$GithubBase/download/v$Version/$asset")
  if ($AtomGitBase -ne "") { $urls += "$AtomGitBase/download/v$Version/$asset" }
  $downloaded = $false
  foreach ($url in $urls) {
    try {
      if ($DryRun) { Write-Output "[dry-run] Invoke-WebRequest $url"; $downloaded = $true; break }
      Invoke-WebRequest -Uri $url -OutFile $assetPath -UseBasicParsing -TimeoutSec 60
      $downloaded = $true
      break
    } catch {
      Write-Warning "download failed: $url"
    }
  }
  if (-not $downloaded) { throw "download failed; use -Archive with a local release asset" }
  try {
    if (-not $DryRun) { Invoke-WebRequest -Uri "$GithubBase/download/v$Version/SHA256SUMS" -OutFile $sumsPath -UseBasicParsing -TimeoutSec 30 }
  } catch {
    Write-Warning "SHA256SUMS unavailable; skipping checksum"
  }
  if (Test-Path $sumsPath) { Assert-Checksum $assetPath $sumsPath }
}

$unpack = Join-Path $cacheDir "unpacked"
if (Test-Path $unpack) { Remove-Item -Recurse -Force $unpack }
New-Item -ItemType Directory -Force -Path $unpack | Out-Null
if ($DryRun) {
  Write-Output "[dry-run] Expand-Archive $assetPath -> $unpack"
} else {
  Expand-Archive -Path $assetPath -DestinationPath $unpack -Force
}
if ($DryRun) {
  Write-Output "[dry-run] install $asset to $Prefix and update user Path"
  exit 0
}
$bin = Get-ChildItem $unpack -Recurse -File | Where-Object { $_.Name -eq "gda.exe" -or $_.Name -eq "gda" } | Select-Object -First 1
if (-not $bin) { throw "no gda.exe in $unpack" }

New-Item -ItemType Directory -Force -Path $Prefix | Out-Null
$target = Join-Path $Prefix "gda.exe"
$tmp = "$target.new-$PID"
Copy-Item -Force $bin.FullName $tmp
if (Test-Path $target) {
  $old = "$target.old-$(Get-Date -UFormat %s)"
  Move-Item -Force $target $old
}
Move-Item -Force $tmp $target

if (-not $NoPath) {
  $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
  $parts = @($userPath -split ";" | Where-Object { $_ -ne "" })
  if ($parts -notcontains $Prefix) {
    if ($DryRun) {
      Write-Output "[dry-run] add $Prefix to user Path"
    } else {
      $newPath = (($parts + $Prefix) -join ";")
      [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
      Write-Output "updated user Path: $newPath"
    }
  }
}

Write-Output "installed gda $Version to $target"
Write-Output "next: $target install skills"
Write-Output "next: $target install plugin -Project <project>"
