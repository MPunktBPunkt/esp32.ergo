# Empfehlungen (Handoff)

## 0. Erledigt

- Connectivity-Shell auf `.88` (siehe `UPDATE_CONNECTIVITY.md`)
- Codec + Limiter hostgetestet

## 1. Jetzt (Entwurfs-Instanz)

→ **[TODO_PFLICHTENHEFT.md](TODO_PFLICHTENHEFT.md)** — Pflichtenheft Rev. 4 einarbeiten,
Beginn: `BleCentral` + `FtmsClient` + Limiter verdrahten. Shell/OTA nicht anfassen.

## 2. Hygiene (nebenbei ok)

1. GitHub Actions `build.yml` (Badge ist noch 404)
2. Docs aus `private/docs/ergometer/` nach `docs/` spiegeln

## 3. Danach

| Priorität | Baustein |
|-----------|----------|
| hoch | Live-Werte Status/SSE/Hub-ios |
| hoch | Steuermodi OFF / LEVEL / ERG(emuliert) / HR |
| später | Web-UI Charts, Kalibrierung, Workouts (v0.2) |
| später | FTMS-Bridge Peripheral (v0.3) |

Kein `0x05`-Wattziel am Varon. Native-Tests und `[common]`-Muster beibehalten.
