# Empfehlungen (Handoff)

## Erledigt auf Hardware (`.88`)

Siehe **[HW_TEST_REPORT.md](HW_TEST_REPORT.md)**: OTA, Caps, Limiter-Rampe, Power-Deny,
Stop, Live-IBD (teilweise unter Tritt), leerer Sweep-Abort.

## Als Nächstes (Entwurf / nächste HW-Session)

1. `caps.wide=false` vs. dokumentiertes sint16 klären
2. Mit Fahrer: Wattmittel Stufe 4 vs. 8, dann Sweep 60/80 rpm
3. Reconnect (Bike aus/an), Pulsgurt
4. Steuermodi laut Pflichtenheft (`OFF` / `MANUAL_LEVEL` / `MANUAL_ERG` / …)
5. CI `build.yml` (Badge noch 404)

## Nicht brechen

Shell/`/ota-upload`, kein `[env]`-Block, Codec/Limiter/PowerMap/Sweep hosttestbar,
jeder Write nur durch Limiter, kein `0x05`-Steuerweg am Varon.
