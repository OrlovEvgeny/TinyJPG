param(
  [string]$Version
)

$ErrorActionPreference = "Stop"
$ManifestUrl = "https://tinyjpg.eorlov.org/tinyjpg/manifest.json"
$tmpdir = $null

function Test-IsAdmin {
  $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
  $principal = New-Object Security.Principal.WindowsPrincipal($identity)
  return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

try {
  try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
  } catch {
    # Older PowerShell hosts may not expose this enum; continue with host defaults.
  }

  try {
    $arch = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
  } catch {
    $arch = $env:PROCESSOR_ARCHITECTURE
  }

  switch -Regex ($arch) {
    "^(X64|AMD64)$" { $platformKey = "windows-x86_64"; break }
    default { throw "Unsupported architecture: $arch" }
  }

  Write-Host "Fetching TinyJPG manifest..."
  $manifest = Invoke-RestMethod -Uri $ManifestUrl

  if ([string]::IsNullOrWhiteSpace($Version)) {
    $selectedVersion = $manifest.latest
  } else {
    $selectedVersion = $Version
  }

  if ([string]::IsNullOrWhiteSpace($selectedVersion)) {
    throw "Failed to determine TinyJPG version from manifest"
  }

  $versionEntry = $manifest.versions.PSObject.Properties[$selectedVersion].Value
  if ($null -eq $versionEntry) {
    throw "TinyJPG version $selectedVersion was not found in the manifest"
  }

  $platform = $versionEntry.platforms.PSObject.Properties[$platformKey].Value
  if ($null -eq $platform) {
    throw "No download is available for platform $platformKey in TinyJPG $selectedVersion"
  }

  $url = $platform.url
  $expectedHash = $platform.sha256
  if ([string]::IsNullOrWhiteSpace($url)) {
    throw "No download URL found for platform $platformKey"
  }
  if ([string]::IsNullOrWhiteSpace($expectedHash)) {
    throw "No SHA256 found for platform $platformKey"
  }

  $tmpdir = Join-Path ([System.IO.Path]::GetTempPath()) ([System.IO.Path]::GetRandomFileName())
  New-Item -ItemType Directory -Path $tmpdir | Out-Null
  $zipPath = Join-Path $tmpdir "tinyjpg.zip"

  Write-Host "Downloading TinyJPG $selectedVersion for $platformKey..."
  Invoke-WebRequest -Uri $url -OutFile $zipPath

  $hash = (Get-FileHash -Path $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
  if ($hash -ne $expectedHash.ToLowerInvariant()) {
    throw "SHA256 mismatch! Expected $expectedHash, got $hash"
  }

  if ($env:INSTALL_DIR) {
    $installDir = $env:INSTALL_DIR
  } elseif (Test-IsAdmin) {
    $installDir = Join-Path $env:ProgramFiles "TinyJPG"
  } else {
    $installDir = Join-Path $env:LOCALAPPDATA "TinyJPG"
  }

  Write-Host "Installing to $installDir"
  $extractDir = Join-Path $tmpdir "extracted"
  Expand-Archive -Path $zipPath -DestinationPath $extractDir -Force
  New-Item -ItemType Directory -Path $installDir -Force | Out-Null

  $tinyjpgExe = Join-Path $extractDir "tinyjpg.exe"
  $tjExe = Join-Path $extractDir "tj.exe"
  if (-not (Test-Path $tinyjpgExe)) {
    throw "Archive did not contain tinyjpg.exe"
  }
  if (-not (Test-Path $tjExe)) {
    throw "Archive did not contain tj.exe"
  }

  Copy-Item $tinyjpgExe (Join-Path $installDir "tinyjpg.exe") -Force
  Copy-Item $tjExe (Join-Path $installDir "tj.exe") -Force

  $userPath = [System.Environment]::GetEnvironmentVariable("Path", "User")
  $pathEntries = @()
  if ($userPath) {
    $pathEntries = $userPath -split ';'
  }
  if ($pathEntries -notcontains $installDir) {
    $newUserPath = if ($userPath) { "$userPath;$installDir" } else { $installDir }
    [System.Environment]::SetEnvironmentVariable("Path", $newUserPath, "User")
    $env:Path = "$env:Path;$installDir"
  }

  & (Join-Path $installDir "tinyjpg.exe") --version | Out-Host
  Write-Host "TinyJPG $selectedVersion installed successfully!"
} catch {
  Write-Error $_.Exception.Message
  exit 1
} finally {
  if ($tmpdir -and (Test-Path $tmpdir)) {
    Remove-Item -Recurse -Force $tmpdir
  }
}
