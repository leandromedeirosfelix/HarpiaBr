"""nx2/oot_patches.py applied twice changes nothing the second time: the shared work folder's lifted C is patched again
after a source change (a lift-time patch run by hand), and a patch that matched its own output added itself again (the text
speed's scale applied twice, a squared speed).

python -I -B nx2/test_oot_patches_idempotent.py
"""
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oot_patches as op  # noqa: E402

HEAD = '#include "a64.h"\n'
FILES = {
    "code_001.c": HEAD + op.TEXT_HEAD + "\n    goto L_80b1e1b4;\n" + op.TEXT_OLD + "}\n",
    "code_002.c": HEAD + "void f_x(Ctx *c) {\n    " + op.OLD + "\n" + op.LABEL + "\n    " + op.SCALE_OLD + "\n}\n",
    "code_003.c": HEAD + "void f_y(Ctx *c) {\n" + op.ROUND_OLD + "    " + op.CLAMP_OLD + "\n}\n",
}


def read(path):
    with open(path, encoding="utf-8", newline="") as f:
        return f.read()


class Twice(unittest.TestCase):
    def test_a_second_apply_changes_nothing(self):
        with tempfile.TemporaryDirectory() as gen:
            for name, text in FILES.items():
                with open(os.path.join(gen, name), "w", encoding="utf-8", newline="") as f:
                    f.write(text)
            op.apply(gen)
            first = {n: read(os.path.join(gen, n)) for n in FILES}
            for n in FILES:
                self.assertNotEqual(first[n], FILES[n], n + ": the first apply changed nothing")
            op.apply(gen)
            for n in FILES:
                self.assertEqual(read(os.path.join(gen, n)), first[n],
                                 n + ": a second apply changed it again")
            self.assertEqual(first["code_001.c"].count("hos_text_scale != 1.0f"), 1)


if __name__ == "__main__":
    unittest.main()
