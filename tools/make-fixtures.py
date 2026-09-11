#!/usr/bin/env python3
"""Erzeugt die Testfixtures fuer FtmsCodec aus einem Sondenlauf.

Die Sollwerte kommen NICHT aus dieser Firmware, sondern aus der bereits
validierten Referenzimplementierung tools/ftms.py der Sonde. Damit prueft der
C++-Test gegen eine unabhaengige zweite Implementierung statt gegen sich
selbst.

    python tools/make-fixtures.py \
        --scan ../esp32.ftmsprobe/docs/ergometer/scan-20260910 \
        --ftms ../esp32.ftmsprobe/tools/ftms.py

Das erzeugte Header-File wird eingecheckt, damit der Test ohne das
Sonden-Repo laeuft.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
DEFAULT_SCAN = ROOT.parent / "esp32.ftmsprobe" / "docs" / "ergometer" / "scan-20260910"
DEFAULT_FTMS = ROOT.parent / "esp32.ftmsprobe" / "tools" / "ftms.py"
OUT = ROOT / "test" / "test_codec" / "fixtures_ibd.h"

# Reihenfolge muss zu ftms::Field in src/ble/FtmsTypes.h passen.
FIELD_BITS = [
    ("speed", 0, "speedRaw"),
    ("avg_speed", 1, "avgSpeedRaw"),
    ("cadence", 2, "cadenceRaw"),
    ("avg_cadence", 3, "avgCadenceRaw"),
    ("distance", 4, "distanceM"),
    ("resistance", 5, "resistanceRaw"),
    ("power", 6, "powerW"),
    ("avg_power", 7, "avgPowerW"),
    ("energy", 8, "energyTotalKcal"),
    ("heart_rate", 9, "heartRateBpm"),
    ("met", 10, "metRaw"),
    ("elapsed_time", 11, "elapsedS"),
    ("remaining_time", 12, "remainingS"),
]

# ftms.py benennt die Felder anders als das C++-Struct.
REF_KEY = {
    "speed": "speed",
    "avg_speed": "speed_avg",
    "cadence": "cadence",
    "avg_cadence": "cadence_avg",
    "distance": "distance",
    "resistance": "resistance",
    "power": "power",
    "avg_power": "power_avg",
    "energy": "energy_total",
    "heart_rate": "heart_rate",
    "met": "met",
    "elapsed_time": "elapsed_s",
    "remaining_time": "remaining_s",
}


def load_ftms(path: Path):
    spec = importlib.util.spec_from_file_location("ftms_ref", path)
    if spec is None or spec.loader is None:
        raise SystemExit(f"ftms.py nicht ladbar: {path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def collect_packets(scan_dir: Path) -> list[str]:
    """Alle eindeutigen 0x2AD2-Notifies in Aufzeichnungsreihenfolge."""
    seen: set[str] = set()
    out: list[str] = []
    for name in ("bike-data.jsonl", "probe-log.jsonl"):
        path = scan_dir / name
        if not path.exists():
            continue
        for line in path.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line:
                continue
            try:
                rec = json.loads(line)
            except json.JSONDecodeError:
                continue
            if rec.get("uuid") != "2AD2" or rec.get("dir") != "notify":
                continue
            hx = (rec.get("hex") or "").upper()
            if not hx or hx in seen:
                continue
            seen.add(hx)
            out.append(hx)
    return out


def raw_of(parsed: dict, key: str):
    """Wire-Rohwert, nicht den skalierten Wert."""
    raws = parsed.get("_raw", {})
    if key in raws:
        return raws[key]
    return parsed.get(key, 0)


def build_rows(packets: list[str], ftms) -> list[dict]:
    rows = []
    for hx in packets:
        data = bytes.fromhex(hx)
        try:
            p = ftms.parse_indoor_bike_data(data)
        except ftms.FtmsParseError as exc:
            print(f"  uebersprungen {hx}: {exc}", file=sys.stderr)
            continue
        flags = p["flags"]
        presence = 0
        values: dict[str, int] = {}
        for name, bit, member in FIELD_BITS:
            present = (flags & 1) == 0 if bit == 0 else bool(flags & (1 << bit))
            if not present:
                continue
            presence |= 1 << bit
            values[member] = int(raw_of(p, REF_KEY[name]))
        rows.append(
            {
                "hex": hx,
                "data": data,
                "flags": flags,
                "presence": presence,
                "consumed": p["_consumed"],
                "trailing": p["_trailing"],
                "values": values,
                # Nebenfelder von "energy", die nicht am Bit haengen
                "energyPerHourKcal": int(raw_of(p, "energy_per_hour")) if presence & (1 << 8) else 0,
                "energyPerMinKcal": int(raw_of(p, "energy_per_min")) if presence & (1 << 8) else 0,
            }
        )
    return rows


MEMBERS = [m for _, _, m in FIELD_BITS] + ["energyPerHourKcal", "energyPerMinKcal"]


def emit(rows: list[dict], scan_dir: Path) -> str:
    lines = [
        "// Automatisch erzeugt von tools/make-fixtures.py — nicht von Hand aendern.",
        f"// Quelle: {scan_dir.name}, Sollwerte aus esp32.ftmsprobe/tools/ftms.py",
        f"// {len(rows)} eindeutige 0x2AD2-Pakete",
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
        "struct IbdFixture {",
        "    const char* hex;",
        "    uint8_t data[30];",
        "    uint8_t len;",
        "    uint16_t flags;",
        "    uint16_t presence;",
        "    uint8_t consumed;",
        "    uint8_t trailing;",
        "    uint16_t speedRaw;",
        "    uint16_t avgSpeedRaw;",
        "    uint16_t cadenceRaw;",
        "    uint16_t avgCadenceRaw;",
        "    uint32_t distanceM;",
        "    int16_t resistanceRaw;",
        "    int16_t powerW;",
        "    int16_t avgPowerW;",
        "    uint16_t energyTotalKcal;",
        "    uint16_t energyPerHourKcal;",
        "    uint8_t energyPerMinKcal;",
        "    uint8_t heartRateBpm;",
        "    uint8_t metRaw;",
        "    uint16_t elapsedS;",
        "    uint16_t remainingS;",
        "};",
        "",
        "static const IbdFixture kIbdFixtures[] = {",
    ]
    for r in rows:
        v = dict.fromkeys(MEMBERS, 0)
        v.update(r["values"])
        v["energyPerHourKcal"] = r["energyPerHourKcal"]
        v["energyPerMinKcal"] = r["energyPerMinKcal"]
        body = ", ".join(f"0x{b:02X}" for b in r["data"])
        lines.append(f'    {{ "{r["hex"]}", {{ {body} }}, {len(r["data"])},')
        lines.append(f'      0x{r["flags"]:04X}, 0x{r["presence"]:04X}, {r["consumed"]}, {r["trailing"]},')
        lines.append(
            "      "
            + ", ".join(
                str(v[m])
                for m in [
                    "speedRaw",
                    "avgSpeedRaw",
                    "cadenceRaw",
                    "avgCadenceRaw",
                    "distanceM",
                    "resistanceRaw",
                    "powerW",
                    "avgPowerW",
                    "energyTotalKcal",
                    "energyPerHourKcal",
                    "energyPerMinKcal",
                    "heartRateBpm",
                    "metRaw",
                    "elapsedS",
                    "remainingS",
                ]
            )
            + " },"
        )
    lines += [
        "};",
        "",
        "static const size_t kIbdFixtureCount = sizeof(kIbdFixtures) / sizeof(kIbdFixtures[0]);",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--scan", type=Path, default=DEFAULT_SCAN)
    ap.add_argument("--ftms", type=Path, default=DEFAULT_FTMS)
    ap.add_argument("--out", type=Path, default=OUT)
    args = ap.parse_args()

    if not args.scan.is_dir():
        raise SystemExit(f"Scan-Ordner fehlt: {args.scan}")
    if not args.ftms.is_file():
        raise SystemExit(f"Referenz-Codec fehlt: {args.ftms}")

    ftms = load_ftms(args.ftms)
    packets = collect_packets(args.scan)
    print(f"{len(packets)} eindeutige 0x2AD2-Pakete gefunden")
    rows = build_rows(packets, ftms)
    if not rows:
        raise SystemExit("keine verwertbaren Pakete")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(emit(rows, args.scan), encoding="utf-8")
    print(f"{len(rows)} Fixtures -> {args.out}")

    flags = {r["flags"] for r in rows}
    print("Flag-Varianten: " + ", ".join(f"0x{f:04X}" for f in sorted(flags)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
