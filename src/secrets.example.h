#pragma once
/*  secrets.example.h — template for src/secrets.h.

    Copy this file to src/secrets.h and fill in real values. secrets.h is
    gitignored; this example is the only one that gets committed.
      PowerShell:  Copy-Item src\secrets.example.h src\secrets.h

    Used by the thermal streaming module (esp32_thermal/README.md). */

#define WIFI_SSID            "YOUR_WIFI_SSID"
#define WIFI_PASSWORD        "YOUR_WIFI_PASSWORD"

// follower-server host (Henri) and its UDP_LISTEN_PORT in app/server.py
#define THERMAL_SERVER_IP    "192.168.1.169"
#define THERMAL_SERVER_PORT  14600
