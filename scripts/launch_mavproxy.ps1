# ============================================================
# follower - MAVProxy bridge (Windows)
# Same as launch_mavproxy.sh, notes are in that file (USB tethered only
# until there's a telemetry radio or an ESP32 wifi bridge).
#
# Usage:
#   .\launch_mavproxy.ps1 sitl
#   .\launch_mavproxy.ps1 usb
# ============================================================

param(
    [string]$Mode = "sitl"
)

$GcsBackendPort = 14550
$MonitorPort    = 14551

if ($Mode -eq "sitl") {
    Write-Host "[launch_mavproxy] Bridging SITL -> backend:$GcsBackendPort + monitor:$MonitorPort"
    Write-Host "[launch_mavproxy] Start SITL separately first:"
    Write-Host "                  sim_vehicle.py -v ArduCopter --out=udp:127.0.0.1:14560"
    mavproxy.py `
        --master=udp:127.0.0.1:14560 `
        --out=udp:127.0.0.1:$GcsBackendPort `
        --out=udp:127.0.0.1:$MonitorPort

} elseif ($Mode -eq "usb") {
    # CHANGE THIS to your FC's actual COM port (Device Manager -> Ports)
    $FcPort = "COM3"
    $FcBaud = 115200

    Write-Host "[launch_mavproxy] Bridging real FC ($FcPort @ $FcBaud) -> backend:$GcsBackendPort + monitor:$MonitorPort"
    mavproxy.py `
        --master="${FcPort},${FcBaud}" `
        --out=udp:127.0.0.1:$GcsBackendPort `
        --out=udp:127.0.0.1:$MonitorPort

} else {
    Write-Host "Usage: .\launch_mavproxy.ps1 [sitl|usb]"
    exit 1
}
