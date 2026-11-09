// (hookshot) Offline test of the Ampere -> Maxwell lowering (shader_recompiler/frontend/ampere/lower_to_maxwell.h).
// Runs LowerToMaxwell over native programs a run dumped (RX_VK_DUMP_FAILED=<folder>: <stage>_<key>.bin, 16-byte
// instructions) and groups what still fails by reason and opcode, with the first failing instructions' words.
//   lower_test <folder> [samples per reason]      (built by nx2/lower_test.py)
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include "shader_recompiler/frontend/ampere/lower_to_maxwell.h"
#include "shader_recompiler/frontend/ampere/program_capture.h"

namespace {
using Shader::Ampere::Instruction;
using Capture = Shader::Ampere::ProgramCaptureState;
int failures = 0;
void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    failures += !ok;
}
constexpr Instruction kExit{{0x794d, 0, 0x03800000, 0x000fea00}};   // EXIT (Pc = PT)
constexpr Instruction kNop{{0x7918, 0, 0, 0x000fc000}};
constexpr Instruction kRet{{0x7950, 0, 0x03c00000, 0x000fea00}};
// The index the capture completes at (-1: it ran off the end, -2: InvalidTarget), as vk_shader.cpp runs it
int capture_end(const std::vector<Instruction>& code) {
    Capture c;
    for (unsigned i = 0; i < code.size(); i++) {
        const auto r = c.Observe(code[i], i);
        if (r == Capture::Result::InvalidTarget) return -2;
        if (r == Capture::Result::Complete) return (int)i;
    }
    return -1;
}
// (lower_test --self) fixtures for the capture (C2) and DEPBAR (C3) changes; exit status 1 on any failure
int self_test() {
    char what[160];
    // C3: DEPBAR.LE SBn, count, {list} lowers to nothing, for every scoreboard, count and wait list
    const auto exit_only = Shader::Ampere::LowerToMaxwell(std::vector<Instruction>{kExit});
    check((bool)exit_only, "EXIT alone lowers");
    for (unsigned sb = 0; sb < 6; sb++)
        for (unsigned count : {0u, 1u, 3u})
            for (unsigned list : {0u, 0x0eu, 0x1eu}) {
                const Instruction depbar{{0x791a, 0x8000u | sb << 12 | count << 6 | list, 0, 0x000fce00}};
                const auto r = Shader::Ampere::LowerToMaxwell(std::vector<Instruction>{depbar, kExit});
                std::snprintf(what, sizeof what, "DEPBAR.LE SB%u, %u, list %#x lowers to no instruction", sb, count, list);
                check(r && exit_only && r.instructions.size() == exit_only.instructions.size(), what);
            }
    for (const unsigned w1 : {0x801eu, 0x800eu, 0xd040u}) {   // (OoT's two SB0 waits, and the SB5 one that always lowered)
        const auto r = Shader::Ampere::LowerToMaxwell(std::vector<Instruction>{{{0x791a, w1, 0, 0x000fce00}}, kExit});
        std::snprintf(what, sizeof what, "DEPBAR words[1] %#x (an OoT encoding) lowers", w1);
        check((bool)r, what);
    }
    // (upstream 2e97b33, tests/lower_depbar_test.cpp: a DEPBAR without .LE, the set alone, is no work here either)
    check((bool)Shader::Ampere::LowerToMaxwell(std::vector<Instruction>{{{0x791a, 0x0040, 0, 0}}, kExit}),
          "DEPBAR without bit 47 (no .LE) lowers");
    check(!Shader::Ampere::LowerToMaxwell(std::vector<Instruction>{{{0x791a, 0x8000, 1, 0}}, kExit}),
          "DEPBAR with words[2] set is still rejected");
    check(!Shader::Ampere::LowerToMaxwell(std::vector<Instruction>{{{0x791a, 0x18000, 0, 0}}, kExit}),
          "DEPBAR with bits past 47 is still rejected");
    // C2: BRA.DIV / .CONV keep their mode in the offset's two low bits: not an unaligned target
    for (const unsigned mode : {2u, 3u}) {
        const std::vector<Instruction> code{{{0x7947, 0x10u | mode, 0x03800000, 0}}, kExit, kExit};
        std::snprintf(what, sizeof what, "BRA mode %u to +2: captured to its target's EXIT (index 2)", mode);
        check(capture_end(code) == 2, what);
    }
    // C2: CALL.REL.NOINC is a call: the capture ends at its callee's RET, not at an EXIT past it
    {
        const std::vector<Instruction> code{{{0x7944, 0x20, 0x03c00000, 0}}, kExit, kNop, kNop, kRet, kExit, kNop};
        check(capture_end(code) == 4, "CALL.NOINC to +3, EXIT, callee, RET: captured to the RET (index 4)");
    }
    {   // (an EXIT after the callee still ends it there)
        const std::vector<Instruction> code{{{0x7944, 0x10, 0x03c00000, 0}}, {{0x7947, 0x20, 0x03800000, 0}}, kNop, kRet,
                                            kExit, kNop};
        check(capture_end(code) == 4, "CALL.NOINC to +2, BRA over the callee to an EXIT: captured to the EXIT (index 4)");
    }
    {   // (the lowering's NOINC forward transfer to a loop's reconvergence: no RET, ends at the EXIT after it)
        const std::vector<Instruction> code{{{0x6944, 0x10, 0x03c00000, 0}}, {{0x7947, 0x10, 0x03800000, 0}}, kNop, kExit,
                                            kNop};
        check(capture_end(code) == 3, "predicated NOINC jump to +2 with no RET: captured to the EXIT (index 3)");
    }
    check(capture_end({kNop, kExit, kNop}) == 1, "no branches: captured to the first EXIT");
    // C10: @UP0 ULEA UR18, UP1, ... (an OoT program's, +0x3e10): lowers, its carry merged into R239 by the output_mask
    // epilogue, not by the unconditional merge that took the guest P5 under a false guard
    {
        const auto has = [](const auto& r, unsigned long long word) {
            return std::find(r.instructions.begin(), r.instructions.end(), word) != r.instructions.end();
        };
        const unsigned long long merge_p5 = 0x38e8000000070000ULL | 239 | (239ULL << 8) | (0x20ULL << 20);
        const unsigned long long save_p5_r253 = 0x38e8000000070000ULL | 253 | (255ULL << 8) | (0x20ULL << 20);
        const unsigned long long save_p5_r254 = 0x38e8000000070000ULL | 254 | (255ULL << 8) | (0x20ULL << 20);
        for (const unsigned a : {0x22u, 0x02u}) {   // (UR34, the OoT program's, and UR2)
            const Instruction ulea{{0x00120291u | a << 24, 0x12, 0x0f82103f, 0x000fc800}};
            const auto r = Shader::Ampere::LowerToMaxwell(std::vector<Instruction>{ulea, kExit});
            std::snprintf(what, sizeof what, "@UP0 ULEA UR18, UP1, UR%u: lowers, UP1 via R254 and the epilogue merge", a == 0x22 ? 34 : 2);
            check(r && has(r, save_p5_r254) && has(r, merge_p5) && !has(r, save_p5_r253), what);
            if (!r) std::printf("      (%s at +%#x)\n", std::string(r.error).c_str(), r.error_offset);
        }
        const auto same_slot = Shader::Ampere::LowerToMaxwell(
            std::vector<Instruction>{{{0x02121291u, 0x12, 0x0f82103f, 0x000fc800}}, kExit});
        check(!same_slot, "@UP1 ULEA with its carry in UP1 is rejected (the guard's slot)");
        const auto unguarded = Shader::Ampere::LowerToMaxwell(
            std::vector<Instruction>{{{0x02127291u, 0x12, 0x0f82103f, 0x000fc800}}, kExit});
        check(unguarded && has(unguarded, save_p5_r253) && has(unguarded, merge_p5),
              "unguarded ULEA with a carry out keeps its own save and merge");
    }
    std::printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: lower_test <folder> [samples] | lower_test --self\n"); return 2; }
    if (std::string(argv[1]) == "--self") return self_test();
    const int samples = argc > 2 ? std::atoi(argv[2]) : 3;
    struct Group { int n = 0; std::vector<std::string> examples; };
    std::map<std::string, Group> groups;
    int total = 0, ok = 0;
    for (const auto& e : std::filesystem::directory_iterator(argv[1])) {
        if (e.path().extension() != ".bin") continue;
        FILE* f = std::fopen(e.path().string().c_str(), "rb");
        if (!f) continue;
        std::vector<Shader::Ampere::Instruction> code;
        Shader::Ampere::Instruction in;
        while (std::fread(&in, sizeof in, 1, f) == 1) code.push_back(in);
        std::fclose(f);
        total++;
        const auto r = Shader::Ampere::LowerToMaxwell(code);
        if (r) { ok++; if (std::getenv("LOWER_TEST_LIST_OK")) std::printf("ok %s\n", e.path().filename().string().c_str()); continue; }
        const unsigned at = r.error_offset / 16;
        const unsigned op = at < code.size() ? code[at].Bits(0, 12) : 0xFFFFFFFFu;
        char key[200];
        std::snprintf(key, sizeof key, "%s (op %#x)", std::string(r.error).c_str(), op);
        Group& g = groups[key];
        g.n++;
        if ((int)g.examples.size() < samples && at < code.size()) {
            char ex[200];
            const auto& w = code[at].words;
            std::snprintf(ex, sizeof ex, "%s +%#x: %08x %08x %08x %08x", e.path().filename().string().c_str(), r.error_offset,
                          w[0], w[1], w[2], w[3]);
            g.examples.push_back(ex);
        }
    }
    std::printf("%d programs, %d lower, %d fail\n", total, ok, total - ok);
    std::vector<std::pair<int, std::string>> order;
    for (auto& [k, g] : groups) order.push_back({-g.n, k});
    std::sort(order.begin(), order.end());
    for (auto& [n, k] : order) {
        std::printf("%5d  %s\n", -n, k.c_str());
        for (auto& ex : groups[k].examples) std::printf("         %s\n", ex.c_str());
    }
    return 0;
}
