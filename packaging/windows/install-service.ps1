param(
  [string]$BinaryPath = "$PSScriptRoot\..\..\bin\tinyjpg.exe",
  [string]$ConfigPath = "$env:ProgramData\TinyJPG\tinyjpg.toml"
)

$ErrorActionPreference = "Stop"
$serviceName = "TinyJPG"
$programData = Split-Path -Parent $ConfigPath

New-Item -ItemType Directory -Force -Path $programData | Out-Null

if (-not (Test-Path $ConfigPath)) {
  & $BinaryPath config print --defaults | Out-File -FilePath $ConfigPath -Encoding utf8
}

$arguments = "watch --config `"$ConfigPath`""
$imagePath = "`"$BinaryPath`" $arguments"

if (Get-Service -Name $serviceName -ErrorAction SilentlyContinue) {
  Stop-Service -Name $serviceName -ErrorAction SilentlyContinue
  sc.exe config $serviceName binPath= $imagePath start= auto | Out-Null
} else {
  New-Service -Name $serviceName -DisplayName "TinyJPG image optimizer" `
    -Description "Optimizes watched image directories." -BinaryPathName $imagePath `
    -StartupType Automatic | Out-Null
}

sc.exe failure $serviceName reset= 60 actions= restart/5000/restart/30000/""/60000 | Out-Null
Start-Service -Name $serviceName
