$ErrorActionPreference = "Stop"
$serviceName = "TinyJPG"

if (Get-Service -Name $serviceName -ErrorAction SilentlyContinue) {
  Stop-Service -Name $serviceName -ErrorAction SilentlyContinue
  sc.exe delete $serviceName | Out-Null
}
