"""The fixed render scale's patch (nx2/oot_patches.py): mode 0 when the scale changed, on fresh and earlier-patched code."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oot_patches  # noqa: E402

FRESH = ('#include "a64.h"\n'
         "    " + oot_patches.OLD + "\n"
         "L_8082640c:;\n"
         "    c->x[8] = 0x100000000ULL;\n"
         "    " + oot_patches.SCALE_OLD + "\n"
         "    c->x[9] &= 0xFFFFFFFFULL;\n")


class DynresPatchTests(unittest.TestCase):
    def test_fresh_code_gets_the_switch_and_mode_0(self):
        out = oot_patches.patch_dynres(FRESH)
        self.assertIn(oot_patches.NEW, out)
        self.assertIn(oot_patches.SCALE_NEW + "\n    c->x[9] &= 0xFFFFFFFFULL;", out)
        self.assertIn("extern volatile uint32_t hos_fixed_scale_bits;", out)

    def test_earlier_patched_code_gains_mode_0_once(self):
        earlier = FRESH.replace(oot_patches.OLD, oot_patches.NEW).replace(oot_patches.SCALE_OLD, oot_patches.SCALE_SET)
        out = oot_patches.patch_dynres(earlier)
        self.assertEqual(out.count("oot_seen_scale_bits = (uint32_t)c->x[9]"), 1)
        self.assertEqual(out.count(oot_patches.SEEN_DECL), 1)
        self.assertIn(oot_patches.NEW, out)   # (the dynamic path sets the seen scale back)
        self.assertIsNone(oot_patches.patch_dynres(out))   # (applied twice: no change)

    def test_view_scale_step_and_cap_on_the_fixed_path(self):
        code = ('#include "a64.h"\n' + oot_patches.ROUND_OLD + "    " + oot_patches.CLAMP_OLD + "\n")
        out = oot_patches.patch_view_scale(code)
        # (local, review: the rounding stays at the default scale, 1.0)
        self.assertIn("(hos_fixed_resolution && hos_fixed_scale_bits != 0x3f800000u) ? 1 : recomp_load8(c,c->x[19] + 427)", out)
        self.assertIn("hos_fixed_scale_bits > 0x3f800000u", out)
        self.assertIn("extern volatile int hos_fixed_resolution;", out)
        self.assertEqual(oot_patches.patch_view_scale(code + oot_patches.CLAMP_OLD), code + oot_patches.CLAMP_OLD)   # (twice: left)
        self.assertIsNone(oot_patches.patch_view_scale(out))   # (applied: nothing left to do)

    def test_a_fixed_scale_after_the_dynamic_path_applies_again(self):
        out = oot_patches.patch_dynres(FRESH)
        dyn = out.index("hos_dynres_runs++;")
        self.assertIn("oot_seen_scale_bits = 0x3f800000u;", out[dyn:dyn + 200])   # (the dynamic branch resets it)
        self.assertIn(oot_patches.SEEN_DECL, out)   # (file scope, not a static in the function)
        self.assertNotIn("static uint32_t _seen", out)

    def test_code_patched_by_the_first_versions_is_brought_up_once(self):
        v1 = FRESH.replace(oot_patches.OLD, oot_patches.NEW_V1).replace(oot_patches.SCALE_OLD, oot_patches.SCALE_NEW_V1)
        out = oot_patches.patch_dynres(v1)
        self.assertIn(oot_patches.NEW, out)
        self.assertIn(oot_patches.SCALE_NEW, out)
        self.assertNotIn("static uint32_t _seen", out)
        self.assertEqual(out.count(oot_patches.SEEN_DECL), 1)
        self.assertIsNone(oot_patches.patch_dynres(out))
        view_v1 = '#include "a64.h"\n' + oot_patches.ROUND_NEW_V1 + "    " + oot_patches.CLAMP_NEW + "\n"
        up = oot_patches.patch_view_scale(view_v1)
        self.assertIn(oot_patches.ROUND_NEW, up)
        self.assertIsNone(oot_patches.patch_view_scale(up))

    def test_other_code_is_left_alone(self):
        self.assertIsNone(oot_patches.patch_dynres('#include "a64.h"\nvoid f_1(void) {}\n'))


if __name__ == "__main__":
    unittest.main()
