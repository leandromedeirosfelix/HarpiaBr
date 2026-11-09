// (hookshot) NVIDIA DLSS through Streamline in place of OoT's upscale network (fd029a317a9a4b94), NVIDIA cards only.
// What the network reads and what DLSS is given instead: DLSS-GATE.md (the T7.1 capture). Off unless the menu's DLSS row is
// on at the start and a signed sl.interposer.dll sits beside the executable; any failure leaves the game's network to run.
#pragma once

#include "vk_internal.h"
#include "dlss_math.h"

namespace mkwvk {

/// Before the instance: Streamline's vkGetInstanceProcAddr when DLSS is wanted, the card is NVIDIA's and the production
/// sl.interposer.dll beside the executable is signed by NVIDIA (slInit done); null otherwise, and vulkan-1.dll is used.
PFN_vkGetInstanceProcAddr dlss_load();
/// sl.interposer.dll beside the executable (the menu shows its DLSS row only then; checked once)
bool dlss_dll_present();
/// Streamline put aside for the run (the Vulkan start through it failed): true if it was loaded; load_global then takes
/// vulkan-1.dll
bool dlss_disable();
/// Streamline loaded and the menu's mode is not Off.
bool dlss_active();
extern std::atomic<uint32_t> dlss_out_h;
extern std::atomic<uint32_t> dlss_scale_bits;   // (the float scale for DLSS's optimal input; 0: the menu's percent)
/// The network's input over its output at 100% render scale (RX_VK_DLSS_BASE, default 0.75: the game's 2048x1152 output)
float dlss_base();
/// The upscale network DLSS replaces (RX_VK_DLSS_KEYS=<hex key>[,...], default fd029a317a9a4b94: the 3/4 network; at 2.5K
/// output the game may use another, its key from the "vk: network" log line; default also 9dc98871eb7fc101, the full-size one)
bool dlss_key(uint64_t key);   // (the network's output height, last seen: 1152 docked until then; Auto's choice)
/// The scene this frame, from its draws (vk_draw.cpp dlss_note_draw): its depth and the camera's two view-projections.
struct DlssScene {
    uint64_t frame = ~0ull;   // (texture_frame_no() of the draws)
    VkImage depth = VK_NULL_HANDLE;   // (the scene's D32, in GENERAL as every texture is)
    VmaAllocation depth_mem = nullptr;
    VkImageUsageFlags depth_usage = 0;
    uint32_t depth_id = 0;            // (its Texture's id: a view made for one image is never used for another at that handle)
    uint32_t width = 0, height = 0;
    float vp[16]{}, prev_vp[16]{};   // (vertex c3 +0x30 and +0x180, row major as the game keeps them)
    float near_z = 0, far_z = 0;     // (c3 +0xe0, +0xe4)
    float aspect = 0, fov = 0;       // (c3 +0xf8; +0x118, radians)
};
/// DLSS in the network's place: color, the network's input view (RGB9E5, square-root encoded), into out (its R11G11B10F
/// output, the storage view the dispatch has). false: nothing usable was recorded and the network should run.
bool dlss_evaluate(VkCommandBuffer cmd, VkImageView color, uint32_t in_w, uint32_t in_h, VkImageView out, uint32_t out_w,
                   uint32_t out_h, const DlssScene& scene);

// ---- (hookshot) DLSS Frame Generation (DLSS-G) with Reflex, the same Streamline: DLSS-GATE.md 9 ----
// The menu's "Frame generation DLSS" (opts.dlssg 2, 3 or 4 times), read at the start like the DLSS row. Made frames are shown
// only: game time and physics follow the game's own presents. Never with the frame interpolation of vk_framegen.cpp.

/// sl.dlss_g.dll (and the interposer) beside the executable and an RTX 40 or 50 card: the menu shows the row only then
bool dlssg_dll_present();
/// DLSS-G loaded at the start and supported by the card and driver (false until the first frame asks)
bool dlssg_on();
/// Reflex paces the game: true only while DLSS-G and Reflex run
bool dlss_reflex_active();
/// The game's thread at its present: the frame handed over (PCL simulation end)
void dlssg_game_present();
/// The game's thread as its next frame starts (its window texture's acquire, after the host's 60 fps pacing, the one limiter,
/// host/nn_vi.c): Reflex's low-latency sleep and PCL simulation start; true when the sleep ran
bool dlssg_frame_start();
/// The replay's thread: a submit of frame `frame` (PCL render submit start, once), its last one (start false: the end)
void dlssg_render_submit(bool start, int64_t frame);
/// After the upscale network's dispatch, or DLSS SR in its place: out (its output, a sampled view) copied as the colour before
/// the HUD, the scene's depth and camera motion tagged for the frame (once), the constants set when DLSS SR did not
void dlssg_scene_done(VkCommandBuffer cmd, VkImageView out, uint32_t out_w, uint32_t out_h, const DlssScene& scene);
/// present_image, before its submit: the colour before the HUD where the picture goes in the window (rect) tagged as DLSS-G's
/// HUD-less colour, and its options for this present (off in the menu, without a scene, while the window changes size)
void dlssg_present(VkCommandBuffer cmd, uint32_t sw, uint32_t sh, const int crop[4], int rx, int ry, uint32_t rw, uint32_t rh,
                   bool resizing);
/// DLSS-G made frames at the last present (a window change waits for a present with it off: NVIDIA's guide 17.0)
bool dlssg_generating();
/// A swapchain made (vk_main.cpp create_swapchain): Reflex's options sent again
void dlssg_swapchain_made();
/// DLSS-G off at once, without a present first (the swapchain out of date: made again now)
void dlssg_force_off();
/// Around vkQueuePresentKHR of frame `frame`: PCL present start (true) and end (false)
void dlssg_present_marker(bool start, int64_t frame);

} // namespace mkwvk
