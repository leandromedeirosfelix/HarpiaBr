"""Game-free checks of nx2/build.py's translator stamp (HOOKSHOT_SHADER_STAMP): a comment edit or the Harpia rename keeps
the shader caches, a code edit does not.

Run with: python -I tests/test_translator_stamp.py
"""
import ast
import hashlib
import os
from pathlib import Path
import re
import tempfile
import unittest

BUILD = Path(__file__).resolve().parents[1] / "nx2" / "build.py"


def load_stamp_functions():
    """translator_text/translator_stamp and their patterns out of build.py, without its imports (kit, toolchain)."""
    tree = ast.parse(BUILD.read_text(encoding="utf-8"))
    wanted = {"_C_TOKEN", "_BRAND", "translator_text", "translator_stamp", "state_stamp"}
    body = [n for n in tree.body if (isinstance(n, ast.FunctionDef) and n.name in wanted)
            or (isinstance(n, ast.Assign) and any(getattr(t, "id", None) in wanted for t in n.targets))]
    names = {"hashlib": hashlib, "os": os, "re": re}
    exec(compile(ast.Module(body=body, type_ignores=[]), str(BUILD), "exec"), names)
    return names["translator_text"], names["translator_stamp"], names["state_stamp"]


translator_text, translator_stamp, state_stamp = load_stamp_functions()

SHADER = b"""// Hookshot: the shader translator
#include "hookshot_names.h"
static const char* path = "hookshot_programs.bin"; // where the programs go
int f(int a) {
    /* the block comment
       over two lines */
    return a + 1;   // (one more)
}
"""

HARPIA = b"""// Harpia: o tradutor de shaders, comentario novo
// mais uma linha de comentario

#include "harpia_names.h"
static const char* path = "harpia_programs.bin";
int f(int a) {\r
\treturn a + 1; /* inline */\r
}
"""


def tree(root, files):
    for rel, data in files.items():
        p = Path(root, rel)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)
    return root


def vk_tree(root, shader):
    return tree(root, {
        "eden/shader_recompiler/a.cpp": shader,
        "sirit/b.h": b"#pragma once // x\n",
        "compat/c.h": b"int c;\n",
        "vk_shader.cpp": shader,
        "vk_state.h": b"struct S { int hookshot_x; };\n",
        "vk_api.h": b"// api\n",
    })


class TranslatorStamp(unittest.TestCase):
    def test_comments_and_rename_keep_the_stamp(self):
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b:
            vk_tree(a, SHADER)
            vk_tree(b, HARPIA)
            Path(b, "vk_state.h").write_bytes(b"struct S { int harpia_x; }; // renamed\n")
            self.assertEqual(translator_stamp(a), translator_stamp(b))

    def test_code_edit_changes_the_stamp(self):
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b:
            vk_tree(a, SHADER)
            vk_tree(b, SHADER.replace(b"a + 1", b"a + 2"))
            self.assertNotEqual(translator_stamp(a), translator_stamp(b))

    def test_comment_markers_inside_strings_are_code(self):
        self.assertNotEqual(translator_text(b'const char* s = "a//b";\n'), translator_text(b'const char* s = "a";\n'))
        self.assertIn(b'"/* kept */"', translator_text(b'x("/* kept */");\n'))

    def test_new_file_changes_the_stamp(self):
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b:
            vk_tree(a, SHADER)
            vk_tree(b, SHADER)
            tree(b, {"eden/extra.cpp": b"int extra;\n"})
            self.assertNotEqual(translator_stamp(a), translator_stamp(b))

    def test_state_stamp_follows_the_state_headers_only(self):
        def gpu(root, shader=SHADER, files=None):
            vk_tree(Path(root, "vk"), shader)
            tree(root, {"render/rx_nvn.h": b"typedef struct { int x; } nvn_st;\n", "render/nvn_api.h": b"typedef int NVNbufferRange;\n"})
            tree(root, files or {})
            return str(Path(root, "vk"))
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b, tempfile.TemporaryDirectory() as c, \
                tempfile.TemporaryDirectory() as d:
            base = state_stamp(gpu(a))
            # (a translator edit and a comment or rename in the state headers: the notes stay good)
            self.assertEqual(base, state_stamp(gpu(b, SHADER.replace(b"a + 1", b"a + 2"), {"vk/vk_api.h": b"// api, renamed for Harpia\n"})))
            self.assertNotEqual(base, state_stamp(gpu(c, files={"vk/vk_state.h": b"struct S { int hookshot_x; int y; };\n"})))
            self.assertNotEqual(base, state_stamp(gpu(d, files={"render/nvn_api.h": b"typedef long long NVNbufferRange;\n"})))

    def test_stamp_is_16_hex_digits(self):
        with tempfile.TemporaryDirectory() as a:
            self.assertRegex(translator_stamp(vk_tree(a, SHADER)), r"^[0-9a-f]{16}$")


if __name__ == "__main__":
    unittest.main()
