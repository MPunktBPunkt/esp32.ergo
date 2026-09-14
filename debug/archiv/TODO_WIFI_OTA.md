# ANWEISUNG: Connectivity-Shell (WiFi + OTA + Hub)

**Priorität: sofort / blockierend für HW-Test auf `.88`**

Zielgerät: ESP32-S3 `192.168.178.88` (läuft aktuell **FtmsProbe-S3 v0.1.4** mit funktionierendem `/ota-upload`).
Build-Host: `/home/martin/projects/esphub/esp32.ergo` — **kein USB**, nur Netz.

## Warum das jetzt

Codec + Limiter sind hostgetestet und in der Firmware gelinkt, aber `main.cpp` ist ein
Platzhalter **ohne WiFi/Web/OTA**. Ein OTA-Flash der aktuellen Bin auf `.88` wäre ein
**Remote-Brick** (kein HTTP mehr, kein Serial am Build-Host).

Deshalb: erst eine minimale Connectivity-Shell wie in den Schwesterprojekten, **dann**
erst OTA auf `.88`. BLE/Coach/Web-Charts kommen danach.

---

## Auftrag (Definition of Done)

1. Boot → WiFiManager-Portal falls keine Creds → STA + NTP
2. mDNS `ergo-XXXXXX.local`
3. `WebServer` Port 80 mit mindestens:
   - `GET /` — kurze Statusseite (Name, Version, IP, Board)
   - `GET /api/status` — JSON (siehe unten)
   - `GET /ota` — Upload-UI
   - `POST /ota-upload` — multipart Feld **`firmware`**, danach Restart
4. Hub-Heartbeat `POST http://<hub>:8093/api/register` mit `fwType: "ergo"`
5. Reset: BOOT/GPIO0 ca. 3 s halten → WiFiManager `resetSettings` + App-NVS clear → Restart
6. Codec-Selbsttest bleibt (Serial und/oder Feld in `/api/status`)
7. `pio run -e ergo` grün; Native-Tests (`codec` + `limiter`) bleiben 43/43
8. Bin-Name: `ergo.<semver>.esp32s3.bin` (z. B. via `extra_scripts` oder `tools/deploy.sh`)

**Nicht** in diesem Schritt: BleCentral, FtmsClient, Limiter-Verdrahtung, Workout-UI, LittleFS-Archive.

---

## Vorlage kopieren

**Beste Vorlage für die Shell:** `esp32.heartrate` (S3-Env, gleiche Libs, schlankes `NetUtil`).
**OTA/Deploy-Skript:** `esp32.ftmsprobe/tools/deploy.sh` (curl + Sicherheitschecks).

| Baustein | Quelle (absolut) |
|----------|------------------|
| App-Orchestrierung | `/home/martin/projects/esphub/nodes/esp32.heartrate/src/app/App.cpp` (+ `.h`) |
| HubClient | `…/esp32.heartrate/src/core/HubClient.cpp` (+ `.h`) |
| ConfigStore | `…/esp32.heartrate/src/core/ConfigStore.cpp` (+ `.h`) |
| NetUtil | `…/esp32.heartrate/src/core/NetUtil.cpp` (+ `.h`) |
| OTA-UI | `…/esp32.heartrate/src/web/UiPages.h` (auf Status+OTA reduzieren) |
| OTA-Handler | `App::handleOtaUpload` / `handleOtaUploadFinish` in heartrate **oder** ftmsprobe |
| Deploy | `/home/martin/projects/esphub/nodes/esp32.ftmsprobe/tools/deploy.sh` |

Zielstruktur in ergo:

```
src/
  main.cpp                 → App::instance().begin() / .loop()
  app/App.cpp, App.h
  core/ConfigStore.*
  core/HubClient.*
  core/NetUtil.*
  web/UiPages.h            → minimal
  ble/…                    → unverändert
  control/Limiter.*        → unverändert, noch nicht verdrahten
```

---

## Ergo-Konstanten (schon in `include/BuildFlags.h`)

| Makro | Wert |
|-------|------|
| `FW_TYPE` | `"ergo"` |
| `WIFI_AP_NAME` | `"ESP-Ergo-Setup"` |
| `WIFI_PORTAL_TIMEOUT_S` | `180` |
| `HUB_HOST_DEFAULT` | `192.168.178.113` |
| `HUB_PORT_DEFAULT` | `8093` |
| `DEVICE_NAME_DEFAULT` | `"Ergo"` |
| `RESET_BUTTON_PIN` / `RESET_HOLD_SEC` | `0` / `3` |
| `ERGO_HW_TYPE` / `ERGO_BOARD_ID` | `esp32s3` / `esp32-s3` |

Anpassen gegenüber heartrate:

- AP → `ESP-Ergo-Setup`
- mDNS-Präfix → `ergo-` + letzte 6 Hex der MAC
- NVS-App-Namespace → **`ergo`** (shared bleibt **`esphub`**: `name`, `hub_host`, `hub_port`)
- Heartbeat `fwType` → `"ergo"` (idealerweise aus `FW_TYPE`)
- Kein zweites Board-Env — nur `env:ergo`

Portal-Parameter wie überall: `name`, `hub_host`, `hub_port`.
Nach Connect: `WiFi.setSleep(true)` (BLE-Koexistenz später).

---

## OTA-Vertrag (identisch zu Probe/HR)

| | |
|--|--|
| Methode/Pfad | `POST /ota-upload` |
| Body | `multipart/form-data`, Feldname **`firmware`** |
| Flash | `Update.begin(UPDATE_SIZE_UNKNOWN)` → `write` → `end(true)` |
| OK | `200 text/plain` → `OK - Neustart...` |
| Fehler | `500 text/plain` → `OTA fehlgeschlagen!` |
| Reboot | `delay(400); ESP.restart();` (auch bei Fehler — Familienkonvention) |

```bash
curl -F "firmware=@dist/ergo.0.1.0-dev.esp32s3.bin" http://192.168.178.88/ota-upload
```

Hub-Pfad (nach Heartbeat):

```bash
curl -F "firmware=@dist/ergo.0.1.0-dev.esp32s3.bin" \
  http://192.168.178.113:8093/api/firmware-upload
curl -X POST -H 'Content-Type: application/json' \
  -d '{"mac":"68B6B329339C","firmware":"ergo.0.1.0-dev.esp32s3.bin"}' \
  http://192.168.178.113:8093/api/ota-push
```

MAC von `.88` (aktuell Probe): `68B6B329339C`. Hub verlangt Chip-kompatiblen Suffix `.esp32s3.bin`.

---

## Minimaler Heartbeat

`POST /api/register`, Intervall default ~30 s:

```json
{
  "mac": "68B6B329339C",
  "name": "Ergo",
  "hwType": "esp32s3",
  "chipModel": "ESP32-S3",
  "version": "0.1.0-dev",
  "ip": "192.168.178.88",
  "rssi": -50,
  "uptime": 120,
  "freeHeap": 200000,
  "freeSketch": 1310720,
  "fwType": "ergo",
  "board": "esp32-s3",
  "ios": {}
}
```

Response: `interval` beachten; bei `otaUrl` → HubClient zieht Bin (wie heartrate).
Watchdog-Default: wie heartrate **300 s** vorschlagen (später bei aktivem Bike-Link ggf. aus).

### `/api/status` (Minimum)

```json
{
  "name": "Ergo",
  "version": "0.1.0-dev",
  "fwType": "ergo",
  "board": "esp32-s3",
  "boardLabel": "ESP32-S3",
  "ip": "…",
  "mac": "…",
  "rssi": -50,
  "uptimeS": 120,
  "heap": 200000,
  "hub": "192.168.178.113:8093",
  "hubOk": true,
  "codecSelfTest": "ok"
}
```

---

## Flash-Plan für `.88` (nach dem Merge)

1. Lokal: `pio run -e ergo` → Bin nach `dist/ergo.<ver>.esp32s3.bin` kopieren/benennen
2. **Vor dem Flash prüfen:** neue Firmware enthält `/ota-upload` (grep/`strings` oder Code-Review) — sonst **nicht** flashen
3. Probe-Bin als Rollback bereithalten:
   `/home/martin/projects/esphub/nodes/esp32.ftmsprobe/dist/` (oder dort neu bauen)
4. OTA auf `http://192.168.178.88/ota-upload`
5. Nach Reboot: `curl http://192.168.178.88/api/status` → `fwType=ergo`
6. Hub: Gerät erscheint mit `fwType: ergo`
7. Zweiter OTA-Roundtrip (Ergo → Ergo) beweist Selbst-Recovery

Falls Portal nötig: Hotspot `ESP-Ergo-Setup`, Creds + Hub-IP setzen.
WLAN-Reset: BOOT 3 s.

**Build-Host-Hinweis:** `/home/martin/.venvs/pio/bin/pio` — siehe `debug/commands.sh`.

---

## Was unverändert bleiben muss

- `[common]` / kein `[env]` in `platformio.ini` (sonst sterben Native-Tests)
- `build_src_filter` für native: Codec + Capabilities + Limiter
- Limiter und Codec Arduino-frei; keine `millis()` im Limiter
- Kein Wattziel-Steuerweg über `0x05` am Varon (nur Resistance/Emulation)

---

## Nach diesem Milestone

Dann erst Hardware-Smoke auf `.88` (Status, OTA-Loop, Hub).
Danach: `BleCentral` + `FtmsClient`, Limiter als einzigen Schreibpfad davor — siehe `RECOMMENDATIONS.md`.

Wenn die Shell steht: kurz in `debug/` ein `UPDATE_CONNECTIVITY.md` mit Build-Ergebnis und erstem OTA-Versuch ergänzen (oder die Build-Instanz bitten).
