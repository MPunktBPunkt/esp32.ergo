# Nacht-Update 2026-09-12 — ohne Fahrer-Bestätigung

## Geliefert

- LittleFS `/workouts/` + Session-Stub `/sessions/last.json`
- `WorkoutJson` Parse/Serialize + 4 Builtins (physio, reha_kurz, easy20, ftp_warm)
- API: list / validate / put / download / start?id= / session/last
- Workouts-Reiter in der UI (Liste, Start, Upload, letzte Session)
- Hub-Heartbeat: session_*, power_target, level_target, workout_*
- Geräte-Rename `FtmsProbe*` → `Ergo` einmalig
- `tools/hw_smoke.sh` — HTTP-Smoke inkl. Nachtest-3-Ablehnung

## Bewusst nicht

Lastgefühl, Nachtest 4 Wirkung, Editor, Progression.
