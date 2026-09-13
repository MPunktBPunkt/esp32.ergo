# UPDATE — Nachtest-Ergebnisse 2026-09-13 Abend (0.3.20-dev)

Stand: 2026-09-13. Baut auf [HW_NACHTEST_20260913.md](HW_NACHTEST_20260913.md)
(Vormittag) und den Kalibrierprotokollen.

## Test 3 — `0x05` am Draht (`force=1`)

| | |
|--|--|
| Zeitpunkt | ~14:50:19Z / 16:50 lokal, FW 0.3.19-dev |
| Mark | `pre_raw05_force` 96 W @ 81 rpm, Stufe 6,0 |
| Draht | `056400` (100 W), Quittung Success |
| Urteil | **NO_EFFECT** (96→101 W, 80,8→84,1 rpm) |

**Entscheidung:** Success ohne Lastwirkung. Produktiv kein `0x05` ans Bike;
ERG weiter über Stufen/`PowerMap`. `force=1` nur Debug/Nachtest.

Capture: `debug/captures/nachtest-live.jsonl`.

## Test 2 — 80 rpm grob

Siehe [HW_TEST2_80RPM.md](HW_TEST2_80RPM.md). Architektur „Fläche“ bestätigt;
Stufe 12 @ 80 rpm optional nachholen.

## Software (0.3.20)

- `DeviceStore`: Varon-Defaults (`sint16`, `powerTrusted=0`) auch nach NVS-Load
  und bei erneutem `remember` der Labor-MAC / Name `TC174` nachziehen, falls
  `powerTrusted` noch „auto“ (−1) war.
