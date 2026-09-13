# Hardware-Nachtests — 2026-09-13

Firmware **0.3.9-dev** auf `192.168.178.88` (OTA von 0.3.5-dev).
Profil **martin** (max Stufe 16 / 280 W, FTP 140).
Bike **TC174** `c2:32:a5:1e:bf:b5`, Gurt **Polar H9** `24:ac:ac:1d:f6:ca`.

Rohmitschnitt (2‑s‑Ticks): [`captures/nachtest-20260913T085205Z.jsonl`](captures/nachtest-20260913T085205Z.jsonl)
(518 Zeilen, 08:52–09:11 UTC / 10:52–11:11 lokal). Probe-Ring war an (`ibdEvery=1`).

---

## Vorbereitung

| Check | Stand |
|-------|--------|
| OTA 0.3.9-dev | ok, Codec-Selbsttest ok |
| `allowSimulation` | an (Einstellungen) |
| `POST /api/probe/arm` | Ring an, Mark `arm` |
| Watch `nachtest_watch.sh` | parallel gelaufen |
| Dual-Link vor Tests | Bike READY + HR READY |

---

## Test 3 — Wattziel `0x05` (raw)

**Aktion:** UI „Test 3 · raw 100 W“ / `POST /api/control/power?watt=100&raw=1` bei ~100 W / ~75 rpm.

**Ergebnis: nicht am Draht ausgeführt.**

Der Limiter hat abgelehnt, bevor Bytes das Bike erreichten:

```text
reason: "Geraet meldet kein Wattziel"
caps.targetPower = false, caps.powerTrusted = false
HTTP 409, result=2 (Denied)
```

Marks `pre_raw05` zeigen den Live-Zustand vor dem Versuch (~100–104 W @ 73–75 rpm).
Im Journal erscheint **kein** Opcode `05` — nur Stufen-`04…` aus MANUAL_LEVEL.

**Bedeutung:** Der klassische Nachtest („Success ohne Wirkung“) ist heute **nicht** belegt.
Belegt ist nur: Produktivpfad lässt untrusted `0x05` nicht durch. Für den Draht-Nachweis
bräuchte es einen bewussten Bypass (`force=1`).

---

## Test 4 — Simulation `0x11`

Gleiche Trittfrequenz ~74–77 rpm, Stufe unverändert **7,0** (70 Tenths). Steigung in %:

| Grade | Draht (gekürzt im Journal) | Quittung | Urteil | pre → post W | pre → post rpm | Δ W/rpm |
|------:|----------------------------|----------|--------|--------------|----------------|--------|
| 1 % | `11000064` | Success | **NO_EFFECT** | 102 → 103 | 74,2 → 74,5 | 0 % |
| 3 % | `1100002C`… | Success | **WORKS** | 106 → 128 | 76,4 → 74 | +25 % |
| 6 % | `11000058`… | Success | **WORKS** | 131 → 157 | 75,3 → 71,9 | +26 % |

Live-Peak unter 6 %: **~174 W @ 78 rpm** (Stufe weiter 7).

Marks: `pre_sim_g1` 100 W, `pre_sim_g3` 104 W, `pre_sim_g6` 130 W.

**Entscheidung:** `0x11` ist ein **wirksamer** Steuerkanal. Success allein lügt bei kleinen
Grades (1 %): `contradictory: true`. Ab ~3 % mess- und spürbar. Bridge-Passthrough nach
Freigabe (`allowSimulation`) ist architektonisch sinnvoll; feine Last unterhalb der
Stufenquantisierung geht über Simulation.

STOP danach: Mode OFF, `08 01` gesendet, Session `endReason=stop`, ~323 s, Ø ~111 W.

---

## Dual-Link / Pulsquellen (Nebenbei zu Test 5)

Beide Links gleichzeitig **READY** während der Sim-Session und danach.

| Quelle | Beobachtung |
|--------|-------------|
| Strap (`0x180D`) | führend (`hrSource=strap`) |
| Bike `2AD2` HR | durchgängig höher |

312 Ticks mit beiden Werten: mittlere Differenz **Bike − Strap ≈ +25 bpm**
(Beispiel früh 83 vs. 66, später 90 vs. 77). GymLink-Kette ist für Regelung
ungenau/träge — für Dashboard ok, für `HR_HOLD` nur mit Vorsicht.

~10‑Minuten-Stabilität unter Last: ja (Sim-Block ohne Linkabriss).

---

## Reconnect (Bike aus → an)

| Zeitpunkt (UTC) | Ereignis |
|-----------------|----------|
| 09:07:47 | Bike **LOST**, `losses: 1`, `bikeLink: false`, HR blieb READY |
| 09:07:50–09:08:48 | ESP kurz **unreachable** (WLAN-Blip unter BLE-Last) |
| 09:08:53 | Bike wieder **READY**, `reconnects: 1`, HR READY |

**Entscheidung:** Reconnect über gemerkte MAC funktioniert. Kurzer ESP-WLAN-Aussetzer
beim Linkverlust notieren — kein Brick, UI kam zurück.

---

## Nicht gefahren heute

| Test | Status |
|------|--------|
| Test 1 / 2 Sweep | bereits früher belegt; heute nicht wiederholt |
| Test 3 Draht-`0x05` | blockiert (siehe oben) — Bypass offen |
| Test 6 Crash unter Last | **offen** |
| Bridge / MyWhoosh-Abnahme | **offen** |
| ERG/HR/Reha Fahrer-Abnahme | Mechanik da, formelle Abnahme offen |

---

## Journal-Kurzstatistik (Session-Ende / danach)

Aus Status nach den Tests (Ring weiter an, weitere LEVEL-Writes möglich):

- `worked: 4` (u. a. Stufe 6→7, Sim 3 %, Sim 6 %, spätere Stufe)
- `noEffect: 2` (Sim 1 %, teils `08 01` bei schon fallender Kadenz)
- `contradictions: 2` (Success ohne W/rpm-Änderung — Sim 1 % und ggf. Stop-Fenster)

---

## Artefakte

| Datei | Inhalt |
|-------|--------|
| `debug/captures/nachtest-20260913T085205Z.jsonl` | Watch-Ticks |
| `dist/ergo.0.3.9-dev.esp32s3.bin` | geflashte Bin |
| `debug/UPDATE_NACHTEST_PROBE.md` | Firmware-Probe-API |

---

## Nächste Hardware-Schritte

1. Optional: `raw=1&force=1` für echten `0x05`-Drahtversuch.  
2. Test 6: Stufe ~10, ESP-Reset **ohne** vorheriges STOP.  
3. Bridge an → MyWhoosh findet Trainer, ERG/Difficulty/HR-Deckel.  
4. `allowSimulation` nach dem Tag wieder **aus**, wenn nicht dauerhaft gebraucht.
