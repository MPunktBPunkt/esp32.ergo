# UPDATE — Session-Lifecycle (Auto-Pause, Archiv, Hub)

Stand: 2026-09-12

## Was

Session ist kein Stub mehr:

- **SessionTracker** — Start/Ende, Auto-Pause bei Kadenz ≈ 0 (~10 s), Arbeit/Ø-Leistung/HR, Freeze-Timeout → `MANUAL_LEVEL`
- **SessionStore** — Ring (12) + JSON-Zeile; Persistenz `/sessions/log.jsonl` + `last.json`
- Hub-Export `POST /api/session-export` (wie heartrate)
- APIs: `/api/session/last`, `/api/session/list`
- Status: `session.{active,paused,durationS,…}`; Hub-IOs `session_paused`, `work_kj`
- UI: Pause-/Freeze-Hinweise auf Ride; Reiter **Verlauf** mit Liste

## Abnahmen (Kern)

| # | Kriterium | Stand |
|---|-----------|--------|
| 10 | Pulsverlust Freeze → LEVEL nach Timeout | Firmware |
| 12 | 10 s Trittfrequenz 0 → Auto-Pause | Firmware |
| 16 | Summary auf LittleFS + Hub | Firmware |

Fahrer-Nachweis am Rad offen.

## Build

Native: `pio test -e native` (inkl. `test_session`). OTA: Bike disconnect, dann `tools/deploy.sh --ota …`.
