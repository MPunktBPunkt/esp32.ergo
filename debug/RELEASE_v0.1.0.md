# Release v0.1.0 — Stand und Testergebnisse

Datum: **2026-09-12**  
Gerät: ESP32-S3 `192.168.178.88` / MAC `68B6B329339C`  
Bike: Hammer Varon XTR II (`TC174`) · Puls: Polar H9  
Bin: `ergo.0.1.0.esp32s3.bin`

## Was v0.1.0 liefert

- BLE Central Bike + HR, FTMS über Limiter (kein `0x05`)
- Profile (Mehrbenutzer, Name-first UI), OFF / LEVEL / ERG / HR_HOLD / REHA / WORKOUT
- Kennfläche, Sweep, Steuer-Journal, Debug-Export
- Ride: Zonen, Ambient, sticky Stufe/STOP, Hysterese
- Workouts: Bibliothek, Vorschau/Machbarkeit, Schritt-Editor, Progression (Physio)
- Tests-UI (Rampe / 20 min / Recovery als Programm-Stubs)
- Session-Archiv, Verlauf, Hub-Heartbeat `fwType: ergo`

## Hardware-Fahrten 2026-09-12

### Stufenwirkung (MANUAL_LEVEL)

Mitschnitt `debug/captures/ride-20260912T082147Z.jsonl` + Journal:

- Writes `0x04` sint16, alle Success, **0 Widersprüche**
- Rampe Stufe ~6→16→4: W/rpm von ~1,0 auf ~3,1 und zurück auf ~0,84
- Journal: WORKS bei ausreichend langem Abstand; viele UNJUDGED nur wegen zu schneller Klicks

**Befund:** Steuerweg wirkt messbar (nicht nur Quittung).

### Session LEVEL (früher am Tag)

- ~12,5 min, Ø ~96 W, ~67 kJ, Puls bis ~137
- Auto-Pause 4×, Zonen überwiegend Z1/Z2

### Rampentest (Builtin `test_ramp`)

Letzte Session auf dem Gerät:

| Feld | Wert |
|------|------|
| Ende | `done` (8/8 Schritte, 480 s) |
| Ø Leistung | ~110 W (Soll ~106 W) |
| Arbeit | ~61 kJ |
| Puls | Ø 107 / max 149 |
| Deckel | 0 |
| Zonen | u. a. ~2 min Z6 am Ende |

Protokollspitze 200 W → grober FTP-Vorschlag ≈ 150 W (kein echtes 1‑Min‑MAP).  
Profil Martin stand danach bei FTP 140 (`estimate`).

**Hinweise Fahrer:** Kadenz oft ~60 rpm statt Profilziel 90 — ERG wirkte schwer. Nach Testende BLE zeitweise weg (Bike `LOST` / offline).

### Profile

- Profil „Manu“ angelegt und gewählt (API ok)
- Kopfzeile: am Handy flüssig; iPad teils alter Tab-Cache
- Formular vereinfacht: nur Name (+ Farbe), ID/Initiale automatisch

### Smoke / Host

- `pio test -e native`: 189 Fälle grün (Stand Progression)
- `tools/hw_smoke.sh` nach OTAs mehrfach grün (ohne Bike-Link)

## Grenzen bewusst in v0.1.0

- Kein voller TestRunner (MAP / Self-paced / Recovery-Note)
- Keine Intervalblöcke/Rampen im Editor
- Flash ~75 %, UI-Seite groß — LittleFS-Auslagerung empfohlen vor v0.2-Wachstum
- ERG/HR/Reha: Mechanik da; weitere Fahrer-Abnahmen offen
- Nachtest 4 (`0x11`) offen

## Nächste Schritte

1. Flash-/UI-Budget (LittleFS oder schlanke Seite)
2. TestRunner
3. Nachtest 4 / dichtere Map optional
