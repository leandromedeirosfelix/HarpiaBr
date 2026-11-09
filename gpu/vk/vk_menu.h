// The F1 / Insert menu (vk_menu.cpp, tabbed): options that apply while the game runs, kept in harpia_options.ini beside the executable.
#pragma once

#include "vk_internal.h"

namespace mkwvk {

// (out of the box every feature is off: what the game does on its own. The ini beside the executable keeps a player's choices)
struct Options {
    int framegen = 0;     // made frames: 0 off, or 2 up to 8 times the game's frames
    int pace = 0;         // frames shown on an even line (vk_framegen.cpp)
    int resolution = 0;   // (legacy ini key "resolution": 0 Auto, 1 100%, 2 75%, 3 67%, 4 50%; fills render_scale once, then kept to it for replay, not shown)
    int render_scale = 0; // the 3D's render scale: 0 Auto (the game's dynamic resolution), else a percent of its full scale, one of
                          // 100 85 75 67 58 50 44 33 (nx2/oot_patches.py, host/nn_vi.c; ini key "render_scale")
    int upscale = 0;      // to the window: 0 linear, 1 Catmull-Rom, 2 Lanczos, 3 AMD FSR 1 (EASU), 4 NVIDIA Image Scaling (NIS)
    int dlss = 0;         // NVIDIA DLSS in place of OoT's upscale: 0 off, 1 Auto, 2 DLAA, 3 Quality, 4 Balanced, 5 Performance,
                          // 6 Ultra Performance; sets the render scale (vk_dlss.cpp); on or off read at the start (ini key "dlss")
    int dlssg = 0;        // NVIDIA DLSS Frame Generation: 0 off, 2, 3 or 4 frames shown per game frame (RTX 40: 2 only), with
                          // Reflex; never with framegen; on or off read at the start (vk_dlss.cpp; ini key "dlss_frame_generation")
    int sharpness = 0;    // 0 to 100: RCAS after FSR 1, NIS's own sharpening with NIS, the contrast-limited sharpen otherwise
    int aspect = 0;       // 0 Stretch the 16:9 picture to the window (as before), 1 Fit: 16:9 with black bars, 2 Fill: the camera at the
                          // monitor's shape, any ratio, read once at the start (host/hos_main.c; ini key "aspect")
    int anisotropy = 0;   // texture filtering: 0 the game's own, else 2, 4, 8 or 16 (mipmapped samplers that do not compare)
    int vsync = 0;        // 0 Off without tearing (mailbox, as before), 1 On (FIFO), 2 Off with tearing (immediate, lowest latency)
    int realtime = 1;     // game time follows the clock (host/nn_vi.c); 0: per frame (slower frames, slower game)
    int handheld = 0;     // the game's handheld profile: ~4 fps, softer; read once, at the start (host/nn_applet.c)
    int ultrawide = 0;    // (legacy ini key "ultrawide": 1 is aspect 2; fills aspect once, then kept to it and saved for the host, not shown)
    int show_fps = 0;
    int fullscreen = 0;   // borderless, at the monitor's size: F11 is the same setting
    int classic_fog = 1;  // 1 Classic: the OoT visual workaround's thinner fog (RX_VK_OOT_VISUAL=1, as this fork had it), 0 the game's
                          // own fog (EvilOOT's default upstream); read at the start, as RX_VK_OOT_VISUAL (vk_shader.cpp; ini key "classic_fog")
    int tex_quality = 0;  // 0 Normal, 1 High: huge textures keep their top levels, ASTC is re-encoded as BC3 only from 8 MB, and the
                          // texture budget is 9 GB instead of 6 (vk_texture.cpp); 2 Ultra: High, and no ASTC is re-encoded as BC3;
                          // read at the start (ini key "texture_quality")
    int output_res = 0;   // 0 the game's (2048x1152 docked), 1 2560x1440: graphics scale level 0, docked only; read by the host at
                          // the start (host/nn_applet.c; ini key "output_res")
    int gpu_device = 0;   // graphics card index: 0 Auto, 1..N device 0..N-1 (gpu/vk/vk_main.cpp; RX_VK_DEVICE)
    int pad_background = 0;   // the controllers count while another window has the focus (host/nn_hid.c; HOS_INPUT_BACKGROUND)
    int mouse_native = 0; // the mouse turns the camera's own angles (host/mouse_native.c); 0: as the right stick
    int mouse_sens = 100; // the mouse's sensitivity in percent: 0.1 degrees a count at 100 with native look (host/nn_hid.c)
    int mouse_invert = 0; // the mouse's up and down swapped
    int text_speed = 0;   // how fast dialogue prints: 0 the game's, 1 2x, 2 4x, 3 instant (host/nn_vi.c, nx2/oot_patches.py; HOS_TEXT_SPEED)
    int invert_cam_x = 0; // right-stick camera horizontal inversion: 0 normal, 1 inverted
    int invert_cam_y = 0; // right-stick camera vertical inversion: 0 normal, 1 inverted
    int pad_layout = 0;   // 0 Nintendo (default), 1 Xbox / PC, 2 Swap Face Buttons, 3 Custom
    int robust = 0;       // bounds-checked buffer access: 0 Auto (on), 1 Off, 2 On (gpu/vk/vk_main.cpp; RX_VK_ROBUST)
    int preset = 0;       // quality preset: 0 Custom, 1 Minimum (4GB), 2 Low (6GB), 3 Balanced (8GB), 4 High (12GB), 5 Ultra (16GB+)
};
extern Options opts;

void menu_init();             // (the options from the file, once)
void menu_set_gpu_count(int count);
void menu_tick();             // (each frame shown: the keys, while the game's window has the focus)
bool menu_is_open();
int menu_dlss_scale();   // (hookshot: the percent DLSS SR or DLSS-G fixes while loaded, the host's options 2 and 3; -1: the player's)
void menu_option_set(int id, int value);   // (from the game's side: 7 the mouse's sensitivity, the - and = keys)
void menu_note_frame(bool game_frame);   // (the frame rates the FPS counter shows)
/// Over the window's image (TRANSFER_DST_OPTIMAL), w x h: the menu and the FPS counter. slot: the frame in flight, 0 to 2.
void menu_record(VkCommandBuffer cmd, VkImage dst, int slot, uint32_t w, uint32_t h);

} // namespace mkwvk
