# UPDATE — Bridge Resistance-Takeover (0.3.16-dev)

Stand: 2026-09-13. Live-Befund: manueller Gang in MyWhoosh wirkte tot, weil
Resistance während `driving` immer verworfen wurde.

## Fix

- Resistance **kurz nach SetPower** (&lt; 3 s) weiter ignorieren (ERG-Spam)
- Danach: **Takeover** → `MANUAL_LEVEL`, Stufe anwenden, ERG aus
- Status/`nachtest_watch`: `lastOp`, `lastResistTenths`, `resistIgnored`,
  `driving`, `exclusive`, …

Kanondoc: [docs/ergometer/BRIDGE.md](../docs/ergometer/BRIDGE.md).
