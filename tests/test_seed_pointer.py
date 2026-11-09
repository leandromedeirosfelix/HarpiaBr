"""A seed program (gpu/vk/vk_shader.cpp seed_program) is translated against a blank NvnState: the storage buffer pass's
pointer question (Env::IsCbufStorageBufferPointer, upstream afbb2d5) has no live state to read there. Such a program must
not be kept in the program cache (play would pick its blank-state variant), and the cache's stamp says so.

python -I -B tests/test_seed_pointer.py
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def body(text, head):
    start = text.index(head)
    return text[start:text.index("\n}\n", start)]


def main():
    src = (ROOT / "gpu" / "vk" / "vk_shader.cpp").read_text(encoding="utf-8")
    ask = body(src, "    bool IsCbufStorageBufferPointer(u32 index, u32 offset) override {")
    if "if (seed_translating) return seed_asked_pointer = true, false;" not in ask:
        raise SystemExit("a seed translation answers the storage buffer pointer question from the blank state")
    if ask.index("seed_translating") > ask.index("ReadCbufValue"):
        raise SystemExit("the blank state's words are read before the seed check")
    if "if (seed_translating && seed_asked_pointer) return;" not in body(src, "void program_cache_save("):
        raise SystemExit("program_cache_save keeps a seed program the blank state could not answer for")
    seed = body(src, "std::unique_ptr<Program> seed_program(")
    if "Seeding() { seed_translating = true, seed_asked_pointer = false; }" not in seed:
        raise SystemExit("seed_program does not mark its translation as a seed's")
    if "keep_rules = 1" not in body(src, "uint64_t disk_stamp() {"):
        raise SystemExit("the program cache's stamp does not carry the keep rule: old blank-state variants stay")
    print("PASS: seed programs with a pointer question are not kept")


if __name__ == "__main__":
    main()
