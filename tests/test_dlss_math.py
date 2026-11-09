"""DLSS's mode-to-scale table and camera reprojection matrix (gpu/vk/dlss_math.h, DLSS-GATE.md)."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class DlssMathTests(unittest.TestCase):
    def test_scales_and_reprojection(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("no C++ compiler")
        with tempfile.TemporaryDirectory(prefix="harpia-dlss-math-") as tmp:
            binary = Path(tmp) / "dlss_math_test"
            compile = subprocess.run(
                [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                 "-I", str(ROOT / "gpu" / "vk"),
                 str(ROOT / "tests" / "dlss_math_test.cpp"), "-o", str(binary)],
                cwd=ROOT, text=True, capture_output=True)
            self.assertEqual(compile.returncode, 0, compile.stderr)
            run = subprocess.run([str(binary)], cwd=ROOT, text=True, capture_output=True)
            self.assertEqual(run.returncode, 0, run.stderr)


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=1))
