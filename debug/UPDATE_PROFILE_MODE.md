# Update — Profile + Steuermodus OFF/MANUAL_LEVEL

Build-Instanz, 2026-09-11. Antwort auf STATE.md §5.4–5.5 (ohne Hardware-Zwang).

## Gebaut

| Datei | Inhalt |
|-------|--------|
| `src/core/Profile.h` | POD + `ProfileStore` (max 4, kein stilles Default) |
| `src/core/ProfileStore.cpp` | put/get/remove/select/applyTo → Limiter |
| `src/control/ControlMode.{h,cpp}` | OFF / MANUAL_LEVEL (Rest → 501) |
| `test/test_profile/` | 10 Fälle |
| `test/test_mode/` | 7 Fälle |
| `App.*` | APIs, Statusfelder, Seed-Profile `standard` + `reha` |

## Tests / Flash

```text
pio test -e native → 136/136
pio run -e ergo    → RAM 21.8 %, Flash 66.2 %
OTA .88            → ok, wide=true
```

Auf Hardware verifiziert:

- `GET /api/profile/list` → standard + reha, `active=null`
- `POST /api/profile/select?id=reha` → Limiter max Stufe 8.0 / 100 W
- `POST /api/control/mode?mode=level&tenths=20` → MANUAL_LEVEL, Write ok, sint16
- Select während MANUAL_LEVEL → 409
- Stop → mode OFF, danach Select standard ok

## API (Kurz)

```bash
curl http://$IP/api/profile/list
curl -X POST "http://$IP/api/profile/select?id=reha"
curl -X POST "http://$IP/api/control/mode?mode=level&tenths=10"
curl -X POST "http://$IP/api/control/level?tenths=160"   # Rampe; Profil klemmt bei reha auf 80
curl -X POST "http://$IP/api/control/stop"              # → OFF
```

## Noch nicht

- LittleFS-Persistenz der Profile
- UI-Reiter Profile (API reicht für Handtest)
- MANUAL_ERG / HR_HOLD / WORKOUT
- Hand-an-Kurbel-Beweis Stufe 1 vs 16 (STATE §3) — **wenn du daheim bist**
