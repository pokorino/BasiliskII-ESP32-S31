$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
Write-Host 'Basilisk II for ESP32-S31-Korvo-1 - Windows flasher'
Write-Host 'This writes the bootloader, partition table, and application.'
Write-Host 'Existing flash partition contents may be affected. The microSD is not written.'
Write-Host ''
foreach ($name in @('esptool.exe', 'bootloader.bin', 'partition-table.bin', 'basiliskii-s31.bin')) {
    if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot $name) -PathType Leaf)) {
        Write-Host "Missing required file: $name. Extract the complete release ZIP first." -ForegroundColor Red
        exit 1
    }
}
$ports = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
if ($ports.Count -gt 0) { Write-Host ('Available serial ports: ' + ($ports -join ', ')) }
else { Write-Host 'No serial ports found. Connect the board and install the USB serial driver.'; exit 1 }
$port = (Read-Host 'Enter the board COM port (for example COM3)').Trim().ToUpperInvariant()
if ($port -notmatch '^COM[1-9][0-9]*$' -or $port -notin $ports) {
    Write-Host 'Invalid or unavailable COM port.' -ForegroundColor Red
    exit 1
}
Write-Host "Selected port: $port"
if ((Read-Host 'Type YES to write the firmware') -cne 'YES') { Write-Host 'Cancelled.'; exit 1 }
& (Join-Path $PSScriptRoot 'esptool.exe') --chip esp32s31 --port $port --baud 921600 write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m 0x2000 (Join-Path $PSScriptRoot 'bootloader.bin') 0x8000 (Join-Path $PSScriptRoot 'partition-table.bin') 0x10000 (Join-Path $PSScriptRoot 'basiliskii-s31.bin')
if ($LASTEXITCODE -ne 0) { Write-Host 'Flash failed. Check the cable, COM port, and boot mode.' -ForegroundColor Red; exit $LASTEXITCODE }
Write-Host 'Flash verified. The board is restarting.' -ForegroundColor Green
