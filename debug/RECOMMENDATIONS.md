# Empfehlungen (Handoff)

Priorisiert für die Instanz, die die App weiterbaut.

## 1. Sofort / Hygiene

1. **`platformio.ini`-Fix committen** (siehe PLATFORMIO_FIX.md) — sonst bleiben Native-Tests tot.
2. **`.gitignore` anlegen**, mindestens:
   ```
   .pio/
   .vscode/.browse.c_cpp.db*
   .vscode/c_cpp_properties.json
   .vscode/launch.json
   .vscode/ipch
   ```
3. **GitHub Actions** laut README-Badge nachziehen (`build.yml`):
   - Job `firmware`: `pio run -e ergo`
   - Job `codec`: `pio test -e native`
   - PlatformIO-Cache sinnvoll
4. README-Badge bleibt sonst rot/404.

## 2. Inhaltlich als Nächstes (Fundamentschicht)

Laut README / Architektur-Reihenfolge sinnvoll:

| Priorität | Baustein | Anmerkung |
|-----------|----------|-----------|
| hoch | `BleCentral` + `FtmsClient` | Codec ist fertig; Verbindung + Notify `0x2AD2` + Control Point |
| hoch | Capabilities-Bindung zur Laufzeit | `FtmsCapabilities` schon da — aus `0x2ACC`/`0x2AD6`/`0x2AD8` füllen |
| mittel | Config / WiFiManager / Hub-Heartbeat | Flags in `BuildFlags.h` sind vorbereitet (`FW_TYPE=ergo`) |
| mittel | Limiter (einziger Schreibpfad) | Opcode-Whitelist, Klemmen, Rampe, Deadman — vor jeder Steuerung |
| später | Web-UI / SSE | erst wenn Live-Werte fließen |
| später | Kalibrierung Stufe×Kadenz→Watt | blockierend für echte ERG-Emulation |

Bewusst **kein** `encodeSetTargetPower` als Steuerweg, solange Nachtest 3 offen ist
(Gerät quittiert Success ohne Feature). Emulation über Resistance (`wide`/sint16).

## 3. Tests nicht verwässern

- Codec bleibt Arduino-/NimBLE-frei → weiter `pio test -e native`.
- Neue Fixtures nur über `tools/make-fixtures.py` + Sonden-`ftms.py` (Referenz).
- `fixtures_synth.h` nicht vom Generator überschreiben lassen.

## 4. Docs

README verweist auf `PFLICHTENHEFT.md`, `WEBINTERFACE.md`, `GERAETEPROFIL.md`,
`NACHTESTS.md` unter `private/docs/ergometer/` — beim ersten Release hierher spiegeln
oder Pfade korrigieren. Build-Kommentar in `platformio.ini` zeigt auf denselben Pfad.

## 5. Upload (wenn Hardware da)

```bash
pio run -e ergo --target upload
pio device monitor
```

Erwartung: Serial zeigt `esp32.ergo 0.1.0-dev auf ESP32-S3` und eine Zeile
`Codec: … W, … rpm, …` vom eingebetteten Varon-Paket.
