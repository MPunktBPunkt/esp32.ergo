# UPDATE — Bridge Exclusive Control (0.3.14-dev)

Stand: 2026-09-13. Antwort auf den MyWhoosh-Kampf (Coach + App + Power↔Stufe).

## Frage

Muss bei aktivem Bridge-Teilnehmer MyWhoosh die komplette Steuerung übernehmen,
wenn die App Regulierungsdaten schickt? Kann man Observer vs. Controller
unterscheiden?

**Ja / Ja.** Nur IBD-Abo oder Request Control ohne Last = Observer. Erstes
Lastkommando (Watt / Stufe / Sim) = Controller → Coach-Last gesperrt.

## Rollen (`FtmsServer::clientRole`)

| Role | Bedingung |
|------|-----------|
| `none` | kein Client |
| `connected` | verbunden, kein IBD-Abo, keine Control-Grant |
| `observer` | IBD-Abo und/oder Control granted, **noch kein** Lastkommando |
| `controller` | mind. ein SetTargetPower / SetResistance / SetIndoorBikeSimulation |

Felder in `/api/status` → `bridge` und `/api/bridge`:
`controlling`, `clientRole`, `loadCommands`, `exclusive`.

## Exklusiv-Lock (Coach)

Solange `bridge.isControlling()`:

- Coach-API für Last → **409** (`mode`≠off, `level`, `power`, `sim`, Workout-Start, Sweep)
- UI: Last-Buttons disabled, Hinweis „Bridge-App steuert exklusiv“
- **STOP** bleibt und ruft `bridge.releaseController()` auf

Lock endet auch bei App-Stop/Reset, Disconnect, Bridge aus.

## MyWhoosh Power↔Resistance

Während `bridgeDriving_` (Bridge-ERG aktiv): eingehende **SetResistance** werden
angewendet **nicht** (BLE schon Success) — Log
`Stufe … ignoriert — ERG aktiv`. Verhindert Mode-Kampf und Stufen-Jagd.

## Nicht gelöst hier

- ERG-Stufenjagd durch PowerController/Kadenz (separates Dämpfen)
- MyWhoosh-Abnahme auf Hardware nach diesem Stand
