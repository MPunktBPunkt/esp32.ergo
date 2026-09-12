# UPDATE — Zonenschiene + zonengeführte Ride-UI

Stand: 2026-09-12

## Was

v0.1-UI-Kern aus WEBINTERFACE.md §2/§3:

- **`Zone`** — Leistungszonen (% FTP, Z1–Z7) und Pulszonen (% HRmax, Z1–Z5)
- **Session** akkumuliert Zeit je Zone; Persistenz + Hub + `/api/session/*`
- **Status/SSE** liefert `zone.{index,code,name,color,lead}`
- **Ride**: Ambient-Hintergrund, Hero in Zonenfarbe, Badge `Z3 · Tempo`, Zonenschiene
- **Verlauf**: Mini-Zonenbalken je Session
- Ist/Ziel-Chart folgt der Zonenfarbe

## Ohne Fahrer prüfbar

- `pio test -e native` (inkl. `test_zone`, Zone-Akkumulation)
- Browser: FTP/HRmax im Profil setzen → Zone wechselt mit Leistung/Puls in SSE
- Hinweis: ohne FTP bleibt Leistungszone leer (UI sagt das)

## Nicht in diesem Paket

Tablet-Voll-Layout, Geisterlinie, Workout-Editor, Tests-Reiter (v0.2).
