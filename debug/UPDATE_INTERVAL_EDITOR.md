# UPDATE — Intervall-Editor

Stand: 2026-09-12

## Was

- JSON: `type:"interval"` + `repeat` + nested Work/Rest → expandiert auf flache
  Steady-Schritte (≤16)
- Editor: **+ Intervall** (Repeat, Work/Rest, Watt oder % FTP)
- `PUT /api/workout/put` speichert Original-JSON (kompakte Intervalle bleiben)
- Hosttests: Expand 4× + Overflow

## Grenzen

- Keine verschachtelten Intervalle, max. 2 Kinder (Work/Rest in der UI)
- Keine Rampen-Autorenschaft (MAP-Rampe weiter als Builtin)
- Engine unverändert (nur flache Schritte)
