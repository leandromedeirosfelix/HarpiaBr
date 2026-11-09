"""DLSS Frame Generation's compute shaders (gpu/vk/shaders/dlssg_*.comp): the SPIR-V headers vk_dlss.cpp includes are the
sources as they are now, compiled by glslang (skipped without it: RX_GLSLANG or PATH)."""
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHADERS = ROOT / "gpu" / "vk" / "shaders"


def glslang():
    for c in (os.environ.get("RX_GLSLANG"), shutil.which("glslang"), shutil.which("glslangValidator")):
        if c and os.path.isfile(c):
            return c
    return None


def words(header_text):
    return [int(w, 16) for w in re.findall(r"0x[0-9a-fA-F]{8}", header_text)]


class DlssgShaderTests(unittest.TestCase):
    def test_spirv_headers_match_sources(self):
        tool = glslang()
        if not tool:
            self.skipTest("no glslang")
        sources = sorted(SHADERS.glob("dlssg_*.comp"))
        self.assertTrue(sources, "no dlssg_*.comp")
        for src in sources:
            name = "DLSSG_" + src.stem[len("dlssg_"):].upper() + "_COMP_SPV"
            header = SHADERS / (src.stem + "_comp_spv.h")
            with tempfile.TemporaryDirectory(prefix="harpia-dlssg-") as tmp:
                out = Path(tmp) / "out.h"
                r = subprocess.run([tool, "-V", "--target-env", "vulkan1.1", "--vn", name, "-o", str(out), str(src)],
                                   capture_output=True, text=True)
                self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
                self.assertEqual(words(out.read_text()), words(header.read_text()), f"{header.name} is stale: rebuild it")


if __name__ == "__main__":
    unittest.main()
