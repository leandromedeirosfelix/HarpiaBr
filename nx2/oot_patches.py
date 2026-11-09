"""Changes to the lifted C for Ocarina of Time, applied by lift.py after it writes the code (and by hand: python -I nx2/oot_patches.py).

Each is a runtime switch where it can be, so the game's own path stays reachable.

- Dynamic resolution (Lichii's built-in "Max Resolution" patch, main+0x805BD0): the engine's config flag at +0x94 picks the path that
  follows the GPU-time controller; with hos_fixed_resolution set it takes the other path, the 3D at full scale. The controller made
  a new set of render targets at each size, ~11 images a frame at times, and held the GPU near its budget.
- Text speed (the Insert menu's "Text speed"): f_80b1e140 (main+0xafe140) steps a text pane's letter animation, the
  LetterAnimControl of the panes with "LetterAnimOn" user data (dialogue, Navi, signs). Its time argument (s0) moves the
  letters' clock (+0x60 += rate +0x4c * time, the rate from the message's speed tag) and the pauses' timer (+0x58 -= time);
  hos_text_scale multiplies it, 1.0 as shipped. The auto-advance timer (+0x5c, before) keeps the game's time. A huge
  scale shows a page at once (one frame per pause), as the game's own skip on A (+0x72 = 2, f_80794b00) without its flags.
"""
import glob
import os
import sys

# (local, review: the fixed scale last applied, in the file and not the function: the dynamic path sets it back to 1.0, so a
# fixed scale after a dynamic stretch applies again; [3676] holds the controller's value then. 1.0 alone never forces mode 0)
SEEN_DECL = "static uint32_t oot_seen_scale_bits = 0x3f800000u;   /* (nx2/oot_patches.py: the fixed scale last applied) */\n"
DECL = ("extern volatile int hos_fixed_resolution;\nextern volatile int hos_dynres_runs;\nextern volatile uint32_t hos_fixed_scale_bits;\n"
        + SEEN_DECL)
OLD = "if (!((c->x[8] >> 0) & 1)) { goto L_8082640c; }"
NEW_V1 = "if (hos_fixed_resolution || !((c->x[8] >> 0) & 1)) { goto L_8082640c; }\n    hos_dynres_runs++;"
NEW = NEW_V1 + "\n    oot_seen_scale_bits = 0x3f800000u;   /* (nx2/oot_patches.py: the dynamic path; a fixed scale applies anew) */"
LABEL = "L_8082640c:;"
SCALE_OLD = "c->x[9] = 0x3f800000ULL;"
SCALE_SET = "c->x[9] = hos_fixed_scale_bits;"   # (the fixed path's scale, 1.0 as shipped)
# (the caller, f_831f3200, applies the returned scale only for mode 0 (x0's high word); the fixed path returned mode 1 or 2,
# so a scale set in game was never used, at any level. Mode 0 once each time the scale changes; 1.0 as shipped: unchanged)
SCALE_NEW_V1 = (SCALE_SET + "\n    { static uint32_t _seen = 0x3f800000u; if ((uint32_t)c->x[9] != _seen) "
                "{ _seen = (uint32_t)c->x[9]; c->x[8] = 0; } }   /* (nx2/oot_patches.py: mode 0 when the scale changed) */")
SCALE_NEW = (SCALE_SET + "\n    if ((uint32_t)c->x[9] != oot_seen_scale_bits) { oot_seen_scale_bits = (uint32_t)c->x[9]; c->x[8] = 0; }"
             "   /* (nx2/oot_patches.py: mode 0 when the scale changed) */")

# (f_831f3200, the per-view render scale from the fixed path's [3676]: the 3D height is rounded down to 108 lines unless byte
# +427 is set (1440 lines became 1404), and the scale is capped at 1 / k unless [3616] bit 19 (the Native AA's 1.3333 became
# 1.0). On the fixed path: no rounding; the cap lifted only for a scale above 1.0, where DLSS's Native AA asks for it)
ROUND_DECL = "extern volatile int hos_fixed_resolution;\nextern volatile uint32_t hos_fixed_scale_bits;\n"
ROUND_OLD = ("    { uint64_t _r = (c->z) ? c->x[9] : c->x[8]; c->x[12] = _r; }\n"
             "    c->x[8]=recomp_load8(c,c->x[19] + 427);\n")
ROUND_NEW_V1 = ("    { uint64_t _r = (c->z) ? c->x[9] : c->x[8]; c->x[12] = _r; }\n"
                "    c->x[8]=hos_fixed_resolution ? 1 : recomp_load8(c,c->x[19] + 427);   /* (nx2/oot_patches.py: no 108-line step) */\n")
# (local, review: at the default scale, 1.0, the rounding stays exactly as the game has it)
ROUND_NEW = ("    { uint64_t _r = (c->z) ? c->x[9] : c->x[8]; c->x[12] = _r; }\n"
             "    c->x[8]=(hos_fixed_resolution && hos_fixed_scale_bits != 0x3f800000u) ? 1 : recomp_load8(c,c->x[19] + 427);"
             "   /* (nx2/oot_patches.py: no 108-line step but at 1.0) */\n")
CLAMP_OLD = "if (((c->x[9] >> 19) & 1)) { goto L_831f3ff0; }"
CLAMP_NEW = ("if ((hos_fixed_resolution && hos_fixed_scale_bits > 0x3f800000u) || ((c->x[9] >> 19) & 1)) { goto L_831f3ff0; }"
             "   /* (nx2/oot_patches.py: no 1/k cap above 1.0 on the fixed path) */")


def patch_view_scale(s):
    """The two edits in f_831f3200; None when the file has neither anchor, s unchanged when they are not each there once."""
    if ROUND_OLD not in s and CLAMP_OLD not in s:
        if ROUND_NEW_V1 in s:   # (code an earlier version patched: the rounding kept at 1.0, once)
            return s.replace(ROUND_NEW_V1, ROUND_NEW, 1)
        return None
    if s.count(ROUND_OLD) != 1 or s.count(CLAMP_OLD) != 1:
        return s
    s = s.replace(ROUND_OLD, ROUND_NEW, 1).replace(CLAMP_OLD, CLAMP_NEW, 1)
    return s.replace('#include "a64.h"\n', '#include "a64.h"\n' + ROUND_DECL, 1)


TEXT_DECL = "extern volatile float hos_text_scale;\n"
TEXT_HEAD = "void f_80b1e140(Ctx *__restrict A64_NOESC c) {"
# (the letters' block: past the early returns, the auto-advance timer and the hidden / no-text checks; reached once a call)
TEXT_OLD = ("L_80b1e1b4:;\n"
            "    c->x[9]=recomp_load16(c,c->x[19] + 110);\n"
            "if ((uint32_t)c->x[9] == 0) { goto L_80b1e18c; }\n")
TEXT_NEW = TEXT_OLD + ("    if (hos_text_scale != 1.0f) { float _t; uint32_t _u; memcpy(&_t,c->vreg[0],4); _t *= hos_text_scale; "
                       "memcpy(&_u,&_t,4); a64_vset64(c->vreg[0],_u); }   /* (nx2/oot_patches.py: text speed) */\n")


def patch_dynres(s):
    if OLD not in s:   # (code an earlier version patched: brought to this one, once)
        out = s
        if NEW_V1 in out and NEW not in out:
            out = out.replace(NEW_V1, NEW, 1)
        if SCALE_NEW_V1 in out:
            out = out.replace(SCALE_NEW_V1, SCALE_NEW, 1)
        elif SCALE_SET + "\n" in out and SCALE_NEW not in out:
            out = out.replace(SCALE_SET + "\n", SCALE_NEW + "\n", 1)
        if out != s and SEEN_DECL not in out:
            out = out.replace('#include "a64.h"\n', '#include "a64.h"\n' + SEEN_DECL, 1)
        return out if out != s else None
    s = s.replace(OLD, NEW, 1)
    at = s.index(LABEL)
    scale = s.index(SCALE_OLD, at)
    s = s[:scale] + SCALE_NEW + s[scale + len(SCALE_OLD):]
    return s.replace('#include "a64.h"\n', '#include "a64.h"\n' + DECL, 1)


def patch_text_speed(s):
    """The scale at the head of the letters' block of f_80b1e140; None when the file has no such function, s unchanged when the
    function is there but its code differs (another game version)."""
    head = s.find(TEXT_HEAD)
    if head < 0:
        return None
    end = s.find("\nvoid f_", head + len(TEXT_HEAD))
    body = s[head:end if end >= 0 else len(s)]
    # (local, review: TEXT_NEW begins with TEXT_OLD, so an applied patch still matched and each apply added one more scale:
    # a second run over a patched gen squared the text speed)
    if TEXT_NEW in body or body.count(TEXT_OLD) != 1 or body.count("goto L_80b1e1b4;") != 1:
        return s
    at = head + body.index(TEXT_OLD)
    s = s[:at] + TEXT_NEW + s[at + len(TEXT_OLD):]
    return s.replace('#include "a64.h"\n', '#include "a64.h"\n' + TEXT_DECL, 1)


def apply(gen):
    done = {"dynamic resolution branch": 0, "text speed block": 0, "view scale step and cap": 0}
    for path in sorted(glob.glob(os.path.join(gen, "code_*.c"))):
        with open(path, encoding="utf-8", newline="") as f:
            s = f.read()
        out = s
        d = patch_dynres(out)
        if d is not None:
            out = d
            done["dynamic resolution branch"] += 1
            print("oot_patches: dynamic resolution switch in", os.path.basename(path))
        v = patch_view_scale(out)
        if v is not None and v != out:
            out = v
            done["view scale step and cap"] += 1
            print("oot_patches: view scale step and cap in", os.path.basename(path))
        t = patch_text_speed(out)
        if t is not None and t != out:
            out = t
            done["text speed block"] += 1
            print("oot_patches: text speed scale in", os.path.basename(path))
        if out != s:
            with open(path, "w", encoding="utf-8", newline="") as f:
                f.write(out)
        # (local, review: a patch already there is found too: an apply over patched code warned of none)
        for what, marker in (("dynamic resolution branch", NEW), ("view scale step and cap", ROUND_NEW), ("text speed block", TEXT_NEW)):
            if marker in s and marker in out:   # (there before this apply: found, not made)
                done[what] += 1
    for what, n in done.items():
        if n != 1:
            print("oot_patches: WARNING: the %s was found %d times" % (what, n))


if __name__ == "__main__":
    apply(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "work", "sa", "gen"))
