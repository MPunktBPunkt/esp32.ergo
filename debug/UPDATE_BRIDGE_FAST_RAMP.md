# UPDATE — Bridge schnelle Stufen-Rampe (0.3.17-dev)

Stand: 2026-09-13. Live: manuelle MyWhoosh-Gänge wirkten träge (Limiter 1 Stufe / 2 s).

## Fix

- `Limiter::setRampOverrideMs(500)` während Bridge-LEVEL-Ziel
- Pending `bridgeLevelWant_` + Retry in `loopBridge` bis erreicht / Denied / ERG
- Coach/ERG behalten Default **2000 ms**
- Runter weiter sofort (unverändert)

Hosttest: `test_ramp_override_faster`.
