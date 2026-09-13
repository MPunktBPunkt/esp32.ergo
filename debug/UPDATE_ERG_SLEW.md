# UPDATE — ERG Stufen-Slew (0.3.15-dev)

Stand: 2026-09-13. Folgepaket nach Bridge Exclusive: Stufenjagd im
`PowerController` dämpfen (MyWhoosh Watt-Spam + Map-Sprünge).

## Problem

Live-Session: Journal zeigte Stufe `1 ↔ 4 ↔ 7`, viele `UNJUDGED`. Ursachen
neben Power↔Resistance (0.3.14):

1. `setTargetW` setzte bei **jedem** Watt-Update `lastLevel_ = -1` → Sofort-Sprung
2. Map-`bestLevel` springt bei Ziel/Kadenz ohne Zwischenstufen

## Fix

- **Slew:** max. ±1 Stufe (`maxStepTenths=10`) pro Regelzyklus Richtung Map-Wunsch
- **Retarget-Hysterese:** `|ΔZiel| < retargetW` (20 W) → nur Ziel aktualisieren,
  kein I-Reset / kein Sofort-Write / kein Level-Reset
- Erster Write (kein `lastLevel`) darf noch voll springen; Limiter rampt nach oben

Hosttests: `test_slew_limits_jump`, `test_small_retarget_no_immediate_write`.

## Nicht hier

Limiter-Rampe (schon asymmetrisch), Resistance-Ignore (0.3.14), Sim-Assist.
