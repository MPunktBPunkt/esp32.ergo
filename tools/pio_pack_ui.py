Import("env")  # type: ignore  # noqa: F821 — PlatformIO

import subprocess
import sys
from pathlib import Path

root = Path(env["PROJECT_DIR"])  # type: ignore  # noqa: F821
script = root / "tools" / "pack_ui.py"
rc = subprocess.call([sys.executable, str(script)])
if rc != 0:
    sys.exit(rc)
