# UPDATE — Workout-Tags + Auto-Pause pro Programm

Stand: **2026-09-13**, Firmware **0.3.7-dev**.

## Was

### Tags
- `WorkoutDoc.tags` max. 4 × 16 Zeichen, JSON `"tags":["easy","zone2"]`
- Builtins: `/workouts/meta.json` → `{ "fav":[…], "tags":{ "physio":["reha"] } }`
- FS-Dateien: Tags im Workout-JSON
- API: `POST /api/workout/tags?id=&tags=a,b` (List liefert `tags[]`)
- UI: Editor-Feld, Chips auf Karten, Filter trifft auch Tag-Namen

### Auto-Pause pro Workout
- JSON `"autoPauseS":15` (0 = Default 10 s, Clamp 2…600)
- Beim Workout-Start → `SessionTracker::setAutoPauseAfterMs`
- Session-Ende / Nicht-Workout → Default 10 s

### Stop-Hinweis
Bereits seit Ride-UI (`#startHint` / `afterStop`) — kein neuer Code.

## Tests
`pio test -e native -f test_wojson -f test_session`
