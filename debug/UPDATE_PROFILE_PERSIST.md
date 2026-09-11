# Profile-Persistenz + UI + CI

Stand: **2026-09-11** (Abend, Weiterarbeit ohne Fahrer).

## Was

1. **NVS-Persistenz** für `ProfileStore` — gleiches Muster wie PowerMap:
   - `save()` / `load()` als Arduino-freier Byte-Blob (`ERGP` magic, v1)
   - App: Namespace `ergoprofs`, Key `profs`
   - Boot: laden; wenn leer → Vorlagen `standard`/`reha` seeden und sofort sichern
   - Jede Mutation (`put` / `delete` / `select`) schreibt NVS
2. **UI-Reiter Profile** — Liste, Wählen, Auswahl aufheben, Detailkarte des Aktiven
3. **GitHub Actions** `.github/workflows/build.yml` — native tests, dann `pio run -e ergo`

## Hosttests

139 Fälle (vorher 136), neu: Roundtrip, Bad-Magic, leerer Store.

## Flash

~66,5 % nach diesem Stand (UI wächst weiter).

## Smoke nach OTA

```bash
curl http://192.168.178.88/api/profile/list
curl -X POST 'http://192.168.178.88/api/profile/select?id=reha'
# Neustart / OTA — Auswahl und Grenzen müssen bleiben
curl http://192.168.178.88/api/profile/list
```

LittleFS bleibt bewusst offen; NVS reicht für ≤4 Profile.
