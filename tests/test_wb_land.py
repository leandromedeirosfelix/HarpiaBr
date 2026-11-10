"""CPU check of the write-back landing's three-way rule (vk_memory.cpp land_bytes) and where landings are applied.

A write-back on a page the CPU also writes lands on the CPU: a byte takes the GPU's value where the GPU changed it (after !=
before) and the CPU did not (guest == before); a byte both changed keeps the CPU's.

python -I -B tests/test_wb_land.py [--cxx clang++]
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def block(text, marker):
    start = text.index(marker)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default='g++')
    parser.add_argument('--source', type=Path, default=ROOT / 'gpu/vk/vk_memory.cpp')
    parser.add_argument('--replay', type=Path, default=ROOT / 'gpu/vk/vk_replay.cpp')
    args = parser.parse_args()
    text = args.source.read_text(encoding='utf-8')
    # (landed before the captures copy pages, at each submission's start, and before the CPU reads GPU results)
    cap = text[text.index('static thread_local std::vector<uint32_t> shared;'):]
    cap = cap[:cap.index('c->pages.assign(')]
    assert 'memory_apply_landings(' in cap, 'a capture no longer lands the write-backs it waited for'
    assert 'memory_apply_landings(mem_completed)' in block(text, 'void memory_begin_submission() {')
    replay = args.replay.read_text(encoding='utf-8')
    assert 'memory_apply_landings(completed())' in replay, 'the readback no longer lands the write-backs it waited for'
    snap = block(text, 'void land_snapshot(const Pool& p, uint64_t off, uint64_t size) {')
    assert 'rb_page(' in snap, 'readback pages (refreshed from guest memory after the copies before) must stay on the GPU path'
    helper = block(text, 'std::pair<uint64_t, uint64_t> land_bytes(')
    fixture = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>
#define CHECK(ok) do { if (!(ok)) { std::fprintf(stderr, "failed line %d\n", __LINE__); std::exit(1); } } while (0)
''' + helper + r'''
int main() {
    // every combination of before / after / guest over 3 values, byte by byte against the rule
    for (int b = 0; b < 3; b++)
        for (int a = 0; a < 3; a++)
            for (int g = 0; g < 3; g++) {
                uint8_t before = (uint8_t)b, after = (uint8_t)a, guest = (uint8_t)g;
                const auto [wrote, kept] = land_bytes(&guest, &before, &after, 1);
                const bool gpu = a != b, cpu = g != b;
                CHECK(guest == (gpu && !cpu ? a : g));
                CHECK(wrote == (gpu && !cpu ? 1u : 0u));
                CHECK(kept == (gpu && cpu ? 1u : 0u));
            }
    {   // a GPU particle record beside the CPU's parameters: the record lands, the CPU's new parameter stays
        uint8_t before[8] = {1, 1, 1, 1, 7, 7, 7, 7};   // (the mirror at the sync: the old record, the old parameter)
        uint8_t after[8] = {2, 2, 2, 2, 7, 7, 7, 7};    // (the GPU wrote the record)
        uint8_t guest[8] = {1, 1, 1, 1, 9, 9, 9, 9};    // (the CPU wrote the parameter for its next frame)
        const auto [wrote, kept] = land_bytes(guest, before, after, 8);
        const uint8_t want[8] = {2, 2, 2, 2, 9, 9, 9, 9};
        for (int i = 0; i < 8; i++) CHECK(guest[i] == want[i]);
        CHECK(wrote == 4 && kept == 0);
    }
    {   // nothing changed: nothing written
        uint8_t before[4] = {5, 6, 7, 8}, after[4] = {5, 6, 7, 8}, guest[4] = {9, 6, 7, 8};
        const auto [wrote, kept] = land_bytes(guest, before, after, 4);
        CHECK(wrote == 0 && kept == 0 && guest[0] == 9);
    }
    std::puts("PASS: a write-back byte lands where the GPU changed it and the CPU did not; the CPU's own writes stay");
}
'''
    with tempfile.TemporaryDirectory(prefix='wb-land-') as tmp:
        cpp = Path(tmp) / 'fixture.cpp'
        exe = Path(tmp) / 'fixture.exe'
        cpp.write_text(fixture, encoding='utf-8')
        subprocess.run([args.cxx, '-std=c++20', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    main()
