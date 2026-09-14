# Notiz für die Entwurfs-Instanz — Nachfass 2026-09-14

Bezug: `debug/TODO_NACHFASS_20260914.md`, Firmware **0.3.24-dev**.

## Ergebnisse (prüfbar)

| Check | Ergebnis |
|-------|----------|
| `pio test -e native` | **245** Cases, alle grün (24 Suiten inkl. `test_hr_source`) |
| `pio run -e ergo` | SUCCESS |
| Flash | **75,0 %** (1 473 936 / 1 966 080) — vs. 74,9 % bei 0.3.23 |
| `tools/docs_html.py --check` | OK (19 Handbuch-Dokumente) |

## Was gebaut wurde (Teil 2)

- `hrUsableForControl` in `src/ble/HrSource.h` (hosttestbar)
- Eintritt: `/api/control/mode` (hr/reha), `/api/control/hr`, `/api/control/reha` → 409
  `{ reason: hr_source_not_trusted, hrSource }`
- Laufend: `loopHr` / `applyRehaCap` behandeln nicht-vertrauenswürdige Quelle als Pulsverlust;
  Workout: Deckel unbewaffnet (`workout.hrCapArmed=false`), Watt läuft weiter
- Status: `hrUsable`, `reha.capArmed`, `workout.hrCapArmed`
- UI: Buttons Puls/Reha disabled + Klartextgrund
- `RehaController`: kein Cap auf unvertrauenswürdigem BPM; bei `lost` fällt `capActive`

## Gegen 2.1?

Nichts aufgefallen, das gegen die Sperre spricht. `resolveHrSource()` meldet
Relay heute noch als `Strap` (beide über `HrClient`) — `hrUsableForControl`
akzeptiert beides; kein Abschneiden der Relay-Topologie.

## Doku

Teil 1+3 erledigt (PH UI/API, Changelog 0.3.9/10/20, captures-Hinweis,
Paket-Zahlen, §11/14/15, STATE-Kriterien). SIM-Passthrough steht unter Offen.

Bin: `dist/ergo.0.3.24-dev.esp32s3.bin`
