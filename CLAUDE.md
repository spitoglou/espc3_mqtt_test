# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

ESP32-C3 Super Mini firmware that publishes JSON telemetry every 5 s to a RabbitMQ broker via the MQTT plugin (the broker speaks MQTT on port 1883 / TLS on 8883). Built with PlatformIO + Arduino framework. Target board: `esp32-c3-devkitm-1`.

## First-time setup

`src/secrets.h` is gitignored and required to build. Copy the template:

```powershell
Copy-Item src/secrets.h.example src/secrets.h
```

Then fill in WiFi credentials and broker host/user/pass. Without this file the build fails immediately.

## Common commands

```powershell
pio run                      # build
pio run -t upload            # build + flash over USB
pio device monitor           # serial monitor at 115200 with esp32_exception_decoder
pio run -t clean             # clean build artifacts
pio pkg update               # refresh library deps
```

Upload + monitor in one shot: `pio run -t upload -t monitor`.

## Architecture

The firmware is a single translation unit ([src/main.cpp](src/main.cpp)) with all configuration as `constexpr` at the top. Two state-keeping blocking helpers handle connectivity:

- **`connectWiFi()`** — blocks indefinitely until associated. Called from `setup()` and re-called from `loop()` on drop.
- **`connectMQTT()`** — blocks with 3 s retry. Same pattern.

Because both helpers are blocking, anything in `loop()` (including `mqtt.loop()`) does not run during reconnects. If you add features that need timely servicing (e.g. subscriptions, sensor sampling), this constraint matters — convert to a non-blocking state machine or accept the freeze window.

The MQTT client ID is built once in `setup()` as `esp32c3-<MAC-without-colons>`, so multiple boards flashed with the same firmware coexist on the broker.

Telemetry payload is built with ArduinoJson v7 (`JsonDocument`) into a stack buffer, then published null-terminated. PubSubClient's internal buffer is bumped to 512 bytes in `setup()`; if payload fields grow, raise `MQTT_BUFFER_SIZE`.

## Hidden constraints

- **Serial-over-USB requires `ARDUINO_USB_MODE=1` and `ARDUINO_USB_CDC_ON_BOOT=1`** ([platformio.ini](platformio.ini)). The C3 Super Mini has no UART-USB chip; Serial goes through the C3's native USB-CDC. Removing these flags silently breaks `Serial.print*`.
- **LED on GPIO 8 is active-LOW.** `digitalWrite(LED_PIN, LOW)` = on. The `ledOn()` / `ledOff()` helpers in [src/main.cpp](src/main.cpp) encapsulate this — prefer them over raw `digitalWrite`.
- **`Serial.begin()` returns before the host has opened the port.** `setup()` waits up to `SERIAL_WAIT_MS` (2 s) for `Serial` truthiness; early prints before that may still be lost on a cold plug-in.

## Library dependencies

Pinned in [platformio.ini](platformio.ini): `PubSubClient ^2.8` (MQTT), `ArduinoJson ^7.2.0`. ArduinoJson v7 uses dynamic `JsonDocument` (no capacity argument) — do not port v6 patterns.
