// (hookshot) gpu/vk/dlss_math.h: the scale each DLSS mode asks for, and the reprojection matrix (tests/test_dlss_math.py).
#include "dlss_math.h"
#include <cmath>
#include <cstdio>

static int fail(int n) {
    std::fprintf(stderr, "dlss math check %d failed\n", n);
    return n;
}

int main() {
    using mkwvk::dlss_scale_percent;
    // (100% is the network's output: Quality 0.667 -> 67, Balanced 0.58 -> 58, Performance 0.5 -> 50, Ultra 0.333 -> 33)
    if (dlss_scale_percent(3, 1152) != 67) return fail(1);
    if (dlss_scale_percent(4, 1152) != 58) return fail(2);
    if (dlss_scale_percent(5, 1152) != 50) return fail(3);
    if (dlss_scale_percent(6, 1152) != 33) return fail(4);
    if (dlss_scale_percent(2, 1152) != 133) return fail(5);   // (Native AA: above 1, the full-size network)
    if (dlss_scale_percent(1, 1152) != 67 || dlss_scale_percent(1, 2160) != 50 || dlss_scale_percent(1, 4320) != 33) return fail(6);
    if (dlss_scale_percent(99, 1152) != 33 || dlss_scale_percent(-3, 1152) != 100) return fail(7);
    // RX_VK_DLSS_BASE=0.75 (the old assumption): Quality 89, Performance 67, Native AA still 133; a base of 0 leaves 100%
    if (dlss_scale_percent(3, 1440, 0.75f) != 89 || dlss_scale_percent(2, 1440, 0.75f) != 133 ||
        dlss_scale_percent(5, 1440, 0.75f) != 67 || dlss_scale_percent(3, 1440, 0.0f) != 100)
        return fail(13);

    // the camera moved: VP is a shift by -2 in x now and was -1 before, so clip x 0 now (world x 2) was at clip x +1
    const float cur[16] = {1, 0, 0, -2, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const float prev[16] = {1, 0, 0, -1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float m[16];
    if (!mkwvk::clip_to_prev(cur, prev, m)) return fail(8);
    const float want[16] = {1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    for (int i = 0; i < 16; i++)
        if (std::fabs(m[i] - want[i]) > 1e-6f) return fail(9);
    if (!mkwvk::clip_to_prev(prev, prev, m)) return fail(10);   // (no motion: identity)
    for (int i = 0; i < 16; i++)
        if (std::fabs(m[i] - (i % 5 == 0 ? 1.0f : 0.0f)) > 1e-6f) return fail(11);
    const float zero[16] = {};
    if (mkwvk::clip_to_prev(zero, prev, m)) return fail(12);   // (singular: refused)
    return 0;
}
