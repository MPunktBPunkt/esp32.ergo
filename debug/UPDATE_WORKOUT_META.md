# UPDATE — Workout-Meta (Favoriten / Ziel / RPE)

Stand: **2026-09-12**, Firmware **0.3.3-dev**.

## Was

- WorkoutDoc: `goal`, `favorite` (Parse/Write); Builtins mit Ziel-Tag
- Favoriten in LittleFS `/workouts/meta.json` (max. 12 IDs)
- `/api/workout/list` liefert `goal`, `favorite`, `recent[]`
- `POST /api/workout/favorite?id=&on=`
- Session: `rpe` (1–10) + `note`; `POST /api/session/annotate`
- UI: Filter Alle / ★ / Zuletzt / Ziele; Stern auf Karten; Editor-Ziel;
  RPE/Notiz bei letzter Session (Workouts + Verlauf)

## Nutzung

1. Workouts → Filter oder ★ tippen
2. Editor → Ziel wählen → Speichern
3. Nach Fahrt: RPE + Notiz → Speichern (schreibt `last.json` + Ring-Kopf)

## Bewusst nicht

.erg-Import, freie Tags, Annotate schreibt nicht die gesamte jsonl-Historie.
