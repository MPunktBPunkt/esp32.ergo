# esp32.ergo

![Version](https://img.shields.io/badge/version-0.3.23--dev-green)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Build](https://github.com/MPunktBPunkt/esp32.ergo/actions/workflows/build.yml/badge.svg)](https://github.com/MPunktBPunkt/esp32.ergo/actions/workflows/build.yml)
[![Donate](https://img.shields.io/badge/Donate-PayPal-00457C.svg?logo=paypal)](https://www.paypal.com/donate/?business=martin%40bchmnn.de&currency_code=EUR)

> **Trainingsrechner und BLE-Steuerung für das Ergometer Hammer Varon XTR II** — ERG-Emulation über die Widerstandsstufe, Pulsführung, Trainingszonen, Profile und Web-UI. Anbindung an [iobroker.esp-hub](https://github.com/MPunktBPunkt/iobroker.esp-hub).

> [!NOTE]
> Stand und offene Punkte: [`STATE.md`](STATE.md). Versionsgeschichte:
> [`CHANGELOG.md`](CHANGELOG.md). Caps-Fix und „Erfolgsquittung beweist nichts“:
> [`debug/UPDATE_CAPS_FIX.md`](debug/UPDATE_CAPS_FIX.md). Coach-Release v0.1.0:
> [`debug/RELEASE_v0.1.0.md`](debug/RELEASE_v0.1.0.md).

> [!TIP]
> Technische Gesamtschau: [`docs/ergometer/ENTWICKLERDOKU.md`](docs/ergometer/ENTWICKLERDOKU.md).
> Bridge: [`docs/ergometer/BRIDGE.md`](docs/ergometer/BRIDGE.md). Kennfläche:
> [`docs/ergometer/KALIBRIERUNG.md`](docs/ergometer/KALIBRIERUNG.md).

---

## Überblick

`esp32.ergo` ist ein **eigenständiger Trainings-Node** zwischen Ergometer und Fahrer. Der ESP verbindet sich als BLE Central mit dem Bike, holt sich den Puls aus einer von drei Quellen und stellt den Widerstand — die Web-UI ist während der Fahrt die **einzige** Anzeige, weil das Konsolendisplay bei bestehendem BLE-Link abschaltet.

| Das ist es | Das ist es nicht |
|------------|------------------|
| Trainingsrechner mit ERG-Emulation | Ersatz für MyWhoosh oder Zwift |
| Zonen, Profile, Workouts, Tests | Medizinisches Messgerät |
| Pulsgeführtes Training inkl. Reha-Deckel | Trainingsplan-Generator |
| Hub-Telemetrie + Session-Archiv | Cloud-Sync ohne Hub |

```
Varon XTR II ──BLE Central──┐
                            ├──▶ ESP32-S3 ──WiFi──▶ WebUI/SSE + ESP-Hub
HR-Relay / Polar H9 ────────┘         │
                                      └──BLE Peripheral──▶ MyWhoosh (Bridge, v0.3)
```

Der Puls kommt wahlweise direkt vom Gurt, vom [HR-Relay](https://github.com/MPunktBPunkt/esp32.heartrate) oder aus dem `0x2AD2`-Feld des Bikes, das der Polar H9 über 5 kHz GymLink speist. Die Relay-Variante ist die interessante: der H9 lässt nur **einen** BLE-Client zu, das Relay hält ihn und bedient MyWhoosh und den Ergo-Node gleichzeitig.

---

## Was das Gerät kann — und was nicht

Vier Befunde aus dem Sondenlauf mit [esp32.ftmsprobe](https://github.com/MPunktBPunkt/esp32.ftmsprobe) tragen die gesamte Architektur:

- **Standard-FTMS mit offenem Control Point.** Kein Hersteller-Protokoll, kein Reverse Engineering nötig.
- **Kein Set Target Power.** `0x2ACC` Target-Bit 3 ist gelöscht, `0x2AD8` fehlt ganz. Jede Wattsteuerung ist deshalb **Emulation über die Widerstandsstufe**.
- **16 Stufen**, 1,0 bis 16,0 in Zehnteln, als `04 <sint16 LE>`. Die 1-Byte-Form wird quittiert, wirkt aber nicht.
- **Eine Erfolgsquittung beweist nichts.** Das Bike antwortet auch auf `05 64 00` mit `80 05 01` Success, obwohl es das Feature nicht meldet.

Stufen-Sweep und Kadenzabhängigkeit fährt die geführte Kalibrierung selbst
(inkl. Verwerfung weggelaufener Kadenz). Auf Hardware: linear bis ~170 W @ Stufe 16;
Kadenztest leicht bestätigt.

**Nichts davon steht als Konstante im Code.** `ftms::Capabilities` leitet zur Verbindungszeit aus `0x2ACC`, `0x2AD6`, `0x2AD8` und den beobachteten `0x2AD2`-Flags ab, was das angeschlossene Gerät kann, und wählt daraus die Steuerstrategie: Wattziel direkt, Emulation über die Stufe, oder nur Dashboard. Ein anderes Ergometer ist damit ein Scan, ein Connect und ein Kalibrierlauf — kein Firmwarethema. Dasselbe gilt für den Pulsgurt: die BLE-Quellen sind reines `0x180D` und herstellerunabhängig.

---

## Features

### v0.1 — Coach *(released)*

- **BLE Central (NimBLE):** Scan, Connect, Remember / Forget für Bike und Gurt
- **Steuermodi:** `OFF`, `MANUAL_LEVEL`, `MANUAL_ERG` (emuliert), `HR_HOLD`, `REHA`, `WORKOUT`
- **Profile** (Mehrbenutzer), harte Grenzen, Zonen mit Ambient/Hysterese
- **Reha:** festes Watt + Pulsdeckel; **Progression** nach sauberer Physio-Einheit
- **Limiter** als einziger Schreibpfad; Steuer-Journal; Kalibrierung / Kennfläche
- **Web-UI:** Ride (Tablet), Bibliothek, Schritt-Editor, Tests-Stubs, Verlauf, Debug
- Session-Archiv, Hub-Heartbeat `fwType: ergo`, OTA in beide Richtungen

### v0.2 — Trainingslehre *(teilweise schon in 0.1)*

Echter TestRunner (MAP / 20 min / Recovery), Interval-/Rampen-Editor, Ghost —
teilweise mitgeliefert; siehe CHANGELOG.

### v0.3 — Bridge *(MVP ab 0.3.0-dev)*

FTMS-Peripheral: MyWhoosh verbindet sich mit dem ESP statt mit dem Bike.
Ab 0.3.14: Observer vs. Controller und Exklusiv-Lock — siehe
[`docs/ergometer/BRIDGE.md`](docs/ergometer/BRIDGE.md).

---

## Hardware

| Board | PlatformIO-Env | Hinweis |
|-------|----------------|---------|
| ESP32-S3 | `ergo` | zwei BLE-Links im Coach, drei in der Bridge |

Kein D1 Mini: der Coach braucht zwei gleichzeitige Verbindungen, die Bridge drei. Kein PSRAM — `heartrate-s3` fährt drei Links plus WiFi und SSE ohne.

---

## Quickstart

```bash
pio run -e ergo --target upload
pio device monitor
```

1. Hotspot **`ESP-Ergo-Setup`** → WLAN + Hub-IP (Port `8093`)
2. Browser: `http://<ESP-IP>/` → Reiter **Geräte** → Suchen → Zeile mit `FTMS`-Marke antippen
3. Gerät erscheint im [ESP-Hub](https://github.com/MPunktBPunkt/iobroker.esp-hub) mit `fwType: ergo`

Die Rolle wird aus dem Advertising abgeleitet: was `0x1826` bewirbt, wird als Bike verbunden, was nur `0x180D` hat, als Pulsgurt. Beides wird gemerkt und nach einem Verbindungsverlust mit ansteigendem Abstand (2, 5, 10, 20, 30 s) neu versucht.

Ohne Kabel ausrollen:

```bash
tools/deploy.sh --ota 192.168.178.88
tools/deploy.sh --hub 192.168.178.113:8093 --mac 68B6B329339C
```

Das Skript baut, benennt die Bin nach dem Familienschema und **weigert sich, eine Bin auszurollen, in der `/ota-upload` nicht vorkommt.** Genau dieser Fall lag bis 0.1.0-dev vor: die Firmware baute, hatte aber keinen Rückweg — ein OTA-Flash auf ein Gerät ohne Kabel wäre ein Totalverlust gewesen.

| | |
|--|--|
| mDNS | `ergo-XXXXXX.local` |
| OTA | `POST /ota-upload` (multipart `firmware`) |
| Bin-Schema | `ergo.<semver>.esp32s3.bin` |
| WLAN zurücksetzen | BOOT / GPIO0 ca. 3 s halten |

---

## Libraries

| Library | Autor | Version |
|---------|-------|---------|
| WiFiManager | tzapu | ≥ 2.0.17 |
| ArduinoJson | bblanchon | ≥ 7.2 |
| NimBLE-Arduino | h2zero | ≥ 1.4.3 |

Platform: `espressif32@6.4.0`, Framework Arduino. NimBLE ist auf 1.4.x gepinnt — gleiche API wie `esp32.heartrate` und `esp32.ftmsprobe`, damit Code zwischen den dreien wandern kann.

---

## Hub-IO-Werte

Heartbeat-Feld `fwType`: **`ergo`**

| Key / Feld | Bedeutung |
|------------|-----------|
| `ergo_state` / `control_mode` | BLE-Zustand, aktiver Steuermodus |
| `profile` | aktives Nutzerprofil |
| `power` / `power_target` | Ist- und Zielleistung in W |
| `level` / `level_target` | Widerstandsstufe — **Schattenwert**, siehe unten |
| `cadence` / `speed` / `distance` | rpm, km/h, m |
| `heart_rate` / `hr_source` / `hr_zone` | BPM, Quelle, Zone 1–5 |
| `work_kj` / `calories` | kJ, kcal |
| `np` / `if` / `tss` | Normalized Power, Intensity Factor, Training Stress Score |
| `workout_name` / `workout_step` / `workout_remaining` | laufendes Programm |
| `target_reachable` | 0, wenn das Wattziel über der Stufendecke liegt |

`level` ist der Schattenwert des ESP, keine Rückmeldung des Bikes: `0x2AD2` liefert bei diesem Gerät kein Resistance-Level-Feld.

---

## API (Auswahl)

Die Oberfläche hat die Reiter aus [WEBINTERFACE.md](docs/ergometer/WEBINTERFACE.md)
§7 (Betrieb + optional Entwickler-UI). Ohne JavaScript: Abschnitte untereinander
und natives OTA-Formular — Rückweg nach Fehlflash.

Vollständige Routenliste aus `src/app/App.cpp` (66 `server.on`-Pfade plus
`/ota-upload` über die vierargumentige Form). Thematisch:

| Gruppe | Endpunkte |
|--------|-----------|
| Shell | `GET /` · `GET /ota` · `POST /ota-upload` · `GET /api/status` · `GET/POST /api/config/get` `/save` · `POST /api/system/restart` · `GET /events` |
| BLE | `/api/ble/scan/start` `/stop` · `/api/ble/devices` · `/api/ble/connect` `/disconnect` `/forget` `/reconnect` |
| Steuerung | `/api/control/mode` `/level` `/power` `/hr` `/reha` `/sim` `/request` `/reset` `/start` `/stop` |
| Kalibrierung | `/api/calib/sweep/start` `/stop` · `/api/calib/map` · `/api/calib/clear` |
| Profile | `/api/profile/list` `/get` `/put` `/select` `/delete` |
| Workouts | `/api/workout/list` `/put` `/download` `/validate` `/start` `/pause` `/resume` `/skip` `/stop` `/favorite` `/import` `/tags` |
| Tests | `/api/test/result` `/accept-ftp` |
| Session | `/api/session/list` `/last` `/annotate` |
| Bridge | `GET/POST /api/bridge` |
| Geräte | `/api/devices` · `/api/device` |
| FTP-Karriere | `/api/ftp-career` `/accept` `/decline` `/set` |
| Progression | `/api/progression/get` `/accept` `/decline` |
| Probe/Debug | `/api/probe/arm` `/clear` `/mark` · `/api/debug/ring` `/clear` `/export` |

Nachtests nutzen die **Probe-API** (`/api/probe/*`) und den Debug-Export — siehe
[NACHTESTS.md](docs/ergometer/NACHTESTS.md) und ENTWICKLERDOKU.

---

## Build und Tests

```bash
pio run -e ergo              # Firmware für den S3 (braucht Python für UI-Pack)
pio test -e native           # Hosttests der Arduino-freien Bausteine
python tools/docs_html.py --check   # Doku-Links
```

**23** Hostsuiten, **240** `RUN_TEST`-Fälle (`grep -rc '^\s*RUN_TEST(' test/`).
Die Positivliste steht in `platformio.ini` `[env:native]`.

> Keine `[env]`-Sektion in `platformio.ini` (sonst erbt `env:native` Arduino).
> Fixture-Sollwerte: `tools/ref/ftms.py` (unabhängige Referenz).

```bash
python tools/make-fixtures.py --verify-curated
# Optional Laborlauf (Monorepo-Nachbar):
python tools/make-fixtures.py --scan ../nodes/esp32.ftmsprobe/docs/ergometer/scan-20260910
```

Drei Paketzahlen (nicht vermischen): Labor-Zusammenfassung **274**, eindeutige
`0x2AD2`-Hexes **383**, eingecheckter Kernsatz **5** (`fixtures_ibd.h`, CI
`--verify-curated`). `fixtures_synth.h` deckt Feldkombinationen ab, die der
Varon nie sendet (nur `flags = 0x0B54` / 19 Byte im Laborlauf).

---

## Docs

| Dokument | Inhalt |
|----------|--------|
| [STATE.md](STATE.md) | lebender Stand, Offen, harte Regeln |
| [CHANGELOG.md](CHANGELOG.md) | Versionsgeschichte |
| [ENTWICKLERDOKU.md](docs/ergometer/ENTWICKLERDOKU.md) | Technik, Persistenz, Sicherheit, Regelung |
| [PFLICHTENHEFT.md](docs/ergometer/PFLICHTENHEFT.md) | Konzept |
| [GERAETEPROFIL.md](docs/ergometer/GERAETEPROFIL.md) | gemessenes Gerät |
| [KALIBRIERUNG.md](docs/ergometer/KALIBRIERUNG.md) | Kennfläche |
| [BRIDGE.md](docs/ergometer/BRIDGE.md) | Bridge-Betrieb |
| [NACHTESTS.md](docs/ergometer/NACHTESTS.md) | Nachtests (Test 6 offen) |
| [WEBINTERFACE.md](docs/ergometer/WEBINTERFACE.md) | UI-Design |
| [BEDIENUNG.md](docs/ergometer/BEDIENUNG.md) | kurze Bedienung |
| [BLE-SCAN.md](docs/ergometer/BLE-SCAN.md) | Scan-Vorlage |

HTML-Handbuch erzeugen: `python tools/docs_html.py` → `docs/HANDBUCH.html`
(nicht eingecheckt).

---

## Verwandte Projekte

| Projekt | Rolle |
|---------|-------|
| [esp32.ftmsprobe](https://github.com/MPunktBPunkt/esp32.ftmsprobe) | Laborsonde; `BleProbe` wird hier zu `BleCentral` + `FtmsClient`, `tools/ftms.py` ist die Referenz für `FtmsCodec` |
| [esp32.heartrate](https://github.com/MPunktBPunkt/esp32.heartrate) | HR-Relay als Pulsquelle; `HrServer` ist die Vorlage für den `FtmsServer` der Bridge |
| [iobroker.esp-hub](https://github.com/MPunktBPunkt/iobroker.esp-hub) | Registrierung, IO-Werte, OTA-Verteilung |

---

## Lizenz & Support

GNU General Public License v3.0 © MPunktBPunkt — siehe [LICENSE](LICENSE).

Wenn dir das Projekt hilft, freue ich mich über einen Kaffee:

[![Donate](https://img.shields.io/badge/Donate-PayPal-00457C.svg?logo=paypal)](https://www.paypal.com/donate/?business=martin%40bchmnn.de&currency_code=EUR)
