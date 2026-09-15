#!/usr/bin/env python3
"""Build docs/HANDBUCH.html from the canon markdown list, or --check links only.

No network. Requires: pip install 'markdown>=3.5'

Linkcheck: .md-Ziele und Existenz werden hart geprüft (Exit 1). Fragment-Anker
(#…) sind nur weich — Renderer sluggen unterschiedlich; ein veralteter Anker
macht die CI deshalb nicht rot.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Explicit editorial order — do not glob.
DOC_LIST = [
    "STATE.md",
    "CHANGELOG.md",
    "README.md",
    "docs/ergometer/ENTWICKLERDOKU.md",
    "docs/ergometer/PFLICHTENHEFT.md",
    "docs/ergometer/GERAETEPROFIL.md",
    "docs/ergometer/KALIBRIERUNG.md",
    "docs/ergometer/NACHTESTS.md",
    "docs/ergometer/BRIDGE.md",
    "docs/ergometer/WEBINTERFACE.md",
    "docs/ergometer/BEDIENUNG.md",
    "docs/ergometer/BLE-SCAN.md",
    "debug/RELEASE_v0.1.0.md",
    "debug/UPDATE_CAPS_FIX.md",
    "debug/HW_TEST1_60RPM.md",
    "debug/HW_TEST2_80RPM.md",
    "debug/HW_NACHTEST_20260913.md",
    "debug/UPDATE_NACHTEST_RESULTS.md",
    "debug/PLATFORMIO_FIX.md",
]

OUT = ROOT / "docs" / "HANDBUCH.html"

CSS = """
:root {
  --bg:#101419; --fg:#E6EAF2; --dim:#8A94A6;
  --accent:#E2802F; --ok:#4CAF63; --bad:#C9304A; --edge:#232A34;
}
* { box-sizing: border-box; }
html { scroll-behavior: smooth; }
body {
  margin: 0; font-family: system-ui, sans-serif; background: var(--bg); color: var(--fg);
  line-height: 1.55;
}
a { color: var(--accent); }
nav {
  position: fixed; top: 0; left: 0; bottom: 0; width: 280px; overflow: auto;
  background: #0c0f13; border-right: 1px solid var(--edge); padding: 1rem 0.75rem;
  font-size: 0.85rem;
}
nav h1 { font-size: 0.95rem; margin: 0 0 0.75rem; color: var(--accent); }
nav a { display: block; color: var(--dim); text-decoration: none; padding: 0.15rem 0.35rem; }
nav a:hover { color: var(--fg); }
nav .h2 { padding-left: 0.85rem; font-size: 0.8rem; }
main { margin-left: 280px; padding: 1.5rem 2rem 4rem; max-width: 52rem; }
.doc { margin-bottom: 3rem; padding-bottom: 2rem; border-bottom: 1px solid var(--edge); }
.doc-title { color: var(--accent); font-size: 1.35rem; }
.meta { color: var(--dim); font-size: 0.85rem; margin-bottom: 1.25rem; }
table { border-collapse: collapse; width: 100%; font-variant-numeric: tabular-nums; margin: 1rem 0; }
th, td { border: 1px solid var(--edge); padding: 0.35rem 0.5rem; text-align: left; vertical-align: top; }
th { background: #161a21; }
code, pre { font-family: ui-monospace, monospace; font-size: 0.88em; }
pre {
  background: #0c0f13; border: 1px solid var(--edge); padding: 0.75rem 1rem;
  overflow-wrap: anywhere; white-space: pre-wrap;
}
blockquote { border-left: 3px solid var(--accent); margin: 1rem 0; padding: 0.25rem 0.75rem; color: var(--dim); }
img { max-width: 100%; height: auto; border: 1px solid var(--edge); border-radius: 8px; margin: 0.75rem 0; }
@media print {
  nav { display: none; }
  main { margin: 0; max-width: none; }
}
"""


def slug_doc(rel: str) -> str:
    name = Path(rel).stem.lower()
    name = re.sub(r"[^a-z0-9]+", "-", name).strip("-")
    return f"doc-{name}"


def heading_id(text: str) -> str:
    s = text.lower().strip()
    s = re.sub(r"[^\w\s\-äöüÄÖÜß]", "", s, flags=re.UNICODE)
    s = re.sub(r"\s+", "-", s)
    return s


def fw_version() -> str:
    ini = (ROOT / "platformio.ini").read_text(encoding="utf-8", errors="replace")
    m = re.search(r'-DFW_VERSION=\\"([^\\"]+)\\"', ini)
    return m.group(1) if m else "?"


def git_short() -> str:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, text=True
        ).strip()
    except Exception:
        return "unknown"


def normalize_md_target(src: Path, href: str) -> Path | None:
    """Resolve a .md link from src to a path under ROOT, or None if not .md."""
    if href.startswith(("http://", "https://", "mailto:")):
        return None
    path_part = href.split("#", 1)[0]
    if not path_part or not path_part.endswith(".md"):
        return None
    # strip accidental query
    path_part = path_part.split("?", 1)[0]
    resolved = (src.parent / path_part).resolve()
    try:
        resolved.relative_to(ROOT.resolve())
    except ValueError:
        return resolved  # outside repo — still check exists
    return resolved


def collect_anchors(md: str) -> set[str]:
    ids: set[str] = set()
    for line in md.splitlines():
        m = re.match(r"^(#{1,6})\s+(.+)$", line)
        if m:
            ids.add(heading_id(m.group(2)))
    # explicit {#id}
    for m in re.finditer(r"\{#([A-Za-z0-9\-]+)\}", md):
        ids.add(m.group(1))
    return ids


def check_links() -> list[str]:
    errors: list[str] = []
    listed = { (ROOT / p).resolve() for p in DOC_LIST }
    listed_rel = { p.replace("\\", "/") for p in DOC_LIST }

    # Rule 4: every .md in repo (except archiv) should be listed or under debug/archiv
    for md in ROOT.rglob("*.md"):
        rel = md.relative_to(ROOT).as_posix()
        if rel.startswith("debug/archiv/"):
            continue
        if "/.pio/" in f"/{rel}/" or rel.startswith(".pio/"):
            continue
        if md.resolve() in listed:
            continue
        # allow debug/README, TODO, HW_TEST2_LIGHT, FIXTURE, captures/calib READMEs, TODO
        allowed_extra = {
            "debug/README.md",
            "debug/TODO_DOKUMENTATION.md",
            "debug/TODO_NACHFASS_20260914.md",
            "debug/NOTIZ_ENTWURF_20260914.md",
            "debug/NOTIZ_NACHFASS_20260914.md",
            "debug/HW_TEST2_LIGHT.md",
            "debug/UPDATE_FIXTURE_6A.md",
            "debug/captures/README.md",
            "debug/calib/README.md",
            "docs/ergometer/README.md",
            "tools/ref/README.md",
            "LICENSE.md",
        }
        if rel in allowed_extra:
            continue
        errors.append(f"unlisted markdown: {rel}")

    # Per-file link checks
    for rel in DOC_LIST:
        src = ROOT / rel
        if not src.is_file():
            errors.append(f"missing listed file: {rel}")
            continue
        text = src.read_text(encoding="utf-8", errors="replace")
        anchors = collect_anchors(text)
        for m in re.finditer(r"\[([^\]]*)\]\(([^)]+)\)", text):
            href = m.group(2).strip()
            if href.startswith("#"):
                aid = href[1:]
                # local anchor — soft: many renderers slug differently; only warn missing exact
                if aid and aid not in anchors and not aid.startswith("doc-"):
                    # don't hard-fail noisy heading slug mismatches; skip
                    pass
                continue
            target = normalize_md_target(src, href)
            if target is None:
                continue
            if not target.is_file():
                errors.append(f"{rel}: broken link → {href}")
                continue
            try:
                trel = target.relative_to(ROOT).as_posix()
            except ValueError:
                errors.append(f"{rel}: link outside repo → {href}")
                continue
            if trel.startswith("debug/archiv/"):
                continue
            if trel not in listed_rel and trel not in {
                "debug/README.md",
                "debug/TODO_DOKUMENTATION.md",
                "debug/HW_TEST2_LIGHT.md",
                "debug/UPDATE_FIXTURE_6A.md",
                "debug/captures/README.md",
                "debug/calib/README.md",
                "docs/ergometer/README.md",
                "docs/ergometer/kalibrierung-map-20260913.json",
                "docs/ergometer/kalibrierung-map-20260915.json",
            }:
                # json is fine; for md require list or allowed
                if trel.endswith(".md"):
                    errors.append(f"{rel}: .md link not in handbook list → {trel}")
            frag = href.split("#", 1)
            if len(frag) == 2 and frag[1]:
                ttext = target.read_text(encoding="utf-8", errors="replace")
                if frag[1] not in collect_anchors(ttext) and not frag[1].startswith("L"):
                    # soft: don't fail on slug style
                    pass
    return errors


def render_html() -> str:
    try:
        import markdown as mdlib
    except ImportError:
        print(
            "Fehlt: pip install 'markdown>=3.5'\n"
            "Ohne das Paket kann tools/docs_html.py nicht rendern.",
            file=sys.stderr,
        )
        sys.exit(2)

    extensions = ["tables", "fenced_code", "toc", "attr_list", "sane_lists", "admonition"]
    parts: list[str] = []
    nav: list[str] = ['<nav><h1>esp32.ergo</h1>']

    for rel in DOC_LIST:
        src = ROOT / rel
        raw = src.read_text(encoding="utf-8", errors="replace")
        doc_id = slug_doc(rel)

        def repl_link(m: re.Match[str]) -> str:
            text, href = m.group(1), m.group(2)
            if href.startswith(("http://", "https://", "mailto:")):
                return m.group(0)
            path_part, _, frag = href.partition("#")
            if not path_part.endswith(".md") and path_part:
                return m.group(0)
            if not path_part:
                # local fragment → keep, prefixed with current doc
                return f'<a href="#{doc_id}--{frag}">{text}</a>' if frag else m.group(0)
            target = normalize_md_target(src, path_part)
            if target is None or not target.is_file():
                return m.group(0)
            try:
                trel = target.relative_to(ROOT).as_posix()
            except ValueError:
                return m.group(0)
            tid = slug_doc(trel)
            if frag:
                return f'<a href="#{tid}--{frag}">{text}</a>'
            return f'<a href="#{tid}">{text}</a>'

        def rewrite_img(m: re.Match[str]) -> str:
            alt, href = m.group(1), m.group(2).strip()
            if href.startswith(("http://", "https://", "mailto:", "data:")):
                return m.group(0)
            path_part = href.split("#", 1)[0].split("?", 1)[0]
            if not path_part:
                return m.group(0)
            resolved = (src.parent / path_part).resolve()
            try:
                out_rel = resolved.relative_to(OUT.parent.resolve()).as_posix()
            except ValueError:
                return m.group(0)
            return f"![{alt}]({out_rel})"

        with_imgs = re.sub(r"!\[([^\]]*)\]\(([^)]+)\)", rewrite_img, raw)
        linked = re.sub(r"\[([^\]]*)\]\(([^)]+)\)", repl_link, with_imgs)
        # rewrite ## headings to include doc prefix in id via attr_list after render is hard;
        # inject HTML markers instead
        html_body = mdlib.markdown(linked, extensions=extensions)
        # prefix h2 ids roughly
        def prefix_h(m: re.Match[str]) -> str:
            level, rest = m.group(1), m.group(2)
            # extract text
            text = re.sub(r"<[^>]+>", "", rest)
            hid = heading_id(text)
            return f'<h{level} id="{doc_id}--{hid}">{rest}</h{level}>'

        html_body = re.sub(r"<h([2-4])>(.*?)</h\1>", prefix_h, html_body, flags=re.S)

        title = Path(rel).name
        parts.append(
            f'<section class="doc" id="{doc_id}">'
            f'<h1 class="doc-title">{title}</h1>'
            f'<p class="meta"><code>{rel}</code></p>{html_body}</section>'
        )
        nav.append(f'<a href="#{doc_id}"><strong>{title}</strong></a>')
        for m in re.finditer(r"^##\s+(.+)$", raw, re.M):
            nav.append(
                f'<a class="h2" href="#{doc_id}--{heading_id(m.group(1))}">{m.group(1)}</a>'
            )

    nav.append("</nav>")
    from datetime import datetime, timezone

    meta = (
        f"Erzeugt {datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M UTC')} · "
        f"git {git_short()} · FW {fw_version()}. "
        f"Generiert aus Markdown — Änderungen in die Quellen."
    )
    return (
        "<!DOCTYPE html><html lang=\"de\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        f"<title>esp32.ergo Handbuch</title><style>{CSS}</style></head><body>"
        + "".join(nav)
        + f"<main><p class=\"meta\">{meta}</p>"
        + "".join(parts)
        + "</main></body></html>"
    )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true", help="nur Linkprüfung")
    args = ap.parse_args()

    errs = check_links()
    if errs:
        print(f"{len(errs)} Doku-Linkfehler:", file=sys.stderr)
        for e in errs:
            print(f"  - {e}", file=sys.stderr)
        if args.check:
            return 1

    if args.check:
        print(f"OK — {len(DOC_LIST)} Dokumente, Links grün")
        return 0

    if errs:
        return 1

    html = render_html()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(html, encoding="utf-8")
    print(f"Wrote {OUT.relative_to(ROOT)} ({len(html)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
