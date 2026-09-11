# Test 1 — Stufen-Sweep 60 rpm (Hardware)

Stand: **2026-09-11** Abend. Fahrer meldet: Test 1 abgeschlossen.

## Abgelesen von `.88` nach dem Lauf

Profil **jetzt:** `reha` (max Stufe **8,0** / 100 W).  
Kennfläche: 14 Punkte, **8 Sweep-Zellen**, 9 Stufen belegt.

| Stufe | Band | Watt | Sweep? |
|------:|------|-----:|:------:|
| 1,0 | 60 rpm | 28 | |
| 2,0 | 60 rpm | 32 | |
| 2,0 | 50 rpm | 30 | * |
| 4,0 | 50 rpm | 50 | * |
| 6,0 | 50 rpm | 69 | * |
| 8,0 | 60 rpm | 91 | * |
| 10,0 | 60 rpm | 92 | * |
| 12,0 | 60 rpm | 92 | * |
| 14,0 | 60 rpm | 93 | * |
| 16,0 | 60 rpm | 90 | * |

(Zusätzlich passive Zellen bei Stufe 1 in anderen Bändern.)

## Lesart

1. **Stufe wirkt zumindest bis ~8.** 28 W → 91 W bei ~60 rpm ist kein
   Quittungs-Phantom. Das stützt den Stufenkanal stärker als der noch offene
   Hand-Beweis allein.
2. **Plateau ~90 W ab Stufe 8…16.** Entweder Decke des Bikes / der Fahrleistung,
   **oder** stilles Klemmen durch Profil `reha` (Limiter max 8,0): der Sweep
   *plante* 10…16, schrieb aber 8, und speicherte die Plan-Stufe in die Map.
3. Kadenz driftete zeitweise aufs **50-rpm-Band** (Stufen 4 und 6) — Punkte
   trotzdem gültig (Drift-Regel).

## Folge

- Firmware: Sweep-Plan wird auf `limiter.effectiveMaxLevelTenths()` gekürzt;
  UI/API meldet `clipped`.
- **Empfehlung:** Profil **standard** wählen, Kennfläche verwerfen, Test 1
  wiederholen — sonst sind 10…16 in der Map nicht vertrauenswürdig.
- Wenn mit `standard` dasselbe Plateau kommt: Decke ~90–100 W bei 60 rpm →
  Nachtest-Entscheidung „um 130 W Extrapolation / Deckel", Simulation `0x11`
  wird wichtiger.

Hand-Beweis §3 bleibt formal offen, ist aber durch den Anstieg 1→8 weniger
dringlich als vorher.
