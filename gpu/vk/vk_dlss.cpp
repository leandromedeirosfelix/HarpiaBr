// (hookshot) NVIDIA DLSS through Streamline in place of OoT's upscale network: vk_dlss.h, DLSS-GATE.md.
// Streamline is loaded only when the menu's DLSS row is on at the start, the card is NVIDIA's and the production
// sl.interposer.dll beside the executable carries NVIDIA's signature; its plugins come from the same folder, nothing is
// downloaded. The NVIDIA DLLs are the player's own copy: never in this repository or a release (NVIDIA's licence).
// Built with the Streamline headers (MIT) when nx2/build.py finds them (HARPIA_STREAMLINE); without them DLSS is never on.
#include "vk_dlss.h"
#include "vk_menu.h"
#include "vk_texture.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace mkwvk {

std::atomic<uint32_t> dlss_out_h{1152};
std::atomic<uint32_t> dlss_scale_bits{0};

float dlss_base() {
    static const float b = getenv("RX_VK_DLSS_BASE") ? (float)atof(getenv("RX_VK_DLSS_BASE")) : 1.0f;   // (dlss_math.h)
    return b;
}

bool dlss_key(uint64_t key) {
    static const std::vector<uint64_t> keys = [] {
        std::vector<uint64_t> v;
        // (the 3/4 network, and the full-size one the game switches to at a scale of 1.33: Native AA's 2560x1440 -> 2560x1440)
        const char* e = getenv("RX_VK_DLSS_KEYS") ? getenv("RX_VK_DLSS_KEYS") : "fd029a317a9a4b94,9dc98871eb7fc101";
        for (const char* q = e; *q;) {
            char* end;
            const uint64_t k = strtoull(q, &end, 16);
            if (end == q) break;
            v.push_back(k);
            q = *end ? end + 1 : end;
        }
        return v;
    }();
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

} // namespace mkwvk

#if defined(_WIN32) && defined(HARPIA_STREAMLINE)
#include <vk_mem_alloc.h>
#include <sl.h>
#include <sl_consts.h>
#include <sl_dlss.h>
#include <sl_security.h>
#include <sl_dlss_g.h>
#include <sl_reflex.h>
#include <sl_pcl.h>
#include <mutex>
#include <map>
#include <string>
#include "shaders/dlss_color_comp_spv.h"
#include "shaders/dlss_motion_comp_spv.h"
#include "shaders/dlss_output_comp_spv.h"
#include "shaders/dlssg_copy_comp_spv.h"
#include "shaders/dlssg_depth_comp_spv.h"
#include "shaders/dlssg_hudless_comp_spv.h"

namespace mkwvk {
extern VmaAllocator vma;   // (vk_vma.cpp)
extern int64_t presents_now;   // (vk_replay.cpp: the presents done)

namespace {
struct Streamline {
    HMODULE mod = nullptr;
    PFun_slInit* init = nullptr;
    PFun_slIsFeatureSupported* supported = nullptr;
    PFun_slGetFeatureFunction* feature = nullptr;
    PFun_slGetNewFrameToken* token = nullptr;
    PFun_slSetConstants* constants = nullptr;
    PFun_slSetTagForFrame* tag = nullptr;
    PFun_slEvaluateFeature* evaluate = nullptr;
    PFun_slDLSSSetOptions* options = nullptr;
    PFun_slDLSSGetOptimalSettings* optimal = nullptr;
    std::atomic<bool> checked{false}, ok{false};   // (DLSS on this device: asked once, at the first frame; t.option reads them)
    std::atomic<int> failures{0};                  // (evaluations that failed in a row: 8 turn it off for the run)
    std::atomic<int> declines{0};                  // (frames skipped in a row, any reason: kDeclines turn it off for the run)
} sl_;
std::wstring sl_dir;   // (the executable's folder: the interposer and its plugins)
bool disabled = false; // (dlss_disable: load_global's second try takes vulkan-1.dll)

void sl_log(sl::LogType type, const char* msg) {
    static std::atomic<int> told{0};   // (a warning Streamline repeats each frame would grow the log without end)
    if (type != sl::LogType::eInfo && told++ < 60) logf("sl: %s", msg);
}

/// An NVIDIA card in the system, from a throwaway instance (Streamline is not loaded on anyone else's)
bool nvidia_card() {
    const HMODULE v = LoadLibraryA("vulkan-1.dll");
    const auto gipa = v ? (PFN_vkGetInstanceProcAddr)(void*)GetProcAddress(v, "vkGetInstanceProcAddr") : nullptr;
    const auto create = gipa ? (PFN_vkCreateInstance)gipa(nullptr, "vkCreateInstance") : nullptr;
    if (!create) return false;
    VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    ai.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &ai;
    VkInstance inst = VK_NULL_HANDLE;
    if (create(&ci, nullptr, &inst) != VK_SUCCESS) return false;
    const auto enumerate = (PFN_vkEnumeratePhysicalDevices)gipa(inst, "vkEnumeratePhysicalDevices");
    const auto props = (PFN_vkGetPhysicalDeviceProperties)gipa(inst, "vkGetPhysicalDeviceProperties");
    const auto destroy = (PFN_vkDestroyInstance)gipa(inst, "vkDestroyInstance");
    bool nv = false;
    uint32_t n = 0;
    if (enumerate && props && enumerate(inst, &n, nullptr) == VK_SUCCESS && n) {
        std::vector<VkPhysicalDevice> pd(n);
        enumerate(inst, &n, pd.data());
        for (VkPhysicalDevice p : pd) {
            VkPhysicalDeviceProperties pp{};
            props(p, &pp);
            nv |= pp.vendorID == 0x10DE;
        }
    }
    if (destroy) destroy(inst, nullptr);
    return nv;
}

PFN_vkGetInstanceProcAddr refuse(const char* why) {
    logf("sl: DLSS not loaded: %s; OoT's own upscale runs", why);
    return nullptr;
}

// ---- the passes around DLSS: colour in, camera motion, the result out (shaders/dlss_*.comp) ----
struct Pass { VkPipeline pipe = VK_NULL_HANDLE; VkPipelineLayout layout = VK_NULL_HANDLE; };
VkSampler point_sampler = VK_NULL_HANDLE;
Pass color_pass, motion_pass, output_pass;

/// A compute pipeline over (binding 0: a sampled image, binding 1: a storage image) as push descriptors
Pass make_pass(const uint32_t* spv, size_t bytes, uint32_t push_bytes) {
    Pass p;
    VkDescriptorSetLayoutBinding b[2] = {{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
                                         {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};
    VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    li.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    li.bindingCount = 2, li.pBindings = b;
    VkDescriptorSetLayout set{};
    if (vkCreateDescriptorSetLayout(dev.device, &li, nullptr, &set) != VK_SUCCESS) return p;
    const VkPushConstantRange pr{VK_SHADER_STAGE_COMPUTE_BIT, 0, push_bytes};
    VkPipelineLayoutCreateInfo pi{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pi.setLayoutCount = 1, pi.pSetLayouts = &set, pi.pushConstantRangeCount = push_bytes ? 1 : 0, pi.pPushConstantRanges = &pr;
    if (vkCreatePipelineLayout(dev.device, &pi, nullptr, &p.layout) != VK_SUCCESS) return p;
    VkShaderModuleCreateInfo mi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    mi.codeSize = bytes, mi.pCode = spv;
    VkShaderModule m{};
    if (vkCreateShaderModule(dev.device, &mi, nullptr, &m) != VK_SUCCESS) return p;
    VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    ci.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, m, "main", nullptr};
    ci.layout = p.layout;
    if (vkCreateComputePipelines(dev.device, VK_NULL_HANDLE, 1, &ci, nullptr, &p.pipe) != VK_SUCCESS) p.pipe = VK_NULL_HANDLE;
    vkDestroyShaderModule(dev.device, m, nullptr);
    return p;
}

void run_pass(VkCommandBuffer cmd, const Pass& p, VkImageView in, VkImageView out, uint32_t w, uint32_t h, const void* push,
              uint32_t push_bytes, VkSampler sampler = VK_NULL_HANDLE) {
    const VkDescriptorImageInfo ii[2] = {{sampler ? sampler : point_sampler, in, VK_IMAGE_LAYOUT_GENERAL},
                                         {VK_NULL_HANDLE, out, VK_IMAGE_LAYOUT_GENERAL}};
    VkWriteDescriptorSet w2[2]{};
    for (int i = 0; i < 2; i++) {
        w2[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w2[i].dstBinding = (uint32_t)i, w2[i].descriptorCount = 1, w2[i].pImageInfo = &ii[i];
        w2[i].descriptorType = i ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p.pipe);
    vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p.layout, 0, 2, w2);
    if (push_bytes) vkCmdPushConstants(cmd, p.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, push_bytes, push);
    vkCmdDispatch(cmd, (w + 7) / 8, (h + 7) / 8, 1);
}

/// An image of ours DLSS reads or writes, GENERAL from the start
struct Image {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation mem = nullptr;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags usage = 0;
    uint32_t w = 0, h = 0;
};
/// Images make_image replaced, destroyed 8 presents later: recorded work, the 3 frames in flight and DLSS-G's present may
/// still use them until then (the replay's thread)
struct Retired { Image im; int64_t at; };
std::vector<Retired> retired;
void free_retired() {
    for (size_t i = 0; i < retired.size();) {
        if (presents_now - retired[i].at <= 8) { i++; continue; }
        vkDestroyImageView(dev.device, retired[i].im.view, nullptr);
        destroy_image(retired[i].im.image, retired[i].im.mem);
        retired[i] = retired.back(), retired.pop_back();
    }
}
bool make_image(VkCommandBuffer cmd, Image& im, VkFormat f, uint32_t w, uint32_t h) {
    free_retired();
    if (im.image) retired.push_back({im, presents_now});
    im = {};
    VkImageCreateInfo ic{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ic.imageType = VK_IMAGE_TYPE_2D, ic.format = f, ic.extent = {w, h, 1}, ic.mipLevels = 1, ic.arrayLayers = 1;
    ic.samples = VK_SAMPLE_COUNT_1_BIT, ic.tiling = VK_IMAGE_TILING_OPTIMAL;
    ic.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
               VK_IMAGE_USAGE_TRANSFER_DST_BIT;   // (Streamline may copy what it is given)
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation mem = nullptr;
    if (!create_image(ic, image, mem)) return false;
    VkImageViewCreateInfo vc{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vc.image = image, vc.viewType = VK_IMAGE_VIEW_TYPE_2D, vc.format = f;
    vc.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(dev.device, &vc, nullptr, &view) != VK_SUCCESS) {   // (nothing recorded uses it yet)
        destroy_image(image, mem);
        return false;
    }
    im.image = image, im.mem = mem, im.view = view;
    VkImageMemoryBarrier2 ib{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};   // (UNDEFINED -> GENERAL, once)
    ib.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    ib.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    ib.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, ib.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    ib.srcQueueFamilyIndex = ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ib.image = im.image, ib.subresourceRange = vc.subresourceRange;
    VkDependencyInfo di{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    di.imageMemoryBarrierCount = 1, di.pImageMemoryBarriers = &ib;
    vkCmdPipelineBarrier2(cmd, &di);
    im.format = f, im.usage = ic.usage, im.w = w, im.h = h;
    return true;
}

VkDeviceMemory memory_of(VmaAllocation a) {
    if (!a) return VK_NULL_HANDLE;
    VmaAllocationInfo info{};
    vmaGetAllocationInfo(vma, a, &info);
    return info.deviceMemory;
}

sl::Resource resource_of(VkImage image, VmaAllocation mem, VkImageView view, VkFormat f, VkImageUsageFlags usage, uint32_t w,
                         uint32_t h) {
    sl::Resource r(sl::ResourceType::eTex2d, (void*)image, (void*)memory_of(mem), (void*)view, VK_IMAGE_LAYOUT_GENERAL);
    r.width = w, r.height = h, r.nativeFormat = (uint32_t)f, r.mipLevels = 1, r.arrayLayers = 1, r.flags = 0, r.usage = usage;
    return r;
}

/// The scene's depth seen as a texture (one view per depth image, kept: a new one when the scene's targets are made again)
VkImageView depth_view(const DlssScene& s) {
    // shortcut: views of depth images the game has since destroyed stay (one per scene target remade); drop old ones if
    // that ever grows
    static std::map<std::pair<VkImage, uint32_t>, VkImageView> views;
    const std::pair<VkImage, uint32_t> key{s.depth, s.depth_id};
    if (auto it = views.find(key); it != views.end()) return it->second;
    VkImageViewCreateInfo vc{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vc.image = s.depth, vc.viewType = VK_IMAGE_VIEW_TYPE_2D, vc.format = VK_FORMAT_D32_SFLOAT;
    vc.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    VkImageView v = VK_NULL_HANDLE;
    if (vkCreateImageView(dev.device, &vc, nullptr, &v) != VK_SUCCESS) return VK_NULL_HANDLE;
    return views[key] = v;
}

sl::DLSSMode sl_mode(int mode, uint32_t out_h) {
    switch (dlss_resolved_mode(mode, out_h)) {
    // (DLAA under a 0.75 network, the engine's 1.0 cap: Quality, whose range takes 0.5 to 1.0 of the output. Ultra Quality
    // is not offered by this DLSS: the A/B's dlaa1 run, "getOptimalSettings returns invalid size (0 x 0)", NGX 0xbad00010)
    case 2: return sl::DLSSMode::eMaxQuality;
    case 3: return sl::DLSSMode::eMaxQuality;
    case 4: return sl::DLSSMode::eBalanced;
    case 5: return sl::DLSSMode::eMaxPerformance;
    default: return sl::DLSSMode::eUltraPerformance;
    }
}

sl::float4x4 to_sl(const float* m, bool transpose) {
    sl::float4x4 r;
    for (uint32_t i = 0; i < 4; i++)
        r.setRow(i, transpose ? sl::float4(m[i], m[4 + i], m[8 + i], m[12 + i]) : sl::float4(m[i * 4], m[i * 4 + 1], m[i * 4 + 2], m[i * 4 + 3]));
    return r;
}

constexpr int kDeclines = 120;
bool fail(const char* why) {
    static int told;
    // (the game draws at DLSS's scale while DLSS is on: declined that long, the player's scale comes back, t.option)
    if (++sl_.declines == kDeclines) logf("sl: DLSS declined %d frames in a row: off for this run", kDeclines);
    if (told++ < 20) logf("sl: DLSS skipped this frame: %s; OoT's own upscale runs", why);
    return false;
}

Image sr_mv;   // (DLSS SR's camera motion, the frame's own after its evaluate)

/// The passes and samplers of DLSS SR and DLSS-G, once (the replay's thread)
VkSampler linear_sampler = VK_NULL_HANDLE;
Pass copy_pass, depth_pass, hudless_pass;
bool make_passes() {
    static int made = -1;
    if (made >= 0) return made;
    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    vkCreateSampler(dev.device, &si, nullptr, &point_sampler);
    si.magFilter = si.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(dev.device, &si, nullptr, &linear_sampler);
    color_pass = make_pass(DLSS_COLOR_COMP_SPV, sizeof DLSS_COLOR_COMP_SPV, 0);
    motion_pass = make_pass(DLSS_MOTION_COMP_SPV, sizeof DLSS_MOTION_COMP_SPV, 64);
    output_pass = make_pass(DLSS_OUTPUT_COMP_SPV, sizeof DLSS_OUTPUT_COMP_SPV, 0);
    copy_pass = make_pass(DLSSG_COPY_COMP_SPV, sizeof DLSSG_COPY_COMP_SPV, 0);
    depth_pass = make_pass(DLSSG_DEPTH_COMP_SPV, sizeof DLSSG_DEPTH_COMP_SPV, 0);
    hudless_pass = make_pass(DLSSG_HUDLESS_COMP_SPV, sizeof DLSSG_HUDLESS_COMP_SPV, sizeof(float) * 12);
    made = point_sampler && linear_sampler && color_pass.pipe && motion_pass.pipe && output_pass.pipe && copy_pass.pipe &&
           depth_pass.pipe && hudless_pass.pipe;
    return made;
}

/// The frame Streamline's token and markers name: the game's presents, this one included (each game present is one
/// do_present; presents_now counts those before the frame being recorded)
uint32_t sl_frame() { return (uint32_t)(presents_now + 1); }
std::atomic<uint32_t> constants_for{0};   // (the frame whose camera constants are set: once a frame, SR's or DLSS-G's)

/// The camera constants of a frame, from the scene (DLSS SR and DLSS-G share them; set once a frame)
bool set_constants(const sl::FrameToken& token, const DlssScene& scene, const float* to_prev, bool reset, uint32_t in_w,
                   uint32_t in_h) {
    const sl::ViewportHandle viewport(0);
    sl::Constants k{};
    float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float back[16];
    k.cameraViewToClip = to_sl(scene.vp, true);   // (view and projection come combined: the view-projection stands for both)
    // (Streamline's matrices are row vectors, v * M: sl_matrix_helpers.h keeps translations in row 3 and composes
    // clipToView * viewToPrevView * prevViewToClip left to right, so each column-form matrix of the game goes in transposed)
    float inv_vp[16];
    k.clipToCameraView = clip_to_prev(scene.vp, identity, inv_vp) ? to_sl(inv_vp, true) : to_sl(identity, false);
    k.clipToLensClip = to_sl(identity, false);
    k.clipToPrevClip = to_sl(to_prev, true);   // (Streamline's row vectors: the transpose of the shader's column form)
    k.prevClipToClip = clip_to_prev(scene.prev_vp, scene.vp, back) ? to_sl(back, true) : to_sl(identity, false);
    k.jitterOffset = {0.0f, 0.0f};   // (OoT renders unjittered: DLSS-GATE.md 2.5; Halton jitter is research D3)
    k.mvecScale = {1.0f / (float)in_w, 1.0f / (float)in_h};
    k.cameraPinholeOffset = {0.0f, 0.0f};
    k.cameraPos = {0, 0, 0}, k.cameraUp = {0, 1, 0}, k.cameraRight = {1, 0, 0}, k.cameraFwd = {0, 0, 1};
    k.cameraNear = scene.near_z, k.cameraFar = scene.far_z;
    k.cameraFOV = scene.fov, k.cameraAspectRatio = scene.aspect;
    k.depthInverted = sl::Boolean::eFalse;            // (1 is far: DLSS-GATE.md 2.3)
    k.cameraMotionIncluded = sl::Boolean::eTrue;      // (the motion pass made it)
    k.motionVectors3D = sl::Boolean::eFalse;
    k.reset = reset ? sl::Boolean::eTrue : sl::Boolean::eFalse;
    if (sl_.constants(k, token, viewport) != sl::Result::eOk) return false;
    constants_for = (uint32_t)token;
    return true;
}

// ---- DLSS Frame Generation and Reflex (vk_dlss.h; DLSS-GATE.md 9) ----
struct FrameGen {
    bool asked = false;            // (dlss_load: its row on and sl.dlss_g.dll there: DLSS-G, Reflex and PCL loaded at slInit)
    std::once_flag once;
    std::atomic<bool> ok{false};   // (supported by the card, driver and Windows, its calls found)
    bool reflex = true;            // (RX_SL_REFLEX=0: Reflex's low latency off, its markers kept: the latency A/B)
    sl::ReflexOptions reflex_set{};   // (sent again after each swapchain: dlssg_swapchain_made)
    PFun_slDLSSGSetOptions* options = nullptr;
    PFun_slDLSSGGetState* state = nullptr;
    PFun_slReflexSetOptions* reflex_options = nullptr;
    PFun_slReflexSleep* sleep = nullptr;
    PFun_slReflexGetState* reflex_state = nullptr;
    PFun_slPCLSetMarker* marker = nullptr;
    std::atomic<uint32_t> game_frame{0};      // (the game's presents handed over, the frame ids: the game's thread)
    std::atomic<uint32_t> submit_started{0};  // (the last frame whose render submit start is marked)
    // (the replay's thread from here)
    uint32_t scene_for = 0;                   // (the frame whose depth and motion are tagged)
    uint32_t most = 0;                        // (numFramesToGenerateMax, read at the first present with the swapchain)
    Image mv, depth, before_hud, hudless;
    uint64_t last_frame = ~0ull;              // (a camera cut, when DLSS SR does not set the constants)
    float last_vp[16]{};
    sl::DLSSGMode mode_set = sl::DLSSGMode::eCount;
    uint32_t frames_set = 0;
    // (the report every 600 presents: frames shown, Reflex's latency)
    uint64_t shown = 0, report_at = 0;
    uint32_t off_scene = 0, off_menu = 0, off_other = 0;   // (presents without made frames, by why)
} fg;

bool fg_ready() {
    if (!sl_.mod || !fg.asked) return false;
    std::call_once(fg.once, [] {
        if (getenv("RX_VK_PARALLEL") && atoi(getenv("RX_VK_PARALLEL")) > 1) {   // (the scene noted from recording threads)
            logf("sl: DLSS Frame Generation not with the parallel replay (RX_VK_PARALLEL); the game's frames only");
            return;
        }
        sl::AdapterInfo ai{};
        ai.vkPhysicalDevice = dev.physical;
        const sl::Result g = sl_.supported(sl::kFeatureDLSS_G, ai), r = sl_.supported(sl::kFeatureReflex, ai),
                         c = sl_.supported(sl::kFeaturePCL, ai);
        if (g != sl::Result::eOk || r != sl::Result::eOk || c != sl::Result::eOk) {
            logf("sl: DLSS Frame Generation not available (Streamline results %d %d %d: an RTX 40 or 50 card, a recent driver and "
                 "Windows' hardware-accelerated GPU scheduling are needed); the game's frames only", (int)g, (int)r, (int)c);
            return;
        }
        const auto get = [](sl::Feature f, const char* name) {
            void* p = nullptr;
            return sl_.feature(f, name, p) == sl::Result::eOk ? p : nullptr;
        };
        fg.options = (PFun_slDLSSGSetOptions*)get(sl::kFeatureDLSS_G, "slDLSSGSetOptions");
        fg.state = (PFun_slDLSSGGetState*)get(sl::kFeatureDLSS_G, "slDLSSGGetState");
        fg.reflex_options = (PFun_slReflexSetOptions*)get(sl::kFeatureReflex, "slReflexSetOptions");
        fg.sleep = (PFun_slReflexSleep*)get(sl::kFeatureReflex, "slReflexSleep");
        fg.reflex_state = (PFun_slReflexGetState*)get(sl::kFeatureReflex, "slReflexGetState");
        fg.marker = (PFun_slPCLSetMarker*)get(sl::kFeaturePCL, "slPCLSetMarker");
        if (!fg.options || !fg.state || !fg.reflex_options || !fg.sleep || !fg.marker) {
            logf("sl: DLSS Frame Generation not available: Streamline lacks its calls; the game's frames only");
            return;
        }
        fg.reflex = !getenv("RX_SL_REFLEX") || atoi(getenv("RX_SL_REFLEX")) != 0;
        // (Reflex's low latency, no limit of its own: host/nn_vi.c's pacing before the frame starts stays the one limiter, 60 fps
        // (frameLimitUs 16667 did not hold the title and menus: 80 to 380 fps, run w1). Its options are set once even when off)
        sl::ReflexOptions ro{};
        ro.mode = fg.reflex ? sl::ReflexMode::eLowLatency : sl::ReflexMode::eOff;
        ro.frameLimitUs = 0;
        fg.reflex_set = ro;
        if (fg.reflex_options(ro) != sl::Result::eOk) {
            logf("sl: DLSS Frame Generation not available: Reflex's options were refused; the game's frames only");
            return;
        }
        fg.ok = true;
        logf("sl: DLSS Frame Generation available, %dx asked; Reflex %s", opts.dlssg,
             fg.reflex ? "on (low latency; the game's own pacing caps 60)" : "off (RX_SL_REFLEX=0)");
    });
    return fg.ok;
}

/// DLSS-G loaded, supported and its row on now (it can be switched off in game: then no passes, tags, markers or sleep)
bool fg_live() { return opts.dlssg >= 2 && fg_ready(); }

void mark(sl::PCLMarker m, uint32_t frame) {
    sl::FrameToken* t = nullptr;
    if (sl_.token(t, &frame) == sl::Result::eOk && t) fg.marker(m, *t);
}
} // namespace

PFN_vkGetInstanceProcAddr dlss_load() {
    menu_init();   // (the saved options: DLSS is chosen before the instance exists)
    if ((!opts.dlss && opts.dlssg < 2) || disabled) return nullptr;   // (DLSS SR or DLSS-G asked for)
    wchar_t exe[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    if (!n || n >= MAX_PATH) return refuse("the executable's folder is unknown");
    sl_dir.assign(exe, n);
    sl_dir.resize(sl_dir.find_last_of(L"\\/"));
    // (only the folder of the executable itself: never a search path, never the SDK's development\ builds)
    const std::wstring path = sl_dir + L"\\sl.interposer.dll";
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return refuse("no sl.interposer.dll beside the executable");
    if (!nvidia_card()) return refuse("no NVIDIA card");
    if (!sl::security::verifyEmbeddedSignature(path.c_str())) return refuse("sl.interposer.dll is not signed by NVIDIA");
    const HMODULE m = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!m) return refuse("sl.interposer.dll did not load");
    const auto get = [m](const char* name) { return (void*)GetProcAddress(m, name); };
    sl_.init = (PFun_slInit*)get("slInit");
    sl_.supported = (PFun_slIsFeatureSupported*)get("slIsFeatureSupported");
    sl_.feature = (PFun_slGetFeatureFunction*)get("slGetFeatureFunction");
    sl_.token = (PFun_slGetNewFrameToken*)get("slGetNewFrameToken");
    sl_.constants = (PFun_slSetConstants*)get("slSetConstants");
    sl_.tag = (PFun_slSetTagForFrame*)get("slSetTagForFrame");
    sl_.evaluate = (PFun_slEvaluateFeature*)get("slEvaluateFeature");
    const auto gipa = (PFN_vkGetInstanceProcAddr)get("vkGetInstanceProcAddr");
    if (!sl_.init || !sl_.supported || !sl_.feature || !sl_.token || !sl_.constants || !sl_.tag || !sl_.evaluate || !gipa) {
        FreeLibrary(m);
        return refuse("sl.interposer.dll lacks the calls this build needs");
    }
    // (DLSS-G with its Reflex and PCL only when its row is on and sl.dlss_g.dll is there: loaded, it puts the swapchain behind
    // Streamline's proxy, an off-screen copy every present)
    static sl::Feature features[4] = {sl::kFeatureDLSS};
    uint32_t nfeatures = 1;
    const auto there = [&](const wchar_t* dll) { return GetFileAttributesW((sl_dir + dll).c_str()) != INVALID_FILE_ATTRIBUTES; };
    // (DLSS-G depends on Reflex and PCL: never loaded without their plugins)
    fg.asked = opts.dlssg >= 2 && there(L"\\sl.dlss_g.dll") && there(L"\\sl.reflex.dll") && there(L"\\sl.pcl.dll");
    if (fg.asked)
        features[nfeatures++] = sl::kFeatureDLSS_G, features[nfeatures++] = sl::kFeatureReflex, features[nfeatures++] = sl::kFeaturePCL;
    static const wchar_t* paths[1];
    paths[0] = sl_dir.c_str();
    sl::Preferences pref{};
    pref.pathsToPlugins = paths, pref.numPathsToPlugins = 1;
    pref.pathToLogsAndData = nullptr;   // (no log files: sl_log)
    pref.logMessageCallback = sl_log;
    // (no optional OTA, no downloaded plugins loaded; our passes bind their own state; slSetTagForFrame needs frame-based
    // tagging. Note: the production interposer still starts NVIDIA's NGX updater, which may fetch into the NGX cache, and a
    // driver profile override can load OTA plugins; both are NVIDIA-signed and checked by Streamline: no flag stops them)
    pref.flags = sl::PreferenceFlags::eDisableCLStateTracking | sl::PreferenceFlags::eUseFrameBasedResourceTagging;
    pref.featuresToLoad = features, pref.numFeaturesToLoad = nfeatures;
    pref.engine = sl::EngineType::eCustom, pref.engineVersion = "harpia";
    pref.projectId = "5d8a0f3e-7c41-4b52-9a8e-3f6b1c2d4e70";
    pref.renderAPI = sl::RenderAPI::eVulkan;
    if (sl_.init(pref, sl::kSDKVersion) != sl::Result::eOk) {
        FreeLibrary(m);
        return refuse("slInit failed");
    }
    sl_.mod = m;
    logf("sl: Streamline loaded from the executable's folder; Vulkan through sl.interposer.dll");
    return gipa;
}

bool dlss_disable() {
    if (!sl_.mod) return false;
    sl_.mod = nullptr;   // (the module stays loaded, unused: freeing it under live hooks is not safe)
    disabled = true;
    return true;
}

bool dlss_active() {
    // (never with the parallel replay, RX_VK_PARALLEL above 1: the scene noted while drawing and read at the network's
    // dispatch would cross recording threads; vk_replay.cpp)
    static const bool parallel = getenv("RX_VK_PARALLEL") && atoi(getenv("RX_VK_PARALLEL")) > 1;
    return sl_.mod && opts.dlss && !parallel && sl_.failures.load(std::memory_order_relaxed) < 8 &&
           sl_.declines.load(std::memory_order_relaxed) < kDeclines &&
           (!sl_.checked.load(std::memory_order_relaxed) || sl_.ok.load(std::memory_order_relaxed));
}

bool dlss_dll_present() {
    static const bool present = [] {
        wchar_t exe[MAX_PATH];
        const DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
        if (!n || n >= MAX_PATH) return false;
        std::wstring p(exe, n);
        p.resize(p.find_last_of(L"\\/"));
        return GetFileAttributesW((p + L"\\sl.interposer.dll").c_str()) != INVALID_FILE_ATTRIBUTES;
    }();
    // (and the game's device NVIDIA's, once it exists; and DLSS supported on it, once asked: the row is not offered for nothing)
    return present && (!dev.device || dev.props.vendorID == 0x10DE) &&
           (!sl_.checked.load(std::memory_order_relaxed) || sl_.ok.load(std::memory_order_relaxed));
}

bool dlss_evaluate(VkCommandBuffer cmd, VkImageView color, uint32_t in_w, uint32_t in_h, VkImageView out, uint32_t out_w,
                   uint32_t out_h, const DlssScene& scene) {
    if (!dlss_active() || !dev.push_descriptor || !vkCmdPushDescriptorSetKHR) return false;
    dlss_out_h = out_h;
    if (!sl_.checked) {
        sl_.checked = true;
        sl::AdapterInfo ai{};
        ai.vkPhysicalDevice = dev.physical;
        void* f = nullptr;
        sl_.ok = sl_.supported(sl::kFeatureDLSS, ai) == sl::Result::eOk &&
                 sl_.feature(sl::kFeatureDLSS, "slDLSSSetOptions", f) == sl::Result::eOk && f;
        sl_.options = (PFun_slDLSSSetOptions*)f;
        void* g = nullptr;
        if (sl_.ok && sl_.feature(sl::kFeatureDLSS, "slDLSSGetOptimalSettings", g) == sl::Result::eOk)
            sl_.optimal = (PFun_slDLSSGetOptimalSettings*)g;
        sl_.ok = sl_.ok && make_passes();
        logf("sl: DLSS %s", sl_.ok ? "available" : "not available on this card or driver; OoT's own upscale runs");
    }
    if (!sl_.ok) return false;
    if (scene.frame != texture_frame_no() || !scene.depth) return fail("no scene camera this frame");
    if (scene.width != in_w || scene.height != in_h) return fail("the scene's depth is not the network's input size");
    if (!(scene.depth_usage & VK_IMAGE_USAGE_SAMPLED_BIT)) return fail("the scene's depth cannot be sampled");
    const VkImageView dview = depth_view(scene);
    float to_prev[16];
    if (!dview || !clip_to_prev(scene.vp, scene.prev_vp, to_prev)) return fail("no usable camera");

    static Image col, res;
    Image& mv = sr_mv;   // (DLSS-G tags it too, after this)
    static int mode_set = -1;
    static uint32_t out_set_w = 0, out_set_h = 0, in_set_w = 0, in_set_h = 0;   // (the mode follows the drawn size too)
    const int mode = opts.dlss;   // (read once: the menu may change it while this frame is recorded)
    // (option c: the scale for DLSS's optimal input, a float the host applies (t.option 18). The menu's integer percent
    // drew 1743x981 for Quality's optimal 1707x960 at 2.5K, ~2.7 ms more GPU a frame (54.7 fps against 60 at 1707x960);
    // the game's percent-to-size curve is not linear (0.6565 drew 0.667, 0.67 drew 0.681, 0.5 drew 0.5), so the first
    // scale, optimal / output, is corrected from what was drawn: 3 times at most per mode or output)
    static uint32_t opt_h = 0;
    static int corrections = 0;
    static float target = 0.0f;
    static uint64_t published_at = 0;
    const auto publish = [&](float s) {
        static int told;
        uint32_t b = 0;
        if (s > 0.0f) std::memcpy(&b, &s, 4);
        target = s, dlss_scale_bits = b, published_at = scene.frame;
        if (s > 0.0f && told++ < 40)
            logf("sl: DLSS asks the game for a scale of %.4f: optimal %u lines of %u, drawn %u", s, opt_h, out_h, in_h);
    };
    const bool new_mode = mode_set != mode || out_set_w != out_w || out_set_h != out_h;
    static uint64_t last_frame = ~0ull;
    static float last_vp[16];
    bool reset = last_frame == ~0ull || scene.frame != last_frame + 1;
    for (int i = 0; i < 16 && !reset; i++) reset = std::fabs(scene.prev_vp[i] - last_vp[i]) > 1e-3f;   // (a camera cut)
    if (col.w != in_w || col.h != in_h) {
        reset = true;
        if (!make_image(cmd, col, VK_FORMAT_R16G16B16A16_SFLOAT, in_w, in_h)) return fail("its colour input could not be made");
    }
    if (mv.w != in_w || mv.h != in_h) {   // (apart: one made and the other not is tried again next frame)
        reset = true;
        if (!make_image(cmd, mv, VK_FORMAT_R16G16_SFLOAT, in_w, in_h)) return fail("its motion input could not be made");
    }
    if (res.w != out_w || res.h != out_h) {
        reset = true;
        if (!make_image(cmd, res, VK_FORMAT_R16G16B16A16_SFLOAT, out_w, out_h)) return fail("its output could not be made");
    }
    const sl::ViewportHandle viewport(0);
    if (mode_set != mode || out_set_w != out_w || out_set_h != out_h || in_set_w != in_w || in_set_h != in_h) {
        sl::DLSSOptions o{};
        // (an input already the output's size is DLAA, whatever the mode: a full-size network, or a scale the engine capped)
        // (the mode follows the scene the game really drew: at the 2.5K output (graphics scale level 0) the game ignores the
        // fixed render scale (the scale67 probe drew 1920x1080), so every mode gets the network's 0.75)
        const float ratio = (float)in_h / (float)out_h;
        o.mode = in_w == out_w && in_h == out_h ? sl::DLSSMode::eDLAA
                 : ratio > 0.70f                ? sl::DLSSMode::eMaxQuality
                                                : sl_mode(mode, out_h);
        const int asked = dlss_scale_percent(mode, out_h, dlss_base());
        // (Native AA asks above 1.0 and the game clamps it to the output: no mismatch then)
        if (std::fabs(std::min(1.0f, dlss_base() * asked / 100.0f) - ratio) > 0.02f)
            logf("sl: DLSS mode %d asked for %d%% of the scale, the game drew %ux%u (%.3f of the output): the mode runs at that",
                 mode, asked, in_w, in_h, ratio);
        o.outputWidth = out_w, o.outputHeight = out_h;
        o.colorBuffersHDR = sl::Boolean::eFalse;   // (tonemapped, square-root encoded: DLSS-GATE.md 2.6)
        o.useAutoExposure = sl::Boolean::eFalse;
        // (NVIDIA's transformer defaults pinned, so a driver or OTA update cannot change the look; after Lichii 72067ce4e1,
        // which pins K. E/F are deprecated in this SDK)
        o.dlaaPreset = o.qualityPreset = o.balancedPreset = o.ultraQualityPreset = sl::DLSSPreset::ePresetK;
        o.performancePreset = sl::DLSSPreset::ePresetM;
        o.ultraPerformancePreset = sl::DLSSPreset::ePresetL;
        if (sl_.options(viewport, o) != sl::Result::eOk) return sl_.failures++, fail("slDLSSSetOptions failed");
        mode_set = mode, out_set_w = out_w, out_set_h = out_h, in_set_w = in_w, in_set_h = in_h;
        sl::DLSSOptimalSettings best{};   // (the scene's size against what DLSS takes for the mode: logged, the first run's check)
        if (sl_.optimal && sl_.optimal(o, best) == sl::Result::eOk)
            logf("sl: DLSS mode %d at %ux%u: the scene is %ux%u, DLSS's optimal %ux%u (accepts %ux%u to %ux%u)", mode, out_w,
                 out_h, in_w, in_h, best.optimalRenderWidth, best.optimalRenderHeight, best.renderWidthMin, best.renderHeightMin,
                 best.renderWidthMax, best.renderHeightMax);
        if (new_mode) {   // (Native AA keeps the menu's 133%: above 1.0 the game takes its full-size network)
            corrections = 0;
            const bool fit = o.mode != sl::DLSSMode::eDLAA && sl_.optimal && best.optimalRenderHeight &&
                             best.optimalRenderHeight < out_h;
            opt_h = fit ? best.optimalRenderHeight : 0;
            publish(fit ? (float)opt_h / (float)out_h : 0.0f);
        }
        reset = true;
    }

    full_barrier(cmd);
    run_pass(cmd, color_pass, color, col.view, in_w, in_h, nullptr, 0);
    run_pass(cmd, motion_pass, dview, mv.view, in_w, in_h, to_prev, sizeof to_prev);
    full_barrier(cmd);

    sl::FrameToken* token = nullptr;
    const uint32_t index = sl_frame();   // (the frame DLSS-G presents and Reflex's markers name too)
    if (sl_.token(token, &index) != sl::Result::eOk || !token) return sl_.failures++, fail("no frame token");
    if (!set_constants(*token, scene, to_prev, reset, in_w, in_h)) return sl_.failures++, fail("slSetConstants failed");

    sl::Resource rc = resource_of(col.image, col.mem, col.view, col.format, col.usage, in_w, in_h);
    sl::Resource rd = resource_of(scene.depth, scene.depth_mem, dview, VK_FORMAT_D32_SFLOAT, scene.depth_usage, in_w, in_h);
    sl::Resource rm = resource_of(mv.image, mv.mem, mv.view, mv.format, mv.usage, in_w, in_h);
    sl::Resource ro = resource_of(res.image, res.mem, res.view, res.format, res.usage, out_w, out_h);
    const sl::Extent in_ext{0, 0, in_w, in_h}, out_ext{0, 0, out_w, out_h};
    const sl::ResourceTag tags[] = {{&rc, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eValidUntilEvaluate, &in_ext},
                                    {&rd, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilEvaluate, &in_ext},
                                    {&rm, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilEvaluate, &in_ext},
                                    {&ro, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilEvaluate, &out_ext}};
    auto* const slcmd = reinterpret_cast<sl::CommandBuffer*>(cmd);
    if (sl_.tag(*token, viewport, tags, 4, slcmd) != sl::Result::eOk) return sl_.failures++, fail("slSetTagForFrame failed");
    const sl::BaseStructure* inputs[] = {&viewport};
    if (sl_.evaluate(sl::kFeatureDLSS, *token, inputs, 1, slcmd) != sl::Result::eOk)
        return sl_.failures++, fail("slEvaluateFeature failed");

    full_barrier(cmd);
    run_pass(cmd, output_pass, res.view, out, out_w, out_h, nullptr, 0);
    sl_.failures = 0, sl_.declines = 0;
    // (the drawn size away from the optimal one, once the last scale had time to apply (the game's 60-frame window))
    if (opt_h && target > 0.0f && corrections < 3 && scene.frame > published_at + 120 && (in_h + 2 < opt_h || in_h > opt_h + 2))
        corrections++, publish(std::clamp(target * (float)opt_h / (float)in_h, 0.2f, 1.0f));
    last_frame = scene.frame;
    std::memcpy(last_vp, scene.vp, sizeof last_vp);
    static bool told;
    if (!told) told = true, logf("sl: DLSS evaluate ok: %ux%u -> %ux%u", in_w, in_h, out_w, out_h);
    return true;
}

// ---- DLSS Frame Generation and Reflex ----
bool dlssg_dll_present() {
    static const bool dll = [] {
        wchar_t exe[MAX_PATH];
        const DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
        if (!n || n >= MAX_PATH) return false;
        std::wstring p(exe, n);
        p.resize(p.find_last_of(L"\\/"));
        return GetFileAttributesW((p + L"\\sl.dlss_g.dll").c_str()) != INVALID_FILE_ATTRIBUTES;
    }();
    if (!dll || !dlss_dll_present()) return false;
    if (sl_.mod && fg.asked) return fg_ready();   // (loaded at the start: Streamline's own answer)
    // (not loaded: frame generation's cards by name, GeForce RTX 40 and 50, the Ada and Blackwell workstation cards; a Quadro
    // RTX 4000 is Turing. Streamline's check at the next start decides, and logs why when it refuses)
    const char* n = dev.props.deviceName;
    return strstr(n, "GeForce RTX 40") || strstr(n, "GeForce RTX 50") || strstr(n, "Ada Generation") || strstr(n, "Blackwell");
}

bool dlssg_on() { return fg_ready(); }

bool dlss_reflex_active() { return fg_live() && fg.reflex; }

void dlssg_game_present() {
    if (fg_ready()) {   // (the frame ids count every present, the row on or off)
        ++fg.game_frame;
        if (opts.dlssg >= 2) mark(sl::PCLMarker::eSimulationEnd, fg.game_frame);
    }
}

bool dlssg_frame_start() {
    if (!fg_live()) return false;
    // (the next frame: Reflex sleeps until it should start (always called, also with its low latency off), then it starts)
    uint32_t next = fg.game_frame.load() + 1;
    sl::FrameToken* t = nullptr;
    if (sl_.token(t, &next) != sl::Result::eOk || !t) return false;
    const bool slept = fg.sleep(*t) == sl::Result::eOk;
    fg.marker(sl::PCLMarker::eSimulationStart, *t);
    static int told;
    if (!slept && told++ < 5) logf("sl: Reflex's sleep failed");
    return slept;
}

void dlssg_render_submit(bool start, int64_t frame) {
    if (!fg.ok || opts.dlssg < 2) return;
    const uint32_t f = (uint32_t)frame;
    if (start && fg.submit_started.exchange(f) == f) return;
    mark(start ? sl::PCLMarker::eRenderSubmitStart : sl::PCLMarker::eRenderSubmitEnd, f);
}

void dlssg_present_marker(bool start, int64_t frame) {
    if (fg.ok && opts.dlssg >= 2) mark(start ? sl::PCLMarker::ePresentStart : sl::PCLMarker::ePresentEnd, (uint32_t)frame);
}

void dlssg_scene_done(VkCommandBuffer cmd, VkImageView out, uint32_t out_w, uint32_t out_h, const DlssScene& scene) {
    if (!fg_live() || !dev.push_descriptor || !vkCmdPushDescriptorSetKHR || !make_passes()) return;
    uint32_t f = sl_frame();
    if (fg.scene_for == f) return;   // (once a frame)
    // (the colour before the HUD: the HUD is drawn into out next, then the final pass encodes it as sRGB)
    if ((fg.before_hud.w != out_w || fg.before_hud.h != out_h) &&
        !make_image(cmd, fg.before_hud, VK_FORMAT_R16G16B16A16_SFLOAT, out_w, out_h))
        return;
    full_barrier(cmd);
    run_pass(cmd, copy_pass, out, fg.before_hud.view, out_w, out_h, nullptr, 0);
    full_barrier(cmd);
    // (the scene's depth and camera motion: DLSS SR's when it ran this frame, else made here the same way)
    if (scene.frame != texture_frame_no() || !scene.depth || !(scene.depth_usage & VK_IMAGE_USAGE_SAMPLED_BIT)) return;
    const VkImageView dview = depth_view(scene);
    float to_prev[16];
    if (!dview || !clip_to_prev(scene.vp, scene.prev_vp, to_prev)) return;
    const uint32_t w = scene.width, h = scene.height;
    const bool sr = constants_for == f && sr_mv.w == w && sr_mv.h == h;
    if ((fg.depth.w != w || fg.depth.h != h) && !make_image(cmd, fg.depth, VK_FORMAT_R32_SFLOAT, w, h)) return;
    run_pass(cmd, depth_pass, dview, fg.depth.view, w, h, nullptr, 0);
    if (!sr) {
        if ((fg.mv.w != w || fg.mv.h != h) && !make_image(cmd, fg.mv, VK_FORMAT_R16G16_SFLOAT, w, h)) return;
        run_pass(cmd, motion_pass, dview, fg.mv.view, w, h, to_prev, sizeof to_prev);
    }
    full_barrier(cmd);
    const Image& mv = sr ? sr_mv : fg.mv;
    sl::FrameToken* token = nullptr;
    if (sl_.token(token, &f) != sl::Result::eOk || !token) return;
    if (constants_for != f) {
        bool reset = fg.last_frame == ~0ull || scene.frame != fg.last_frame + 1;
        for (int i = 0; i < 16 && !reset; i++) reset = std::fabs(scene.prev_vp[i] - fg.last_vp[i]) > 1e-3f;   // (a camera cut)
        if (!set_constants(*token, scene, to_prev, reset, w, h)) return;
        fg.last_frame = scene.frame;
        std::memcpy(fg.last_vp, scene.vp, sizeof fg.last_vp);
    }
    // (valid until the present: images of ours (the depth copied: the game's may be destroyed once the frame's work is done),
    // and the frame's work, its present and the next frame's work go on the one queue, which DLSS-G's default queue mode
    // (eBlockPresentingClientQueue) holds until its work has read them; no copies)
    sl::Resource rd = resource_of(fg.depth.image, fg.depth.mem, fg.depth.view, fg.depth.format, fg.depth.usage, w, h);
    sl::Resource rm = resource_of(mv.image, mv.mem, mv.view, mv.format, mv.usage, w, h);
    const sl::Extent ext{0, 0, w, h};
    const sl::ResourceTag tags[] = {{&rd, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent, &ext},
                                    {&rm, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent, &ext}};
    const sl::ViewportHandle viewport(0);
    if (sl_.tag(*token, viewport, tags, 2, reinterpret_cast<sl::CommandBuffer*>(cmd)) != sl::Result::eOk) return;
    fg.scene_for = f;
}

bool dlssg_generating() { return fg.ok && fg.mode_set == sl::DLSSGMode::eOn; }

void dlssg_swapchain_made() {
    // (Reflex's options again with the swapchain there: the first ones went before it existed, from create_swapchain)
    if (fg.ok && fg.reflex_options(fg.reflex_set) != sl::Result::eOk) logf("sl: Reflex's options refused after a new swapchain");
}

void dlssg_force_off() {
    if (!dlssg_generating()) return;
    sl::DLSSGOptions o{};
    o.mode = sl::DLSSGMode::eOff;
    o.numFramesToGenerate = std::max(fg.frames_set, 1u);
    o.flags = sl::DLSSGFlags::eRetainResourcesWhenOff;
    const bool set = fg.options(sl::ViewportHandle(0), o) == sl::Result::eOk;
    fg.mode_set = sl::DLSSGMode::eOff;
    logf("sl: DLSS Frame Generation off for a swapchain made again%s", set ? "" : " (refused)");
}

void dlssg_present(VkCommandBuffer cmd, uint32_t sw, uint32_t sh, const int crop[4], int rx, int ry, uint32_t rw, uint32_t rh,
                   bool resizing) {
    if (!fg.ok) return;
    uint32_t f = (uint32_t)presents_now;
    const sl::ViewportHandle viewport(0);
    sl::DLSSGState st{};
    const bool have_state = fg.state(viewport, st, nullptr) == sl::Result::eOk;
    if (have_state && std::max(1u, st.numFramesToGenerateMax) > fg.most) {
        fg.most = std::max(1u, st.numFramesToGenerateMax);
        logf("sl: DLSS Frame Generation: up to %ux on this card (status %#x, at least %u pixels a side)", fg.most + 1,
             (unsigned)st.status, st.minWidthOrHeight);
    }
    const VkExtent2D e = dev.swap_extent;
    // (generated frames only for the game's own scene: not in the menu, a load, a movie, a frame without the scene's camera,
    // nor while the window changes size or is under DLSS-G's smallest side)
    bool on = fg.scene_for == f && !menu_is_open() && !resizing && opts.dlssg >= 2 && fg.before_hud.view && sw && sh && rw &&
              rh && fg.most && have_state && e.width >= st.minWidthOrHeight && e.height >= st.minWidthOrHeight;
    if (!on) (fg.scene_for != f ? fg.off_scene : menu_is_open() ? fg.off_menu : fg.off_other)++;
    static uint32_t status_told = ~0u;
    static int status_lines;
    if (have_state && (uint32_t)st.status != status_told && status_lines++ < 20)   // (its reasons, as they change)
        status_told = (uint32_t)st.status, logf("sl: DLSS Frame Generation status %#x at present %u", status_told, f);
    if (on && (fg.hudless.w != e.width || fg.hudless.h != e.height))
        on = make_image(cmd, fg.hudless, VK_FORMAT_R8G8B8A8_UNORM, e.width, e.height);
    if (on) {
        const bool cropped = crop && crop[2] > 0 && crop[3] > 0;
        const float push[12] = {(float)rx, (float)ry, (float)rw, (float)rh,
                                cropped ? (float)crop[0] / sw : 0.0f, cropped ? (float)crop[1] / sh : 0.0f,
                                cropped ? (float)crop[2] / sw : 1.0f, cropped ? (float)crop[3] / sh : 1.0f, 0, 0, 0, 0};
        full_barrier(cmd);
        run_pass(cmd, hudless_pass, fg.before_hud.view, fg.hudless.view, e.width, e.height, push, sizeof push, linear_sampler);
        full_barrier(cmd);
        sl::FrameToken* token = nullptr;
        sl::Resource rh_ = resource_of(fg.hudless.image, fg.hudless.mem, fg.hudless.view, fg.hudless.format, fg.hudless.usage,
                                       e.width, e.height);
        const sl::Extent ext{0, 0, e.width, e.height};
        // (the picture's rectangle in the back buffer: Aspect Fit's bars are copied as they are; the HUD-less colour stays the
        // back buffer's size, as NVIDIA's guide 5.1 asks; no resource for the back buffer's tag, Streamline knows it)
        const sl::Extent sub{(uint32_t)rx, (uint32_t)ry, rw, rh};
        const sl::ResourceTag tags[] = {{&rh_, sl::kBufferTypeHUDLessColor, sl::ResourceLifecycle::eValidUntilPresent, &ext},
                                        {nullptr, sl::kBufferTypeBackbuffer, sl::ResourceLifecycle::eValidUntilPresent, &sub}};
        on = sl_.token(token, &f) == sl::Result::eOk && token &&
             sl_.tag(*token, viewport, tags, 2, reinterpret_cast<sl::CommandBuffer*>(cmd)) == sl::Result::eOk;
    }
    const sl::DLSSGMode mode = on ? sl::DLSSGMode::eOn : sl::DLSSGMode::eOff;
    const uint32_t frames = std::clamp<uint32_t>((uint32_t)std::max(opts.dlssg - 1, 1), 1u, std::max(fg.most, 1u));
    if (mode != fg.mode_set || frames != fg.frames_set) {
        sl::DLSSGOptions o{};
        o.mode = mode;
        o.numFramesToGenerate = frames;
        o.flags = sl::DLSSGFlags::eRetainResourcesWhenOff;   // (off in menus and loads without the stutter of a new allocation)
        // (RX_SL_FG_MADE_ONLY=1, a test: only the made frames shown, for their artefacts on the HUD and particles)
        static const bool made_only = getenv("RX_SL_FG_MADE_ONLY") && atoi(getenv("RX_SL_FG_MADE_ONLY")) != 0;
        if (made_only) o.flags = o.flags | sl::DLSSGFlags::eShowOnlyInterpolatedFrame;
        const bool set = fg.options(viewport, o) == sl::Result::eOk;
        static int told;
        if (told++ < 40)
            logf("sl: DLSS Frame Generation %s at present %u (%ux)%s", on ? "on" : "off", f, frames + 1, set ? "" : ": refused");
        if (set || mode == sl::DLSSGMode::eOff) fg.mode_set = mode, fg.frames_set = frames;
    }
    // (every 600 presents: the frames shown against the game's, and Reflex's latency from the frame's start to its GPU end)
    if (have_state) fg.shown += st.numFramesActuallyPresented;
    if (f % 600 == 0) {
        const uint64_t now = (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::steady_clock::now().time_since_epoch()).count();
        sl::ReflexState rs{};
        double lat = 0;
        int n = 0;
        if (fg.reflex_state && fg.reflex_state(rs) == sl::Result::eOk && rs.latencyReportAvailable)
            for (const sl::ReflexReport& r : rs.frameReport)
                if (r.simStartTime && r.gpuRenderEndTime > r.simStartTime) lat += (double)(r.gpuRenderEndTime - r.simStartTime), n++;
        if (fg.report_at)
            logf("sl: DLSS-G over the last 600 presents: game %.1f fps, shown %.1f fps; Reflex: simulation start to GPU end "
                 "%.2f ms (%d frames); off for %u (no scene camera %u, menu %u, other %u)", 600e6 / (double)(now - fg.report_at),
                 (double)fg.shown * 1e6 / (double)(now - fg.report_at), n ? lat / n / 1000.0 : 0.0, n,
                 fg.off_scene + fg.off_menu + fg.off_other, fg.off_scene, fg.off_menu, fg.off_other);
        fg.report_at = now, fg.shown = 0, fg.off_scene = fg.off_menu = fg.off_other = 0;
    }
}

} // namespace mkwvk

#else   // (no Streamline headers at build time, or not Windows: DLSS is never on)
namespace mkwvk {
PFN_vkGetInstanceProcAddr dlss_load() { return nullptr; }
bool dlss_active() { return false; }
bool dlss_dll_present() { return false; }
bool dlss_disable() { return false; }
bool dlss_evaluate(VkCommandBuffer, VkImageView, uint32_t, uint32_t, VkImageView, uint32_t, uint32_t, const DlssScene&) { return false; }
bool dlssg_dll_present() { return false; }
bool dlssg_on() { return false; }
bool dlss_reflex_active() { return false; }
void dlssg_game_present() {}
bool dlssg_frame_start() { return false; }
void dlssg_render_submit(bool, int64_t) {}
void dlssg_scene_done(VkCommandBuffer, VkImageView, uint32_t, uint32_t, const DlssScene&) {}
void dlssg_present(VkCommandBuffer, uint32_t, uint32_t, const int*, int, int, uint32_t, uint32_t, bool) {}
bool dlssg_generating() { return false; }
void dlssg_swapchain_made() {}
void dlssg_force_off() {}
void dlssg_present_marker(bool, int64_t) {}
} // namespace mkwvk
#endif
