"""The Insert menu's footer (gpu/vk/vk_menu.cpp) holds every row's description, in either language, at any UI scale, with
no game or GPU: the menu's own strings, font and wrap (GDI's DrawTextW) measured here, against the footer's line count
measure_foot works out (the longest description's lines at scale 1, a little narrower than drawn, at least 3).
Windows only (GDI). Prints the longest description of each language: the rows worth a windowed screenshot.

python -I -B tests/test_menu_footer.py [--cxx clang++]
"""
import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCALES = (0.5, 0.6, 0.75, 0.9, 1.0, 1.1, 1.25, 1.5, 1.75, 2.0)


def block(src, head):
    at = src.index(head)
    return src[at:src.index("\n}\n", at) + 3]


def fixture(src):
    strings = src[src.index("#define MENU_STRINGS(X)"):src.index("const char* const strings[S_N][2]")]
    table = src[src.index("const char* const strings[S_N][2]"):]
    table = table[:table.index("\n};\n") + 4]
    consts = "\n".join(re.findall(r"^constexpr int (?:kW|kDescTop) .*$", src, re.M))
    rows = src[src.index("Row rows[] = {"):src.index("constexpr int kRows")]
    descs = sorted(set(re.findall(r"\{\"\w+\", [^,]+, TAB_\w+, S_\w+, (S_\w+),", rows)))
    assert len(descs) > 10, "the rows table's descriptions were not found"
    ids = ", ".join(descs + ["S_C_HINT", "S_C_HINT_LISTEN"])
    names = ", ".join('"%s"' % d for d in descs + ["S_C_HINT", "S_C_HINT_LISTEN"])
    return f"""
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <string>
{consts}
{strings}
{table}
{block(src, "HFONT font(int px, int weight)")}
{block(src, "std::wstring wide(const char* s)")}
static const int ids[] = {{{ids}}};
static const char* const names[] = {{{names}}};
int lines(HDC dc, int font_px, int width, const char* s) {{
    HFONT f = font(font_px, FW_NORMAL);
    HGDIOBJ old = SelectObject(dc, f);
    TEXTMETRICW tm{{}};
    GetTextMetricsW(dc, &tm);
    const std::wstring w = wide(s);
    RECT r{{0, 0, width, 0}};
    DrawTextW(dc, w.c_str(), (int)w.size(), &r, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
    SelectObject(dc, old), DeleteObject(f);
    const int line = std::max(1, (int)tm.tmHeight);
    return ((int)r.bottom + line - 1) / line;
}}
int main() {{
    HDC dc = CreateCompatibleDC(nullptr);
    const int kDescW = kW - 72;
    int foot = 3, longest[2] = {{0, 0}}, which[2] = {{0, 0}}, shortest[2] = {{99, 99}}, least[2] = {{0, 0}};
    for (int i = 0; i < (int)(sizeof ids / sizeof ids[0]); i++)
        for (int l = 0; l < 2; l++) {{
            const char* s = strings[ids[i]][l] ? strings[ids[i]][l] : strings[ids[i]][0];
            const int n = lines(dc, 19, kDescW - 16, s);   // (measure_foot's measure)
            foot = std::max(foot, n);
            if (n > longest[l]) longest[l] = n, which[l] = i;
            if (n < shortest[l]) shortest[l] = n, least[l] = i;
        }}
    std::printf("footer: %d lines\\n", foot);
    for (int l = 0; l < 2; l++)
        std::printf("%s longest: %s (%d lines); shortest: %s (%d)\\n", l ? "pt-BR" : "en-US", names[which[l]], longest[l],
                    names[least[l]], shortest[l]);
    int bad = 0;
    for (float ui : {{{", ".join("%.2ff" % s for s in SCALES)}}})   // (paint_menu: th(19) font, px(kDescW) wide)
        for (int i = 0; i < (int)(sizeof ids / sizeof ids[0]); i++)
            for (int l = 0; l < 2; l++) {{
                const char* s = strings[ids[i]][l] ? strings[ids[i]][l] : strings[ids[i]][0];
                const int n = lines(dc, std::max(1, (int)(19 * ui + 0.5f)), (int)(kDescW * ui + 0.5f), s);
                if (n > foot) std::printf("CUT at %.2fx: %s (%s) needs %d lines\\n", ui, names[i], l ? "pt-BR" : "en-US", n), bad++;
            }}
    DeleteDC(dc);
    return bad ? 1 : 0;
}}
"""


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--cxx", default="clang++")
    a = p.parse_args()
    if sys.platform != "win32":
        print("skipped: GDI is Windows only")
        return 0
    src = (ROOT / "gpu" / "vk" / "vk_menu.cpp").read_text(encoding="utf-8")
    with tempfile.TemporaryDirectory(prefix="menu-footer-") as tmp:
        cpp, exe = Path(tmp) / "fixture.cpp", Path(tmp) / "fixture.exe"
        cpp.write_text(fixture(src), encoding="utf-8")
        subprocess.run([a.cxx, "-std=c++20", "-O1", "-finput-charset=UTF-8", str(cpp), "-o", str(exe), "-lgdi32", "-luser32"],
                       check=True)
        rc = subprocess.run([str(exe)]).returncode
    print("menu footer: ok" if rc == 0 else "menu footer: a description is cut")
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
