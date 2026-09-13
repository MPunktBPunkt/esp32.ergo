# UPDATE — Gzip-UI + TestRunner (0.1.2-dev)

Stand: 2026-09-12

## Paket 1 — Flash/UI

- Quelle: `web/index.html`
- Build: `tools/pack_ui.py` / `pre:tools/pio_pack_ui.py` → `include/UiPagesGz.h`
- Serve: `Content-Encoding: gzip` + `send_P(..., PAGE_MAIN_GZ_LEN)`
- Gewinn: ~110 kB → ~30 kB PROGMEM (~80 kB App-Flash frei)

## Paket 2 — TestRunner

- `src/control/TestRunner.{h,cpp}` + Hosttests
- Beobachtet Watt/Puls während Builtin-Tests
- Rampe: bestes gleitendes 60‑s‑Mittel = MAP → FTP 0,75×
- 20 min: Ø nur im Schritt „Haupt…“ → FTP 0,95×
- Recovery: HR Start→Ende Erholung → Note 1–10
- API: `GET /api/test/result`, `POST /api/test/accept-ftp`
- UI Tests-Reiter liest Firmware-Ergebnis (keine Client-Heuristik mehr)

## Grenzen

- 20‑Min-Hauptblock weiter ERG @ 100 % FTP (noch nicht self-paced)
- Recovery-Note grob skaliert (nicht 1:1 Konsolenformel)
- Rampe max. 16 Stufen (60…360 W)

## Paket 3 — 20-Min self-paced (2026-09-12)

- `WorkoutStep.selfPaced` + JSON `target.self_paced`
- Builtin `test_20min`: Warm ERG → Haupt open → Cool ERG
- Während Haupt: Stufe ± aktiv, kein ERG; TestRunner Ø nur Haupt
- `/api/control/level` erlaubt Stufe im laufenden Workout ohne Moduswechsel
