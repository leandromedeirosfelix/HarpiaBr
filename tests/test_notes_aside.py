"""The player's pipeline notes (hookshot_pipelines.bin, gpu/vk/vk_draw.cpp) are never emptied or deleted by a build of
another layout: a stale file is set aside under its stamp (hookshot_pipelines.bin.<stamp>), this build's own set-aside copy
is put back, and a file that cannot be set aside is not written. nx2/make_game.py names them the same way.

python -I -B tests/test_notes_aside.py
"""
import sys
import tempfile
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]


def body(text, head):
    start = text.index(head)
    return text[start:text.index("\n}\n", start)]


def main():
    draw = (ROOT / "gpu" / "vk" / "vk_draw.cpp").read_text(encoding="utf-8")
    prep = body(draw, "int notes_prepare() {")
    if "MOVEFILE_REPLACE_EXISTING" not in prep or "return -1;" not in prep or "remove(" in prep:
        raise SystemExit("notes_prepare: a stale file is not moved aside in one checked step")
    if "notes_aside_path(kNoteStamp)" not in prep:
        raise SystemExit("notes_prepare: this build's own set-aside notes are not put back")
    note = body(draw, "void pipeline_note(")
    if note.index("notes_prepare()") > note.index('same ? "ab" : "wb"') or "if (prepared < 0)" not in note:
        raise SystemExit("pipeline_note: the file can be opened \"wb\" without being set aside first")
    if "notes_prepare();" not in body(draw, "void precompile_run("):
        raise SystemExit("precompile_run does not set stale notes aside or put this build's back")
    for fn in ("pipeline_note(", "precompile_run("):
        if 'remove((pipe_notes_path() + ".bak")' in body(draw, "void " + fn) or 'remove((path + ".bak")' in body(draw, "void " + fn):
            raise SystemExit(fn + ": a .bak is removed before the new one is in place")
    game = (ROOT / "nx2" / "make_game.py").read_text(encoding="utf-8")
    if "def notes_aside(" not in game or 'harpia_pipelines.bin.%016x' not in game:
        raise SystemExit("make_game.py does not set the player's notes aside, under the runtime's naming, before a seed replaces them")
    seed_step()
    print("PASS: the notes are set aside by stamp, never emptied")


def seed_step():
    """make_game.seed_pipelines over a player's notes of an older layout: set aside under their stamp, then the seed"""
    sys.path.insert(0, str(ROOT / "nx2"))
    import make_game
    mine, old = (0x1111).to_bytes(8, "little"), (0x2222).to_bytes(8, "little")
    size = (24).to_bytes(8, "little")
    with tempfile.TemporaryDirectory() as tmp:
        root, out = Path(tmp, "src"), Path(tmp, "game")
        root.mkdir(), out.mkdir()
        seed = mine + size + b"k" * 24
        player = old + size + b"p" * 48
        (root / "harpia_pipelines_seed.bin").write_bytes(seed)
        (out / "harpia_pipelines.bin").write_bytes(player)
        with mock.patch.object(make_game, "ROOT", str(root)), mock.patch.object(make_game, "build_note_stamp", lambda: mine):
            make_game.seed_pipelines(str(out))
        aside = out / ("harpia_pipelines.bin.%016x" % 0x2222)
        if not aside.is_file() or aside.read_bytes() != player:
            raise SystemExit("seed_pipelines: the player's older notes were not set aside under their stamp")
        if (out / "harpia_pipelines.bin").read_bytes() != seed:
            raise SystemExit("seed_pipelines: the seed did not take the stale file's place")


if __name__ == "__main__":
    main()
