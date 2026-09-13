# Test 2 grob — 80 rpm, Stufen 4 / 8 / 12 / 16

Stand: **2026-09-13** ~16:55 lokal (Firmware 0.3.19-dev).
Profil **standard**. Capture: `debug/captures/nachtest-live.jsonl`
(Sweep-Ende ~14:56:39Z).

Ergänzt den leichten Lauf [HW_TEST2_LIGHT.md](HW_TEST2_LIGHT.md)
(nur 4+8, 2026-09-11).

## Sweep-Punkte (`calib.sweep`, targetRpm 80)

| Stufe | Mittel-W | Mittel-rpm | rpm min…max | gültig |
|------:|---------:|-----------:|-------------|:------:|
| 4,0 | **72,7** | 84,7 | 82…87 | ja |
| 8,0 | **122,9** | 79,4 | 78…81 | ja |
| 12,0 | 180,8 | 80,6 | 74…84 | **nein** („Kadenz nicht gehalten“) |
| 16,0 | **244,6** | 81,6 | 79…84 | ja |

3 von 4 Punkten gültig. Stufe 12 verworfen — optional wiederholen.

## Steuer-Journal (Stufenwechsel)

u. a. **WORKS**: 4→8 (71→127 W), 8→12 (121→174 W), 12→16 (175→256 W).
0 Widersprüche auf diesen Writes.

## Vergleich 60 rpm (Test 1) ↔ 80 rpm

| Stufe | 60 rpm ([HW_TEST1_60RPM.md](HW_TEST1_60RPM.md)) | 80 rpm (dieser Lauf) | Δ |
|------:|-----------------------------------------------:|---------------------:|--:|
| 4 | 50 W | **73 W** | +46 % |
| 8 | 89 W | **123 W** | +38 % |
| 12 | 129 W | ~(181, verworfen) | — |
| 16 | 170 W | **245 W** | +44 % |

## Entscheidung

Bei fester Stufe steigt die Leistung mit der Kadenz klar — **Fläche**, keine
Tabelle. Stufe 16 bei 80 rpm liefert ~245 W; der Widerstandskanal trägt den
Alltagsbereich. Spitzen darüber weiter über Kadenz und/oder `0x11`.

Live-Map nach dem Lauf: `levelsCovered≈13`, `ceilingW≈226`, Slot 0 /
MAC `c2:32:a5:1e:bf:b5` (NVS, nicht in git).
