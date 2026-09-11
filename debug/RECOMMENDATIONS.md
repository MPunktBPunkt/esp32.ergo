# Empfehlungen (Handoff)

Priorisiert für die Instanz, die die App weiterbaut.

## 0. Jetzt zuerst (blockiert HW-Test)

→ **[TODO_WIFI_OTA.md](TODO_WIFI_OTA.md)** — Connectivity-Shell wie heartrate/ftmsprobe.
Ohne das kein OTA auf `.88` (aktuell Probe). Limiter ist fertig, aber unverdrahtet.

## 1. Hygiene (wenn Connectivity steht)

1. GitHub Actions laut README-Badge (`build.yml`): `pio run -e ergo` + `pio test -e native`
2. `tools/deploy.sh` analog ftmsprobe (Bin-Namen + curl OTA)

## 2. Danach (Fundamentschicht Coach)

| Priorität | Baustein | Anmerkung |
|-----------|----------|-----------|
| hoch | `BleCentral` + `FtmsClient` | Codec fertig; Notify `0x2AD2` + Control Point |
| hoch | Limiter verdrahten | Einziger Schreibpfad vor jedem FTMS-Write |
| hoch | Capabilities zur Laufzeit | aus `0x2ACC`/`0x2AD6`/`0x2AD8` |
| später | Web-UI / SSE Live | erst wenn Live-Werte fließen |
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

## 5. Upload auf `.88`

Kein USB am Build-Host — nur OTA, und **nur** nach Connectivity-Shell
(siehe TODO_WIFI_OTA.md). Probe-Bin als Rollback bereithalten.
