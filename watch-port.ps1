# Watch for USB serial port appearance and auto-trigger upload
# Run: powershell -NoProfile -ExecutionPolicy Bypass -File watch-port.ps1

$lastPorts = @()

while ($true) {
  $ports = (Get-WmiObject Win32_SerialPort).Name | Where-Object { $_ -match "COM" }

  if ($ports.Count -eq 0) {
    $ports = @()
  } elseif ($ports -isnot [array]) {
    $ports = @($ports)
  }

  $newPorts = @($ports | Where-Object { $lastPorts -notcontains $_ })

  if ($newPorts.Count -gt 0) {
    Write-Host "Device detected: $($newPorts -join ', ')"
    Write-Host "Triggering PlatformIO upload..."
    & pio run -t upload
  }

  $lastPorts = $ports
  Start-Sleep -Seconds 2
}
