// (hookshot) DLSS's arithmetic without Vulkan (vk_dlss.cpp; tests/dlss_math_test.cpp): the scale a mode asks for, and the
// matrix that takes this frame's clip position to the last frame's.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace mkwvk {

/// The render scale (percent of the upscale network's output: the scale the menu sets applies to it) a DLSS mode asks for.
/// mode: Options::dlss (0 Off, 1 Auto, 2 DLAA, 3 Quality, 4 Balanced, 5 Performance, 6 Ultra Performance). out_h: the
/// network's output height (Auto: Quality up to 1440, Performance up to 2160, Ultra Performance above).
inline int dlss_resolved_mode(int mode, uint32_t out_h) {
    if (mode == 1) mode = out_h <= 1440 ? 3 : out_h <= 2160 ? 5 : 6;
    return std::clamp(mode, 0, 6);
}
/// base: the network's input over its output at 100%. 1.0: once nx2/oot_patches.py makes the fixed scale apply, the game
/// draws that fraction of the output (Harpia 2026-11-09: 89% drew 0.889, 67% 0.68, 50% 0.50; a base of 0.75 asked Quality
/// for 89% and got 0.89, not 0.667). Native AA asks for 133% whatever the base: above 1.0 the game takes its full-size
/// network and clamps the scene to the output (f_831f3200's 1 / k cap lifted above 1.0; gate_h_dlaa drew input = output).
inline int dlss_scale_percent(int mode, uint32_t out_h, float base = 1.0f) {
    static constexpr float ratio[] = {1.0f, 1.0f, 1.0f, 0.667f, 0.58f, 0.5f, 0.333f};   // (Off: 100%)
    const int m = dlss_resolved_mode(mode, out_h);
    if (m == 0 || !(base > 0.0f)) return 100;
    if (m == 2) return 133;   // (Native AA: the full-size network, the output's size)
    return std::min(134, (int)std::lround(ratio[m] / base * 100.0f));   // (above 1 the game clamps anyway)
}

/// previous * inverse(current), both row major as the game keeps them, in double (float made distant surfaces shimmer in
/// TotK's port). After Lichii's ClipToPreviousClip. false: current is singular or the result is not finite.
inline bool clip_to_prev(const float cur[16], const float prev[16], float out[16]) {
    double m[4][8]{};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) m[i][j] = cur[i * 4 + j];
        m[i][i + 4] = 1.0;
    }
    for (int c = 0; c < 4; c++) {
        int pivot = c;
        for (int r = c + 1; r < 4; r++)
            if (std::fabs(m[r][c]) > std::fabs(m[pivot][c])) pivot = r;
        std::swap(m[c], m[pivot]);
        const double d = m[c][c];
        if (!std::isfinite(d) || std::fabs(d) < 1e-10) return false;
        for (double& v : m[c]) v /= d;
        for (int r = 0; r < 4; r++)
            if (r != c) {
                const double f = m[r][c];
                for (int k = 0; k < 8; k++) m[r][k] -= f * m[c][k];
            }
    }
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            double v = 0;
            for (int k = 0; k < 4; k++) v += prev[r * 4 + k] * m[k][c + 4];
            if (!std::isfinite(v)) return false;
            out[r * 4 + c] = (float)v;
        }
    return true;
}

} // namespace mkwvk
