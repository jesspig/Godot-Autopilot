param(
  [string]$Version = "",
  [string]$Prefix = "",
  [string]$Archive = "",
  [string]$Project = "",
  [switch]$Skills,
  [switch]$SkillsProject,
  [switch]$Zh,
  [switch]$NoPath,
  [switch]$NoPlugin,
  [switch]$SkipChecksum,
  [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$GithubBase = if ($env:GDA_GITHUB_BASE) { $env:GDA_GITHUB_BASE.TrimEnd("/") } else { "https://github.com/jesspig/Godot-Autopilot/releases" }

if ($Skills -and $SkillsProject) {
  throw "--skills and --skills-project are mutually exclusive"
}
if ($Zh -and -not ($Skills -or $SkillsProject)) {
  throw "--zh requires --skills or --skills-project"
}

function Resolve-Latest {
  try {
    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/jesspig/Godot-Autopilot/releases/latest" `
      -Headers @{ "User-Agent" = "gda-install" } -TimeoutSec 30
    if ([string]::IsNullOrEmpty($release.tag_name)) { throw "empty tag_name" }
    return $release.tag_name
  } catch {
    throw "cannot resolve the latest release; pass -Version <version>: $($_.Exception.Message)"
  }
}

function Find-ProjectRoot([string]$Start) {
  $dir = (Get-Item -LiteralPath $Start -ErrorAction Stop).FullName
  while ($true) {
    if (Test-Path (Join-Path $dir "project.godot") -PathType Leaf) { return $dir }
    $parent = Split-Path $dir -Parent
    if ([string]::IsNullOrEmpty($parent) -or $parent -eq $dir) { return $null }
    $dir = $parent
  }
}

function Get-Platform {
  $arch = $env:PROCESSOR_ARCHITECTURE
  if ($arch -eq "AMD64" -or $arch -eq "x86_64") {
    return @{ Platform = "windows-x64"; Ext = "zip" }
  }
  throw "unsupported Windows architecture: $arch"
}

function Invoke-Process([string]$Executable, [string[]]$Arguments) {
  if ($DryRun) {
    Write-Output "[dry-run] $Executable $($Arguments -join ' ')"
    return
  }
  & $Executable @Arguments
  if ($LASTEXITCODE -ne 0) {
    throw "command failed ($LASTEXITCODE): $Executable $($Arguments -join ' ')"
  }
}

function Assert-Checksum([string]$File, [string]$Sums) {
  if (-not (Test-Path $Sums)) {
    if ($SkipChecksum) { Write-Warning "checksum file missing; skipping checksum"; return }
    throw "checksum file missing: $Sums"
  }
  $line = Get-Content $Sums | Where-Object {
    $parts = $_ -split '\s+'
    $parts.Count -ge 2 -and ($parts[0] -eq $asset -or $parts[-1] -eq $asset)
  } | Select-Object -First 1
  if (-not $line) {
    if ($SkipChecksum) { Write-Warning "$asset is not listed in checksums; skipping"; return }
    throw "$asset is not listed in $Sums"
  }
  $parts = $line -split '\s+'
  $expected = if ($parts[0] -eq $asset) { $parts[1] } else { $parts[0] }
  $actual = (Get-FileHash $File -Algorithm SHA256).Hash.ToLower()
  if ($expected.ToLower() -ne $actual) { throw "checksum mismatch for $asset" }
}

$projectInput = $Project
$projectSource = ""
if ([string]::IsNullOrEmpty($Project) -and $env:GDA_PROJECT) {
  $Project = $env:GDA_PROJECT
  $projectInput = $Project
}
if (-not [string]::IsNullOrEmpty($Project)) {
  $resolved = Get-Item -LiteralPath $Project -ErrorAction SilentlyContinue
  if ($null -eq $resolved -or -not (Test-Path (Join-Path $resolved.FullName "project.godot") -PathType Leaf)) {
    throw "not a Godot project (project.godot missing): $projectInput"
  }
  $Project = $resolved.FullName
  $projectSource = "explicit"
} elseif (-not $NoPlugin -or $SkillsProject) {
  $Project = Find-ProjectRoot (Get-Location).Path
  if ($null -ne $Project) {
    $projectSource = if ((Get-Location).Path -eq $Project) { "cwd" } else { "parent" }
  }
}
if ($SkillsProject -and [string]::IsNullOrEmpty($Project)) {
  throw "-SkillsProject requires a Godot project; pass -Project <dir>"
}

if ($Version -eq "") { $Version = $env:GDA_VERSION }
if ($Version -eq "") {
  if ($Archive -ne "") {
    if ((Test-Path $Archive -PathType Container) -and (Test-Path (Join-Path $Archive "VERSION"))) {
      $Version = (Get-Content (Join-Path $Archive "VERSION") -Raw).Trim()
    } else {
      throw "a version is required with -Archive (-Version or GDA_VERSION)"
    }
  } else {
    $Version = Resolve-Latest
  }
}
$Version = $Version.Trim()
$Version = $Version -replace '^v', ''

$platformInfo = Get-Platform
$platform = $platformInfo.Platform
$ext = $platformInfo.Ext
if ($Prefix -eq "") { $Prefix = Join-Path $HOME ".godot-autopilot\bin" }
$asset = "gda-$Version-$platform.$ext"
$cacheDir = Join-Path $HOME ".godot-autopilot\cache\cli\$Version\$platform"
$sumsPath = Join-Path $cacheDir "sha256sums.txt"
$assetPath = Join-Path $cacheDir $asset

New-Item -ItemType Directory -Force -Path $cacheDir | Out-Null

if ($Archive -ne "") {
  $source = if (Test-Path $Archive -PathType Container) { Join-Path $Archive $asset } else { $Archive }
  if (-not (Test-Path $source)) { throw "archive not found: $source" }
  Copy-Item -Force $source $assetPath
  $archiveSums = if (Test-Path $Archive -PathType Container) {
    Join-Path $Archive "sha256sums.txt"
  } else {
    Join-Path (Split-Path $Archive -Parent) "sha256sums.txt"
  }
  if (-not $DryRun) { Assert-Checksum $assetPath $archiveSums }
} else {
  $assetTag = ""
  foreach ($tag in @("v$Version", "$Version")) {
    $url = "$GithubBase/download/$tag/$asset"
    try {
      if ($DryRun) { Write-Output "[dry-run] Invoke-WebRequest $url"; $assetTag = $tag; break }
      Invoke-WebRequest -Uri $url -OutFile $assetPath -UseBasicParsing -TimeoutSec 60
      $assetTag = $tag
      break
    } catch {
      Write-Warning "download failed: $url"
    }
  }
  if ($assetTag -eq "") {
    throw "download failed for gda $Version ($platform); use -Archive with a local release asset"
  }
  $sumsUrl = "$GithubBase/download/$assetTag/sha256sums.txt"
  if ($DryRun) {
    Write-Output "[dry-run] Invoke-WebRequest $sumsUrl"
  } else {
    try {
      Invoke-WebRequest -Uri $sumsUrl -OutFile $sumsPath -UseBasicParsing -TimeoutSec 30
      Assert-Checksum $assetPath $sumsPath
    } catch {
      if ($SkipChecksum) {
        Write-Warning "checksum unavailable; skipping checksum"
      } else {
        throw "cannot download $sumsUrl; pass -SkipChecksum to install without verification"
      }
    }
  }
}

$unpack = Join-Path $cacheDir "unpacked"
if ($DryRun) {
  Write-Output "[dry-run] Expand-Archive $assetPath -> $unpack"
  Write-Output "[dry-run] install $asset to $Prefix"
  if (-not [string]::IsNullOrEmpty($Project) -and -not $NoPlugin) {
    Write-Output "[dry-run] $Prefix\gda.exe install plugin --project $Project"
  } else {
    Write-Output "[dry-run] plugin skipped"
  }
  if ($Skills) {
    $suffix = if ($Zh) { " --zh" } else { "" }
    Write-Output "[dry-run] $Prefix\gda.exe install skills --force$suffix"
  } elseif ($SkillsProject) {
    $suffix = if ($Zh) { " --zh" } else { "" }
    Write-Output "[dry-run] $Prefix\gda.exe install skills --path $Project\.agents\skills --force$suffix"
  } else {
    Write-Output "[dry-run] skills skipped"
  }
  exit 0
}

if (Test-Path $unpack) { Remove-Item -Recurse -Force $unpack }
New-Item -ItemType Directory -Force -Path $unpack | Out-Null
Expand-Archive -Path $assetPath -DestinationPath $unpack -Force
$bin = Get-ChildItem $unpack -Recurse -File | Where-Object { $_.Name -eq "gda.exe" -or $_.Name -eq "gda" } | Select-Object -First 1
if (-not $bin) { throw "no gda binary in $unpack" }

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
    $newPath = (($parts + $Prefix) -join ";")
    [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
    Write-Output "updated user Path: $newPath"
  }
}

Write-Output "installed gda $Version to $target"
if (-not [string]::IsNullOrEmpty($Project)) {
  Write-Output "project: $Project ($projectSource)"
}

if (-not [string]::IsNullOrEmpty($Project) -and -not $NoPlugin) {
  Invoke-Process $target @("install", "plugin", "--project", $Project)
  if (-not (Test-Path (Join-Path $Project "addons\godot-autopilot\godot-autopilot.gdextension"))) {
    throw "plugin install did not produce addons/godot-autopilot/godot-autopilot.gdextension"
  }
  Write-Output "installed plugin to $(Join-Path $Project 'addons\godot-autopilot')"
} elseif (-not [string]::IsNullOrEmpty($Project)) {
  Write-Output "plugin: skipped (-NoPlugin)"
} else {
  Write-Output "plugin: skipped (no project.godot found; pass -Project <dir>)"
}

if ($Skills) {
  $skillsDir = Join-Path $HOME ".agents\skills"
  $skillArgs = @("install", "skills", "--force")
  if ($Zh) { $skillArgs += "--zh" }
  Invoke-Process $target $skillArgs
  if (-not (Test-Path (Join-Path $skillsDir "godot-autopilot\SKILL.md"))) {
    throw "skills install did not produce $(Join-Path $skillsDir 'godot-autopilot\SKILL.md')"
  }
  Write-Output "installed skills to $skillsDir"
} elseif ($SkillsProject) {
  $skillsDir = Join-Path $Project ".agents\skills"
  $skillArgs = @("install", "skills", "--force", "--path", $skillsDir)
  if ($Zh) { $skillArgs += "--zh" }
  Invoke-Process $target $skillArgs
  if (-not (Test-Path (Join-Path $skillsDir "godot-autopilot\SKILL.md"))) {
    throw "skills install did not produce $(Join-Path $skillsDir 'godot-autopilot\SKILL.md')"
  }
  Write-Output "installed skills to $skillsDir"
} else {
  Write-Output "skills: skipped"
}

Write-Output "next: $target --version"
Write-Output "next: $target start --project <project>"
Write-Output "next: $target status --json"
