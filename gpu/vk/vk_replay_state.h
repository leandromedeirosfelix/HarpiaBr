// The replay's state: what a pipeline note keeps (NvnState, vk_state.h) and what only the replay keeps beside it.
// (local, review) vk_state.h's text is HARPIA_STATE_STAMP and sizeof(NvnState) the notes' record size (nx2/build.py
// state_stamp, vk_draw.cpp PipeNote): a counter added there made every player's harpia_pipelines.bin stale on the
// first start of a build, so the replay's own bookkeeping is here instead
#pragma once

#include "vk_state.h"

namespace mkwvk {

struct ReplayState : NvnState {
    // (upstream 11d2c1a) what bind_gen was when a stage's textures, samplers or images (tex_gen) or its storage buffers
    // (ssbo_gen) last changed: the driver banks c2 and c0 of a graphics stage are made from those alone, so a draw after
    // another uniform buffer's bind keeps its banks
    uint64_t tex_gen[kNvnStages] = {}, ssbo_gen[kNvnStages] = {};
};

}  // namespace mkwvk
