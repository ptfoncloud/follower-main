#pragma once
/*  template for src/secrets.h (gitignored, only this example is committed)
      Copy-Item src\secrets.example.h src\secrets.h
    then fill it in. Used by the thermal streaming (esp32_thermal/README.md). */

#define WIFI_SSID            "YOUR_WIFI_SSID"
#define WIFI_PASSWORD        "YOUR_WIFI_PASSWORD"

// follower-server host (Henri) and its UDP_LISTEN_PORT in app/server.py
#define THERMAL_SERVER_IP    "192.168.1.169"
#define THERMAL_SERVER_PORT  14600
