# UPDATE — .zwo-Import

Stand: **2026-09-12**, Firmware **0.3.4-dev**.

## Was

- `src/control/ZwoImport.{h,cpp}` — Arduino-frei, hostgetestet
- `POST /api/workout/import` — Body = `.zwo`-XML → Workout-JSON
- UI: Drop/Datei akzeptiert `.zwo` / `.xml` (neben JSON)
- Mapping: SteadyState, Warmup/Cooldown/Ramp (als 2 Steadies), IntervalsT,
  FreeRide (`self_paced`); Power als %FTP

## Nutzung

1. Workouts → Editor → JSON/.zwo ablegen
2. Prüfen → Auf Gerät speichern
3. Start wie jedes andere Programm

## Grenzen

- Max. 8 Editor-Zeilen / 16 expandierte Schritte (Intervalle zählen ×2)
- Cadence-/Textevents werden ignoriert
- Warmup/Cooldown nicht als feine `type:ramp`-Scheiben (Budget)
- Kein `.erg`-Import

## Tests

`pio test -e native -f test_zwo`
