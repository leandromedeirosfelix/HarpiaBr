"""The Insert menu's rules around DLSS SR and DLSS Frame Generation (gpu/vk/vk_menu.cpp), no game or GPU required.

- No DLLs (nothing loaded): exactly as before. The regular frame generation and the player's render scale stay, and
  a preset sets frame generation.
- DLSS-G loaded: a preset leaves frame generation off, and the scale shown is DLSS's.
- The restart badges follow what was loaded at the start, not the ini's value then.

python -I -B tests/test_menu_dlss.py [--cxx g++]
"""
import argparse
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def block(src, head):
    at = src.index(head)
    return src[at:src.index("\n}\n", at) + 3]


FIXTURE = r"""
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <string_view>
struct { int render_scale = 0, sharpness = 0, tex_quality = 0, framegen = 0, upscale = 0, dlss = 0, dlssg = 0; } opts;
bool sr_loaded = false, g_loaded = false;
bool dlss_active() { return sr_loaded && opts.dlss; }
bool dlssg_on() { return g_loaded; }
uint32_t dlss_out_h = 1440;
float dlss_base() { return 0.75f; }
int dlss_scale_percent(int mode, uint32_t, float) { return mode == 2 ? 133 : mode == 5 ? 67 : 100; }
enum { S_AUTO };
const char* T(int) { return "Auto"; }
char num[16];
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); std::exit(1); } } while (0)
// PRODUCTION
int main() {
    // (no DLLs: the player's scale, and a preset's frame generation even with dlss_frame_generation= in the file)
    opts.dlssg = 4; opts.render_scale = 85;
    CHECK(menu_dlss_scale() == -1);
    CHECK(std::string_view(tx_render_scale(85)) == "85%");
    apply_preset(3);
    CHECK(opts.framegen == 2 && opts.render_scale == 100);
    // (DLSS SR loaded, Native AA: its scale shown)
    sr_loaded = true; opts.dlss = 2;
    CHECK(menu_dlss_scale() == 133);
    CHECK(std::string_view(tx_render_scale(100)) == "133% (DLSS)");
    // (DLSS-G alone loaded: 100%, and a preset leaves the regular frame generation off)
    sr_loaded = false; opts.dlss = 0; g_loaded = true; opts.framegen = 0;
    CHECK(menu_dlss_scale() == 100);
    opts.render_scale = 85;   // (a fixed percent stays with DLSS-G; Auto is 100%)
    CHECK(menu_dlss_scale() == 85 && std::string_view(tx_render_scale(85)) == "85%");
    opts.render_scale = 0;
    CHECK(menu_dlss_scale() == 100 && std::string_view(tx_render_scale(0)) == "100% (DLSS)");
    apply_preset(5);
    CHECK(opts.framegen == 0);
    opts.dlssg = 0; apply_preset(5);   // (DLSS-G switched off in the menu: the preset's again)
    CHECK(opts.framegen == 3 && menu_dlss_scale() == -1);
    std::puts("menu DLSS rules: ok");
}
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="g++")
    args = parser.parse_args()
    menu = (ROOT / "gpu" / "vk" / "vk_menu.cpp").read_text(encoding="utf-8")
    init = block(menu, "void menu_init() {")
    if any("dlssg" in line and "opts.framegen = 0" in line for line in init.splitlines()):
        raise SystemExit("menu_init turns the regular frame generation off by itself: no DLLs must be exactly as before")
    for need, why in (("if (r.value == &opts.dlssg && opts.dlssg) opts.framegen = 0;", "DLSS-G set by hand turns the other off"),
                      ("if (r.value == &opts.framegen && opts.framegen) opts.dlssg = 0;", "and the other way"),
                      ("[](int v) { return v != 0 && !dlss_active(); }", "DLSS's restart badge: what was loaded"),
                      ("[](int v) { return v >= 2 && !dlssg_on(); }", "DLSS-G's restart badge: what was loaded"),
                      ("{S_SCALE, tx_render_scale(opts.render_scale)}", "the Info tab shows DLSS's scale"),
                      ("dlssg_row_limit();", "DLSS-G's 3x and 4x offered on an RTX 50 only"),
                      ("S_SCALE_D, tx_render_scale,", "the render scale row shows DLSS's scale")):
        if need not in menu:
            raise SystemExit("vk_menu.cpp: %s is missing (%s)" % (need, why))
    replay = (ROOT / "gpu" / "vk" / "vk_replay.cpp").read_text(encoding="utf-8")
    option = replay[replay.index("    t.option = [](int id) -> int {"):]
    if "menu_dlss_scale()" not in option[:option.index("\n    };")]:
        raise SystemExit("vk_replay.cpp: t.option does not give the host the scale the menu shows (menu_dlss_scale)")
    tx_scale = menu[menu.index("const char* tx_scale(int v)"):]
    production = (tx_scale[:tx_scale.index("\n") + 1] + block(menu, "int menu_dlss_scale() {") +
                  block(menu, "const char* tx_render_scale(int v) {") + block(menu, "void apply_preset(int p) {"))
    with tempfile.TemporaryDirectory(prefix="menu-dlss-") as tmp:
        cpp, exe = Path(tmp) / "fixture.cpp", Path(tmp) / "fixture.exe"
        cpp.write_text(FIXTURE.replace("// PRODUCTION", production), encoding="utf-8")
        subprocess.run([args.cxx, "-std=c++20", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()
