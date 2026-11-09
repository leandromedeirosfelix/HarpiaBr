"""The pipeline notes' stamp the build's seed step compares (nx2/make_game.py build_note_stamp) is the one the renderer
writes (gpu/vk/vk_draw.cpp kNoteStamp: "HKNOTES1" ^ HARPIA_STATE_STAMP). They differed by two swapped bytes, and a
player's notes of an older layout were never replaced by the build's own seed.

python -I -B tests/test_note_stamp.py
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "nx2"))
import build  # noqa: E402
import make_game  # noqa: E402


def main():
    draw = (ROOT / "gpu" / "vk" / "vk_draw.cpp").read_text(encoding="utf-8")
    m = re.search(r"constexpr uint64_t kNoteStamp = 0x([0-9A-Fa-f']+)ull \^ HARPIA_STATE_STAMP;", draw)
    if not m:
        raise SystemExit("vk_draw.cpp: kNoteStamp is not \"<magic> ^ HARPIA_STATE_STAMP\" any more")
    magic = int(m.group(1).replace("'", ""), 16)
    if magic.to_bytes(8, "little") != b"HKNOTES1":
        raise SystemExit("vk_draw.cpp: kNoteStamp's magic is %r, not HKNOTES1" % magic.to_bytes(8, "little"))
    state = int(build.state_stamp(str(ROOT / "gpu" / "vk")), 16)
    want = ((magic ^ state) & (2 ** 64 - 1)).to_bytes(8, "little")
    got = make_game.build_note_stamp()
    if got != want:
        raise SystemExit("make_game.build_note_stamp() is %r, the renderer writes %r" % (got, want))
    print("PASS: the seed step and the renderer agree on the notes' stamp")


if __name__ == "__main__":
    main()
