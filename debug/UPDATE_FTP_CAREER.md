# UPDATE — FTP-Karriere + Profile 6 + OTA-Schutz

Stand: **2026-09-13**, Firmware **0.3.6-dev**.

## Was

### FTP-Karriere
- 8 Stufen: Warmup → SS 3×12 → SS 2×20 → TH 4×8 → FTP 2×20 → O/U → VO₂ 5×4 → Ramp
- Persistenz `/progression/ftp_career.json`
- API: `GET /api/ftp-career`, `POST …/accept|decline|set?unlocked=`
- Nach sauberem `done` der aktuellen Stufe: Angebot „Nächste Stufe“
- Gesperrte Stufen: Start mit `force=1` / UI-Confirm
- UI: Karte unter Verlauf + Schloss auf Workout-Karten

### Profile
- `kMaxProfiles` 4→**6**, `kMaxBytes` 384→**512**

### OTA-Schutz
- Hub-OTA wird aufgeschoben, solange Bike verbunden oder Session aktiv
- `/ota-upload` antwortet 409 in dem Fall

## Tests
`pio test -e native -f test_ftp_career -f test_profile`
