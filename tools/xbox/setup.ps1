# One-time Xbox Wireless setup. Uses Windows' built-in CAB extractor and HTTP.
[CmdletBinding()]
param([string]$PS5Address, [string]$FirmwarePath)
$ErrorActionPreference = 'Stop'

if (-not $PS5Address) { $PS5Address = Read-Host 'PS5 IP address (LibrePadPS5 must be running)' }
if ($PS5Address -notmatch '^https?://') { $PS5Address = "http://${PS5Address}:8096" }
$dashboardUri = [uri]$PS5Address
if ($dashboardUri.Scheme -notin @('http', 'https') -or -not $dashboardUri.Host -or
    $dashboardUri.UserInfo -or $dashboardUri.AbsolutePath -ne '/' -or
    $dashboardUri.Query -or $dashboardUri.Fragment) { throw 'Enter only the PS5 IP address or dashboard URL.' }
$dashboardBase = $dashboardUri.AbsoluteUri.TrimEnd('/')
$status = Invoke-RestMethod -Uri "$dashboardBase/api/xbox" -TimeoutSec 10
if (-not $status.enabled) { throw 'Load the standard LibrePadPS5 payload with Xbox Wireless support first.' }
$expectedHash = '48084d9fa53b9bb04358f3bb127b7495dc8f7bb0b3ca1437bd24ef2b6eabdf66'
# Microsoft's legacy distribution URL uses HTTP. The pinned SHA-256 below
# verifies the extracted image before any bytes are sent to the console.
$driverUrl = 'http://download.windowsupdate.com/c/msdownload/update/driver/drvs/2017/07/1cd6a87c-623f-4407-a52d-c31be49e925c_e19f60808bdcbfbd3c3df6be3e71ffc52e43261e.cab'
$taskTempDir = Join-Path ([IO.Path]::GetTempPath()) ('LibrePadPS5-' + [guid]::NewGuid().ToString('N'))
try {
    if (-not $FirmwarePath) {
        New-Item -ItemType Directory -Path $taskTempDir | Out-Null
        $cabPath = Join-Path $taskTempDir 'driver.cab'
        Write-Host 'Downloading the tested Xbox adapter firmware from Microsoft...'
        Invoke-WebRequest -UseBasicParsing -Uri $driverUrl -OutFile $cabPath -TimeoutSec 60
        & "$env:SystemRoot\System32\expand.exe" '-F:FW_ACC_00U.bin' $cabPath $taskTempDir | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'Windows could not extract the driver package.' }
        $FirmwarePath = Join-Path $taskTempDir 'FW_ACC_00U.bin'
    }
    if ((Get-Item -LiteralPath $FirmwarePath).Length -ne 70620) { throw 'Firmware size does not match the tested 2017 image.' }
    $firmwareBytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $FirmwarePath).Path)
    $hashAlgorithm = [Security.Cryptography.SHA256]::Create()
    try { $actualHash = [BitConverter]::ToString($hashAlgorithm.ComputeHash($firmwareBytes)).Replace('-', '').ToLowerInvariant() }
    finally { $hashAlgorithm.Dispose() }
    if ($actualHash -ne $expectedHash) {
        throw 'Firmware hash does not match the tested 2017 image. Run again without -FirmwarePath to download it.'
    }
    $result = Invoke-RestMethod -Uri "$dashboardBase/api/xbox/firmware" -Method Post -TimeoutSec 30 `
        -Headers @{'X-LibrePad-Control'='1'} -ContentType 'application/octet-stream' -Body $firmwareBytes
    Write-Host $result.message
} finally {
    # Only remove the two named files created in our unique temporary directory.
    if (Test-Path -LiteralPath $taskTempDir) {
        foreach ($name in @('driver.cab', 'FW_ACC_00U.bin')) {
            $taskFile = Join-Path $taskTempDir $name
            if (Test-Path -LiteralPath $taskFile) { Remove-Item -LiteralPath $taskFile -Force }
        }
        Remove-Item -LiteralPath $taskTempDir
    }
}
