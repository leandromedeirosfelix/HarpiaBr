"""The Insert menu's upscale label stays inside its three names, and RX_VK_UPSCALE stays inside the row.

python -I -B tests/test_upscale_label.py [--cxx g++]
"""
import argparse
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def block(text, marker):
    start = text.index(marker)
    end = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--cxx", default="g++")
    a = p.parse_args()
    source = (ROOT / "gpu" / "vk" / "vk_menu.cpp").read_text(encoding="utf-8")
    # (local: the tabbed menu's upscaler has five names, FSR 1 and NIS after upstream's three, and clamps v into them;
    # RX_VK_UPSCALE is stored as given and then snapped into every row's range by menu_init's snap loop)
    fn = block(source, "const char* tx_upscale(int v) {")
    if "std::clamp(v, 0, 4)" not in fn:
        raise SystemExit("tx_upscale reads t[v] for every v")
    init = source[source.index('getenv("RX_VK_UPSCALE")'):]
    if "snap(r, *r.value)" not in init[:init.index("\n    legacy_sync();")]:   # (its own line: not inside a comment)
        raise SystemExit("RX_VK_UPSCALE is stored without the row's range")
    fixture = r'''#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL:%d %s\n", __LINE__, #x); std::exit(1); } } while (0)
''' + fn + r'''
int main() {
    CHECK(std::strcmp(tx_upscale(0), "Linear") == 0);
    CHECK(std::strcmp(tx_upscale(1), "Catmull-Rom") == 0);
    CHECK(std::strcmp(tx_upscale(2), "Lanczos") == 0);
    CHECK(std::strcmp(tx_upscale(3), "AMD FSR 1") == 0);
    CHECK(std::strcmp(tx_upscale(4), "NVIDIA NIS") == 0);
    CHECK(std::strcmp(tx_upscale(-1), "Linear") == 0);
    CHECK(std::strcmp(tx_upscale(99), "NVIDIA NIS") == 0);
    std::printf("PASS: upscale labels for 0..4 and for a setting past the row\n");
}
'''
    with tempfile.TemporaryDirectory(prefix="upscale-label-") as tmp:
        cpp = Path(tmp) / "fixture.cpp"
        exe = Path(tmp) / "fixture"
        cpp.write_text(fixture, encoding="utf-8")
        subprocess.run([a.cxx, "-std=c++20", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()
