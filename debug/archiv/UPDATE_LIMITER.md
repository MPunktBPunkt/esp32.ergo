# Update-Build — 2026-09-11 (Limiter)

Nach Pull `399d862..375e052` gebaut und getestet.

## Was kam rein

| Commit | Inhalt |
|--------|--------|
| `b4cfebd` | README + `platformio.ini` (Kommentar zum `[common]`-Muster, Limiter in `build_src_filter`) |
| `375e052` | `src/control/Limiter.{h,cpp}` + `test/test_limiter/` |

Limiter = einziger Schreibpfad: Opcode-Whitelist, Klemmen, Rasterung, Rampe, Deadman.
Zeit kommt als Parameter (kein `millis()`), deshalb hosttestbar.

## Ergebnis

```text
pio test -e native
  test_limiter  21/21 PASSED
  test_codec    22/22 PASSED
  → 43/43 in ~1.4 s

pio run -e ergo
  → SUCCESS
  RAM:   18472 / 327680  (5.6 %)
  Flash: 258581 / 1966080 (13.2 %)   (+160 B vs. vorherigem Codec-only-Link)
```

PlatformIO-Fix aus dem ersten Build bleibt erhalten und ist im Upstream dokumentiert.

## Offen / nächster Baustein

Limiter ist isoliert fertig, aber noch nicht an BLE/App angebunden.
Als Nächstes laut Architektur: `BleCentral` / `FtmsClient`, dann Limiter davor schalten.
