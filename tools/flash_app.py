#!/usr/bin/env python3
"""Flash the application via JLink (sector erase only — preserves MCUboot)."""

import subprocess
import sys
from pathlib import Path

project_dir = Path(__file__).resolve().parent.parent
workspace_dir = project_dir.parent
build_dir = project_dir / "build"

ret = subprocess.run([
    "west", "flash", "--runner", "jlink",
    "--build-dir", str(build_dir),
], cwd=str(workspace_dir))

if ret.returncode != 0:
    print("App flash FAILED.", file=sys.stderr)
    sys.exit(ret.returncode)

print("App flashed successfully.")
