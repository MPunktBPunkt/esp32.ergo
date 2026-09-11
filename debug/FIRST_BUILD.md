# Erster Build — 2026-09-11

Ausgeführt auf Host unter `/home/martin/projects/esphub/esp32.ergo`.
PlatformIO: `/home/martin/.venvs/pio/bin/pio` (Core 6.2.0).

## Ausgangslage

- Repo-Inhalt: FTMS-Codec (`FtmsCodec`, `FtmsCapabilities`), Host-Fixtures, Platzhalter-`main.cpp`.
- Keine App-Schicht (kein BleCentral, Web-UI, Hub, Limiter).
- README behauptet CI-Badge → Workflow **existiert nicht** (`.github/` fehlt, `actions/workflows` = 0).
- Docs unter `private/docs/ergometer/` sind im Repo noch nicht vorhanden.

## Firmware `ergo` (ESP32-S3)

```text
pio run -e ergo
→ SUCCESS (~29 s erster Lauf, ~20 s Rebuild)

Board:   esp32-s3-devkitc-1
Platform: espressif32@6.4.0 / Arduino 2.0.11
RAM:     18472 / 327680  (5.6 %)
Flash:   258421 / 1966080 (13.1 %)
Bin:     .pio/build/ergo/firmware.bin  (258784 bytes)
```

Libs installiert (Firmware-Env):

- WiFiManager 2.0.17
- ArduinoJson 7.4.3
- NimBLE-Arduino 1.4.3

`main.cpp` bleibt Platzhalter: Serial-Banner + Decode eines aufgezeichneten Varon-Pakets.

## Host-Tests `native`

### Vorher (Original-`platformio.ini`)

```text
pio test -e native
→ ERRORED
Please specify `board` in `platformio.ini` to use with 'arduino' framework
```

Ursache: siehe [PLATFORMIO_FIX.md](PLATFORMIO_FIX.md).

### Nachher (lokaler Fix)

```text
pio test -e native
→ PASSED  22/22 in ~0.77 s
```

Bestehende Cases (Auswahl):

- Gerätepakete + synthetische Fixtures
- Speed-Flag-Inversion, uint24 Distance, Skalierung
- Feature/Stufenbereich Varon, Encoder vs. Aufzeichnung
- Capabilities: Varon / Smarttrainer / behauptetes Wattziel / große Skala / ohne Stellweg / aus Datenstrom

## Offene lokale Änderungen (Stand Handoff)

```text
 M platformio.ini          ← Fix (siehe PLATFORMIO_FIX.md)
?? debug/                  ← dieser Ordner
?? .pio/                   ← Build-Cache, nicht versionieren
```

Kein Commit/Push von dieser Instanz, außer der User fordert es.
