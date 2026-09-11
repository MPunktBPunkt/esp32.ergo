#!/usr/bin/env python3
"""Erzeugt die Testfixtures fuer FtmsCodec aus einem Sondenlauf oder Debug-Export.

Die Sollwerte kommen NICHT aus dieser Firmware, sondern aus der bereits
validierten Referenzimplementierung tools/ref/ftms.py (Kopie aus der Sonde).
Damit prueft der C++-Test gegen eine unabhaengige zweite Implementierung.

    # voller Laborlauf (wenn nodes/esp32.ftmsprobe nebenan liegt):
    python tools/make-fixtures.py

    # Debug-Export / eingecheckte Stichprobe (Abnahmekriterium 6a):
    python tools/make-fixtures.py \
        --scan test/fixtures/debug-export \
        --out /tmp/fixtures_from_export.h

    # Roundtrip-Pruefung ohne die kuratierte fixtures_ibd.h zu ueberschreiben:
    python tools/make-fixtures.py --verify-curated
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import re
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
DEFAULT_SCANS = [
    ROOT.parent / "nodes" / "esp32.ftmsprobe" / "docs" / "ergometer" / "scan-20260910",
    ROOT.parent / "esp32.ftmsprobe" / "docs" / "ergometer" / "scan-20260910",
    ROOT / "test" / "fixtures" / "debug-export",
]
DEFAULT_FTMS = [
    HERE / "ref" / "ftms.py",
    ROOT.parent / "nodes" / "esp32.ftmsprobe" / "tools" / "ftms.py",
    ROOT.parent / "esp32.ftmsprobe" / "tools" / "ftms.py",
]
OUT = ROOT / "test" / "test_codec" / "fixtures_ibd.h"
CURATED = OUT

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


def first_existing(paths: list[Path]) -> Path | None:
    for p in paths:
        if p.exists():
            return p
    return None


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
                "energyPerHourKcal": int(raw_of(p, "energy_per_hour")) if presence & (1 << 8) else 0,
                "energyPerMinKcal": int(raw_of(p, "energy_per_min")) if presence & (1 << 8) else 0,
            }
        )
    return rows


MEMBERS = [m for _, _, m in FIELD_BITS] + ["energyPerHourKcal", "energyPerMinKcal"]


def emit(rows: list[dict], scan_dir: Path) -> str:
    lines = [
        "// Automatisch erzeugt von tools/make-fixtures.py — nicht von Hand aendern.",
        f"// Quelle: {scan_dir.name}, Sollwerte aus tools/ref/ftms.py",
        f"// {len(rows)} eindeutige 0x2AD2-Pakete",
        "#pragma once",
        "",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        "struct IbdFixture {",
        "    const char* note;",
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
        lines.append(f'    {{ "", "{r["hex"]}", {{ {body} }}, {len(r["data"])},')
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


def parse_curated_hexes(path: Path) -> dict[str, dict]:
    """Liest hex + Kernfelder aus der kuratierten fixtures_ibd.h."""
    text = path.read_text(encoding="utf-8")
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//.*?$", " ", text, flags=re.M)
    pat = re.compile(
        r'"([0-9A-Fa-f]{10,})",\s*\{([^}]+)\},\s*'
        r"(\d+),\s*0x([0-9A-Fa-f]+),\s*0x([0-9A-Fa-f]+),\s*(\d+),\s*(\d+),\s*"
        r"([-\d,\s]+)\}",
        re.S,
    )
    out: dict[str, dict] = {}
    for m in pat.finditer(text):
        hx = m.group(1).upper()
        nums = [int(x.strip()) for x in m.group(8).split(",") if x.strip()]
        if len(nums) < 15:
            continue
        keys = [
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
        vals = dict(zip(keys, nums[:15]))
        vals["len"] = int(m.group(3))
        vals["flags"] = int(m.group(4), 16)
        vals["presence"] = int(m.group(5), 16)
        vals["consumed"] = int(m.group(6))
        vals["trailing"] = int(m.group(7))
        out[hx] = vals
    return out


def verify_curated(scan_dir: Path, ftms, curated_path: Path) -> int:
    packets = collect_packets(scan_dir)
    rows = {r["hex"]: r for r in build_rows(packets, ftms)}
    curated = parse_curated_hexes(curated_path)
    if not curated:
        print("keine kuratierten Fixtures geparst", file=sys.stderr)
        return 1
    missing = [h for h in curated if h not in rows]
    if missing:
        print(f"fehlend im Export/Scan ({len(missing)}): {missing[:3]}...", file=sys.stderr)
        return 1
    errors = 0
    for hx, expect in curated.items():
        got = rows[hx]
        gv = dict.fromkeys(MEMBERS, 0)
        gv.update(got["values"])
        gv["energyPerHourKcal"] = got["energyPerHourKcal"]
        gv["energyPerMinKcal"] = got["energyPerMinKcal"]
        for k, ev in expect.items():
            if k in ("len", "flags", "presence", "consumed", "trailing"):
                actual = got[k] if k != "len" else len(got["data"])
                if k == "len":
                    actual = len(got["data"])
                elif k in got:
                    actual = got[k]
                else:
                    continue
                if actual != ev:
                    print(f"{hx} {k}: erwartet {ev}, ref {actual}", file=sys.stderr)
                    errors += 1
                continue
            if gv.get(k, 0) != ev:
                print(f"{hx} {k}: erwartet {ev}, ref {gv.get(k)}", file=sys.stderr)
                errors += 1
    if errors:
        print(f"VERIFY FAIL: {errors} Abweichungen", file=sys.stderr)
        return 1
    print(f"VERIFY OK: {len(curated)} kuratierte Pakete matchen ftms.py ({scan_dir})")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--scan", type=Path, default=None)
    ap.add_argument("--ftms", type=Path, default=None)
    ap.add_argument("--out", type=Path, default=OUT)
    ap.add_argument(
        "--verify-curated",
        action="store_true",
        help="Prueft, dass Export/Scan die kuratierten Fixtures gegen ftms.py bestaetigt",
    )
    ap.add_argument(
        "--write-curated",
        action="store_true",
        help="UEBERSCHREIBT fixtures_ibd.h (nur bewusst; Standard ist Stichprobe → temp)",
    )
    args = ap.parse_args()

    scan = args.scan or first_existing(DEFAULT_SCANS)
    ftms_path = args.ftms or first_existing(DEFAULT_FTMS)
    if scan is None or not Path(scan).is_dir():
        raise SystemExit("Scan-Ordner fehlt (test/fixtures/debug-export oder Sondenlauf)")
    if ftms_path is None or not Path(ftms_path).is_file():
        raise SystemExit("Referenz-Codec fehlt (tools/ref/ftms.py)")

    ftms = load_ftms(Path(ftms_path))
    scan = Path(scan)

    if args.verify_curated:
        # Bevorzugt die eingecheckte Debug-Export-Stichprobe.
        sample = ROOT / "test" / "fixtures" / "debug-export"
        return verify_curated(sample if sample.is_dir() else scan, ftms, CURATED)

    packets = collect_packets(scan)
    print(f"{len(packets)} eindeutige 0x2AD2-Pakete gefunden in {scan}")
    rows = build_rows(packets, ftms)
    if not rows:
        raise SystemExit("keine verwertbaren Pakete")

    out = args.out
    if not args.write_curated and out.resolve() == OUT.resolve():
        # Schutz: kuratierte Hand-Fixtures nicht versehentlich plattmachen.
        out = Path(tempfile.gettempdir()) / "fixtures_ibd_generated.h"
        print(f"Hinweis: schreibe nach {out} (ohne --write-curated bleibt fixtures_ibd.h unangetastet)")

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(emit(rows, scan), encoding="utf-8")
    print(f"{len(rows)} Fixtures -> {out}")
    flags = {r["flags"] for r in rows}
    print("Flag-Varianten: " + ", ".join(f"0x{f:04X}" for f in sorted(flags)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
