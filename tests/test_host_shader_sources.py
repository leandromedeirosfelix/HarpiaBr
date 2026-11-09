"""Game-free check: every generated SPIR-V header (`*_comp_spv.h`) has its GLSL source (`*.comp`) beside it, so the
corresponding source of the arrays is always in the tree (GPL-3.0 section 1). When glslang is found (RX_GLSLANG or PATH),
the sources of gpu/vk/host_shaders are also compiled and compared word for word with the headers.

Run with: python -I tests/test_host_shader_sources.py
"""
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DIRS = [ROOT / "gpu" / "vk" / "shaders", ROOT / "gpu" / "vk" / "host_shaders"]


def words(text):
    return re.findall(r"0x[0-9a-fA-F]{8}", text)


class HostShaderSources(unittest.TestCase):
    def test_every_header_has_a_source(self):
        missing = []
        for d in DIRS:
            for h in sorted(d.glob("*_comp_spv.h")):
                src = d / (h.name[:-len("_comp_spv.h")] + ".comp")
                if not src.is_file():
                    missing.append(str(h.relative_to(ROOT)))
        self.assertEqual(missing, [], "SPIR-V headers without their .comp source")

    def test_host_shader_headers_match_sources(self):
        tool = os.environ.get("RX_GLSLANG") or shutil.which("glslang") or shutil.which("glslangValidator")
        if not tool or not os.path.isfile(tool):
            self.skipTest("no glslang (RX_GLSLANG or PATH)")
        d = ROOT / "gpu" / "vk" / "host_shaders"
        for src in sorted(d.glob("*.comp")):
            name = src.stem
            with tempfile.TemporaryDirectory() as tmp:
                out = Path(tmp) / "out.h"
                subprocess.run([tool, "-V", "--target-env", "vulkan1.1", "--vn", name.upper() + "_COMP_SPV", str(src), "-o", str(out)],
                               check=True, capture_output=True)
                self.assertEqual(words(out.read_text()), words((d / (name + "_comp_spv.h")).read_text()), name)


if __name__ == "__main__":
    unittest.main()
