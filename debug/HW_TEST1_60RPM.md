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

## Lesart (nach Fahrer-Rückmeldung)

Fahrer hat **kein Profil umgestellt**. Trotzdem war `reha` aktiv — Rest aus
früheren Agenten-Tests (NVS-Persistenz). Das passt exakt zum Gefühl „ab irgendwann
kein schwererer Widerstand mehr": ab Plan-Stufe 10 hat der Limiter auf **8,0**
geklemmt. Das Training an der Konsole kann trotzdem schwerer gehen (andere
Programme / Watt-Modus).

1. **Stufe wirkt zumindest bis ~8.** 28 W → 91 W bei ~60 rpm ist kein
   Quittungs-Phantom.
2. **Plateau ~90 W ab „Stufe 8…16"** = Messartefakt durch Reha-Deckel, kein
   Beweis für eine echte Stufendecke des Bikes.
3. Kadenz driftete zeitweise aufs **50-rpm-Band** (Stufen 4 und 6).

## Folge / Reset 2026-09-11

- Firmware: Sweep-Plan wird auf Profil-Max gekürzt (kein stilles Vergiften mehr).
- Gerät zurückgesetzt: Profil **standard** (max 16,0 / 300 W), Kennfläche **leer**.
- **Nächster Schritt:** Test 1 (60 rpm) erneut fahren.
