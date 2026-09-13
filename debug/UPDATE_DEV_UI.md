# UPDATE — Betrieb vs. Entwickler-UI (0.3.18-dev / Fix 0.3.19)

Stand: 2026-09-13.

## Entscheidung

| Betrieb (immer) | Entwickler (`showDevUi`) |
|-----------------|--------------------------|
| Ride, Workouts, Tests, Verlauf, Profile, Geräte, Einstellungen, OTA | Kalibrierung, Debug |
| Bridge Difficulty/HR, Hub-Basics | Simulation 0x11, ERG-Assist, Gerätequirks |

**Warum Checkbox, nicht zweite URL?** Eine NVS-Flag (`show_dev`) hält die
Alltags-UI schlank und speichert die Freischaltung geräteweit. Hash auf
versteckte Reiter fällt auf Ride zurück.

**0.3.19:** Zone-Render darf `body.className` nicht ohne `devui` setzen — sonst
blendet CSS die Dev-Tabs trotz freigeschalteter Flag aus.

Kanondoc: [docs/ergometer/WEBINTERFACE.md](../docs/ergometer/WEBINTERFACE.md) §7.
