"""(local: Harpia) HarpiaSetup.exe, the offline installer (builder/setup/installer.c), from a Harpia Builder package folder.

    python -I scripts/make_harpia_setup.py <package folder> <tag> <out folder>

1. scripts/make_setup_parts.py splits the package into its part zips and harpia-setup.txt (no llvm-mingw).
2. NOTICE.txt: what the installer's first page shows (the notices below, then LICENSE).
3. installer.c is compiled with the build's own clang and the Windows SDK (as nx2/build.py; rc.exe for its icon and manifest),
   with the list's SHA-256 compiled in, so the installer trusts only that list, and the list names each part's SHA-256.
4. <out>/HarpiaSetup.exe: the program, then the parts, the list and the notice, a table of them and a trailer
   (installer.c payload_open). NVIDIA's DLSS/Streamline libraries are never in it (make_setup_parts.py leaves nvngx*/sl.* out).
Needs Windows, the emsdk the build uses (EMSDK) and the Windows SDK. Prints each file's size and SHA-256.
"""
import glob
import hashlib
import os
import shutil
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SETUP = os.path.join(ROOT, "builder", "setup")
sys.path[:0] = [os.path.join(ROOT, "nx2"), os.path.join(ROOT, "third_party", "a64-kit", "tools")]
MAGIC = b"HARPIAPL"
GITHUB_LIMIT = 2 * 1024 ** 3   # (one release asset)

# (one line a paragraph: the installer's box wraps them; LICENSE follows as it is)
NOTICE = """Harpia — Ocarina of Time no PC / on PC

PORTUGUÊS
O Harpia é software livre (GNU GPL-3.0), baseado no Hookshot e nos projetos que ele credita. É um projeto de fãs, sem nenhuma ligação com a Nintendo. Este instalador traz só o Harpia Builder e suas ferramentas: nenhum arquivo do jogo vem junto. O Builder monta o jogo no seu PC a partir da SUA cópia (.nup) — sem ela, não há jogo.
Nada é baixado nem enviado, exceto a etapa opcional do DLSS: o Harpia Builder só baixa os arquivos oficiais da NVIDIA se você escolher "Ativar NVIDIA DLSS" e aceitar os termos da NVIDIA. Sem telemetria.
As bibliotecas da NVIDIA (DLSS / Streamline) NÃO vêm neste instalador nem no código do Harpia; o jogo funciona sem elas.
Para montar o jogo, o PC precisa das Ferramentas de Build do Visual Studio (C++) e do Windows SDK, gratuitas da Microsoft: a Verificação do PC do Builder diz se faltam.

ENGLISH
Harpia is free software (GNU GPL-3.0), based on Hookshot and the projects it credits. It is a fan project, not affiliated with Nintendo. This installer carries only the Harpia Builder and its tools: no game files. The Builder builds the game on your PC from YOUR own copy (.nup) — without it there is no game.
Nothing is downloaded or sent, except the optional DLSS step: the Harpia Builder downloads NVIDIA's official files only if you choose "Enable NVIDIA DLSS" and accept NVIDIA's terms. No telemetry.
NVIDIA's libraries (DLSS / Streamline) are NOT in this installer nor in Harpia's code; the game works without them.
To build the game the PC needs Microsoft's free Visual Studio Build Tools (C++) and the Windows SDK: the Builder's PC check says if they are missing.

Incluídos, cada um sob a própria licença (os arquivos de licença ficam na pasta de instalação) / Included, each under its own licence (their licence files are in the install folder):
- .NET runtime (MIT), Python (PSF License), the Emscripten SDK's LLVM/clang (Apache-2.0 with LLVM exceptions), CPM packages (FFmpeg, Vulkan headers, SPIRV headers, Vulkan Memory Allocator: their own licences), wgpu-native (MIT / Apache-2.0), SDL2 (zlib), the Inter font (SIL Open Font License 1.1), NVIDIA Streamline's headers (MIT; headers only, used at build time).

----------------------------------------------------------------------------------------------------------------------
"""


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while b := f.read(1 << 20):
            h.update(b)
    return h.hexdigest()


def check(path, listing, parts_dir):
    """The file read back as installer.c reads it: the trailer, the table, each entry's bytes against its source, and the list's
    SHA-256 compiled into the program"""
    with open(path, "rb") as f:
        data = f.read()
    stub, table, count, version, magic = struct.unpack("<QQII8s", data[-32:])
    assert magic == MAGIC and version == 1 and 0 < stub <= table, "bad trailer"
    for i in range(count):
        e = data[table + 128 * i: table + 128 * (i + 1)]
        name = e[:112].rstrip(b"\0").decode()
        off, size = struct.unpack("<QQ", e[112:])
        with open(os.path.join(parts_dir, name), "rb") as src:
            assert data[off:off + size] == src.read(), "entry %s differs from its source" % name
    assert sha256(listing).encode() in data[:stub], "the list's SHA-256 is not in the program"


def compile_installer(list_sha, work):
    import make_game   # noqa: E402  (check_windows_sdk: INCLUDE/LIB of the Windows SDK and Visual Studio, as the game's build)
    import toolchain   # noqa: E402
    make_game.check_windows_sdk()
    rcs = sorted(glob.glob(os.path.join(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"),
                                        "Windows Kits", "10", "bin", "*", "x64", "rc.exe")))
    if not rcs:
        sys.exit("rc.exe (the Windows SDK's resource compiler) was not found")
    res = os.path.join(work, "installer.res")
    subprocess.run([rcs[-1], "/nologo", "/fo", res, os.path.join(SETUP, "installer.rc")], cwd=SETUP, check=True)
    stub = os.path.join(work, "installer-stub.exe")
    cmd = ([toolchain.need(toolchain.find("clang"))] + toolchain.native_target() + toolchain.native_link() +
           ["-O2", '-DHARPIA_OFFLINE_LIST_SHA256="%s"' % list_sha, "-Wno-unused-function",
            os.path.join(SETUP, "installer.c"), res, "-o", stub,
            toolchain.link_opt("/subsystem:windows"), toolchain.link_opt("/entry:wWinMainCRTStartup")])
    subprocess.run(cmd, check=True)
    return stub


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    package, out, tag = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[3]), sys.argv[2]
    if not tag or any(c in tag for c in ":/\\") or ".." in tag:   # (installer.c refuses a list whose tag is a path)
        sys.exit("the tag is a name, such as v1.0.0: " + tag)
    parts_dir = os.path.join(out, "parts")
    if os.path.isdir(parts_dir):
        shutil.rmtree(parts_dir)
    os.makedirs(parts_dir)
    subprocess.run([sys.executable, "-I", os.path.join(ROOT, "scripts", "make_setup_parts.py"), package, tag, parts_dir,
                    "--base", "offline"], check=True)
    listing = os.path.join(parts_dir, "harpia-setup.txt")
    with open(listing, encoding="utf-8") as f:
        text = f.read()
    if "llvm-mingw" in text:
        sys.exit("the list names llvm-mingw: Harpia's release has none")
    zips = [line.split()[6].rsplit("/", 1)[-1] for line in text.splitlines() if line.startswith("part ")]
    notice = os.path.join(parts_dir, "NOTICE.txt")
    with open(os.path.join(ROOT, "LICENSE"), encoding="utf-8") as f:
        licence = f.read()
    with open(notice, "w", encoding="utf-8", newline="\n") as f:
        f.write(NOTICE + licence)
    stub = compile_installer(sha256(listing), out)
    # (written under a name that is not an .exe and renamed only once complete and checked: a half-written installer never sits
    # under its final name, where someone could start it)
    setup = os.path.join(out, "HarpiaSetup.exe")
    partial = setup + ".partial"
    entries = []
    with open(partial, "wb") as o:
        with open(stub, "rb") as f:
            o.write(f.read())
        stub_size = o.tell()
        for name in zips + ["harpia-setup.txt", "NOTICE.txt"]:
            at = o.tell()
            with open(os.path.join(parts_dir, name), "rb") as f:
                shutil.copyfileobj(f, o, 1 << 20)
            entries.append((name, at, o.tell() - at))
        table = o.tell()
        for name, at, size in entries:   # (installer.c Entry: char[112], u64 off, u64 size)
            raw = name.encode("utf-8")
            if len(raw) >= 112:
                sys.exit("name too long for the table: " + name)
            o.write(raw.ljust(112, b"\0") + struct.pack("<QQ", at, size))
        o.write(struct.pack("<QQII", stub_size, table, len(entries), 1) + MAGIC)   # (installer.c Trailer)
        total = o.tell()
    # shortcut: one file only; a release past GitHub's 2 GB asset limit would need the parts split out (setup.c's offline zip)
    if total >= GITHUB_LIMIT:
        os.remove(partial)
        sys.exit("HarpiaSetup.exe is %.2f GB: over GitHub's 2 GB limit for one asset" % (total / 1024 ** 3))
    check(partial, listing, parts_dir)
    os.replace(partial, setup)
    digest = sha256(setup)
    print("HarpiaSetup.exe  %d bytes (%.0f MB)  sha256 %s" % (total, total / 1048576, digest))
    print("  installer program %d KB; parts: %s" % (stub_size >> 10, ", ".join("%s %d MB" % (n, s >> 20) for n, _, s in entries[:-2])))
    with open(setup + ".sha256", "w", newline="\n") as f:
        f.write("%s  HarpiaSetup.exe\n" % digest)


if __name__ == "__main__":
    main()
