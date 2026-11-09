"""scripts/make_setup_parts.py keeps NVIDIA's DLLs out of a release and Streamline's headers in it: the player's build needs
third_party/streamline/include/sl*.h to compile DLSS in (a filter on the "sl." prefix once dropped sl.h too).

python -I -B tests/test_setup_parts.py
"""
import importlib.util
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    spec = importlib.util.spec_from_file_location("make_setup_parts", ROOT / "scripts" / "make_setup_parts.py")
    parts = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(parts)
    with tempfile.TemporaryDirectory() as tmp:
        keep = ["tools/third_party/streamline/include/sl.h", "tools/third_party/streamline/include/sl_dlss.h",
                "tools/third_party/streamline/license.txt", "tools/nx2/make_game.py"]
        drop = ["tools/sl.interposer.dll", "tools/x/nvngx_dlss.dll", "tools/x/NvLowLatencyVk.dll", "tools/x/sl.dlss_g.dll",
                "tools/nx2/helper.ps1"]
        for rel in keep + drop:
            p = Path(tmp, *rel.split("/"))
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text("x")
        got = set(parts.files_under(tmp, "tools"))
    missing = [k for k in keep if k not in got]
    leaked = [d for d in drop if d in got]
    for k in missing:
        print("MISSING from the package: " + k)
    for d in leaked:
        print("LEAKED into the package: " + d)
    print("setup parts: ok" if not (missing or leaked) else "setup parts: FAILED")
    return 1 if missing or leaked else 0


if __name__ == "__main__":
    sys.exit(main())
