# UPDATE — Nachtest-Force & Probe-Fixes (0.3.10-dev)

Stand: **2026-09-13**. Baut auf [UPDATE_NACHTEST_PROBE.md](UPDATE_NACHTEST_PROBE.md)
und den HW-Befunden [HW_NACHTEST_20260913.md](HW_NACHTEST_20260913.md).

## Was

### Test 3 — `0x05` einmalig durch den Limiter
- `LimiterConfig::allowUntrustedPower` (Default aus)
- `POST /api/control/power?watt=100&raw=1&force=1` setzt den Flag **nur für diesen Write**, dann zurück
- Verdict bleibt `Clamp` mit Reason `UNTRUSTED 0x05…` (sichtbar, nicht „Allow“)
- UI: Button „Test 3 · force 0x05“ (Confirm-Dialog)

Ohne `force` bleibt der Deny wie bisher („Gerät meldet kein Wattziel“).

### Simulation-Rest nach STOP
- Bei `allowSimulation` und Bike-Link: STOP sendet zusätzlich `setSimulation(0,0,40,51)` (Grade 0)

### Probe / Journal
- `probe.hrDelta` = bike − strap
- Journal `cmd` bis **8 Byte** (Simulation `0x11` = 7), JSON `cmdLen`

## Nicht in diesem Paket
- Nachtest 6 Crash
- Dauerhaft `allowUntrustedPower` / produktives `0x05`
- Bridge/MyWhoosh-Abnahme

## Tests
`pio test -e native -f test_limiter` (inkl. `test_wattziel_untrusted_force`)
