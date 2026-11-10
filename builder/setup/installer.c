/* (local: Harpia) HarpiaSetup.exe: Harpia's offline installer. One file: this program, then the release's parts appended after it
   by scripts/make_harpia_setup.py (the Harpia Builder, its .NET runtime and build tools, harpia-setup.txt naming them, and the
   licence and notices). Nothing is downloaded: HarpiaBr's releases need a login, so the online setup (setup.c) cannot fetch them.
   It shows the licence and notices, asks for a folder (default %LOCALAPPDATA%\Harpia), checks the list against the SHA-256 compiled
   in (-DHARPIA_OFFLINE_LIST_SHA256) and each part against its own in the list, unpacks them with Windows' tar.exe as setup.c does,
   adds Start-menu and desktop shortcuts and an entry in Windows' installed apps (current user, no admin), and opens the builder.
   The install folder gets this program without the parts, as HarpiaSetup.exe: "HarpiaSetup.exe --uninstall" removes the install,
   its shortcuts and its entry, and, if the player says so, the builder's build files. Games built with it are never touched.
   For scripted tests: --no-shortcuts (both shortcut boxes start unticked), --no-launch (the builder is not opened at the end).
   setup.c's checksum, folder and unpacking code is reused (included below, without its own wWinMain).
   --no-launch: the builder is not opened at the end (tests). */
#define SETUP_NO_MAIN
#include "setup.c"
#include <dwmapi.h>
#include <uxtheme.h>
#include <psapi.h>
#pragma comment(lib, "winhttp")
#pragma comment(lib, "bcrypt")
#pragma comment(lib, "comctl32")
#pragma comment(lib, "shell32")
#pragma comment(lib, "ole32")
#pragma comment(lib, "uuid")
#pragma comment(lib, "advapi32")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "user32")
#pragma comment(lib, "dwmapi")
#pragma comment(lib, "uxtheme")

#ifndef HARPIA_OFFLINE_LIST_SHA256
#error "build with -DHARPIA_OFFLINE_LIST_SHA256=\"<harpia-setup.txt's sha256>\" (scripts/make_harpia_setup.py)"
#endif

/* ---- the parts after the program: entries, a table of them, then a trailer at the very end ---- */
#define PAYLOAD_MAGIC "HARPIAPL"
typedef struct { char name[112]; unsigned long long off, size; } Entry;                                  /* 128 bytes */
typedef struct { unsigned long long stub, table; unsigned int count, version; char magic[8]; } Trailer;   /* 32 bytes */
static Entry g_ent[64];
static int g_nent;
static unsigned long long g_stub;   /* (the program's own size: what the uninstaller copy keeps) */
static wchar_t g_self[MAX_PATH];

static int payload_open(void) {
    HANDLE f = CreateFileW(g_self, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER size, at;
    Trailer t;
    DWORD n;
    int ok = 0;
    GetFileSizeEx(f, &size);
    at.QuadPart = size.QuadPart - (LONGLONG)sizeof t;
    if (at.QuadPart > 0 && SetFilePointerEx(f, at, NULL, FILE_BEGIN) && ReadFile(f, &t, sizeof t, &n, NULL) && n == sizeof t &&
        !memcmp(t.magic, PAYLOAD_MAGIC, 8) && t.count <= 64 && t.table + (unsigned long long)t.count * sizeof(Entry) <= (unsigned long long)at.QuadPart) {
        LARGE_INTEGER tab;
        tab.QuadPart = (LONGLONG)t.table;
        ok = SetFilePointerEx(f, tab, NULL, FILE_BEGIN) && ReadFile(f, g_ent, t.count * (DWORD)sizeof(Entry), &n, NULL) &&
             n == t.count * sizeof(Entry);
        for (unsigned i = 0; ok && i < t.count; i++)   /* (every entry between the program and the table) */
            g_ent[i].name[111] = 0, ok = g_ent[i].off >= t.stub && g_ent[i].off + g_ent[i].size <= t.table;
        g_nent = ok ? (int)t.count : 0, g_stub = t.stub;
    }
    CloseHandle(f);
    return ok;
}

static const Entry *entry(const char *name) {
    for (int i = 0; i < g_nent; i++)
        if (!strcmp(g_ent[i].name, name)) return &g_ent[i];
    return NULL;
}

/* [off, off + size) of this file into a new file; progress through g_percent (done of total bytes; the copy is half a part) */
static int copy_range(unsigned long long off, unsigned long long size, const wchar_t *to, unsigned long long done, unsigned long long total) {
    HANDLE in = CreateFileW(g_self, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    HANDLE out = CreateFileW(to, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    int ok = in != INVALID_HANDLE_VALUE && out != INVALID_HANDLE_VALUE;
    LARGE_INTEGER at;
    at.QuadPart = (LONGLONG)off;
    ok = ok && SetFilePointerEx(in, at, NULL, FILE_BEGIN);
    static unsigned char buf[1 << 20];
    for (unsigned long long left = size; ok && left;) {
        DWORD want = left > sizeof buf ? (DWORD)sizeof buf : (DWORD)left, n = 0, wrote = 0;
        ok = ReadFile(in, buf, want, &n, NULL) && n == want && WriteFile(out, buf, n, &wrote, NULL) && wrote == n;
        left -= n;
        if (total) {
            InterlockedExchange(&g_percent, (LONG)((done + (size - left) / 2) * 100 / total));
            PostMessageW(g_win, WM_STATUS, 0, 0);
        }
    }
    if (in != INVALID_HANDLE_VALUE) CloseHandle(in);
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    if (!ok) DeleteFileW(to);
    return ok;
}

static int entry_mem(const char *name, Mem *m) {
    const Entry *e = entry(name);
    if (!e || e->size > (64u << 20)) return 0;
    HANDLE f = CreateFileW(g_self, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER at;
    at.QuadPart = (LONGLONG)e->off;
    char *p = malloc((size_t)e->size + 1);
    DWORD n = 0;
    int ok = p && SetFilePointerEx(f, at, NULL, FILE_BEGIN) && ReadFile(f, p, (DWORD)e->size, &n, NULL) && n == e->size;
    CloseHandle(f);
    if (!ok) { free(p); return 0; }
    p[e->size] = 0;
    *m = (Mem){p, (size_t)e->size, (size_t)e->size + 1};
    return 1;
}

/* ---- the words: pt-BR, whatever Windows' language (HarpiaBR is a pt-BR project, as the builder's own default); the English is
   kept for pick_language, unused while the builder's PT | EN switch is the only language choice ---- */
static int g_pt = 1;
#define L(pt, en) (g_pt ? L"" pt : L"" en)   /* (L"" glued to each: wide literals from the plain ones) */
static void pick_language(void) {
    static const WORD en[] = {LANG_ENGLISH, LANG_FRENCH, LANG_GERMAN, LANG_SPANISH, LANG_ITALIAN, LANG_DUTCH, LANG_POLISH, LANG_RUSSIAN,
                              LANG_JAPANESE, LANG_CHINESE, LANG_KOREAN};
    const WORD lang = PRIMARYLANGID(GetUserDefaultUILanguage());
    for (size_t i = 0; i < sizeof en / sizeof en[0]; i++)
        if (lang == en[i]) g_pt = 0;
}

/* ---- the registry (current user: no admin) ---- */
#define KEY_APP L"Software\\Harpia"
#define KEY_UNINSTALL L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Harpia"
static void reg_str(HKEY k, const wchar_t *name, const wchar_t *v) {
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)v, (DWORD)((wcslen(v) + 1) * sizeof(wchar_t)));
}
static void reg_dword(HKEY k, const wchar_t *name, DWORD v) { RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE *)&v, sizeof v); }
static int reg_get(const wchar_t *key, const wchar_t *name, wchar_t *out, DWORD chars) {
    DWORD bytes = chars * sizeof(wchar_t);
    out[0] = 0;
    return RegGetValueW(HKEY_CURRENT_USER, key, name, RRF_RT_REG_SZ, NULL, out, &bytes) == ERROR_SUCCESS && out[0];
}

static unsigned long long dir_size(const wchar_t *dir) {
    wchar_t pat[MAX_PATH], p[MAX_PATH];
    WIN32_FIND_DATAW fd;
    unsigned long long n = 0;
    swprintf(pat, MAX_PATH, L"%ls\\*", dir);
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..") || (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        swprintf(p, MAX_PATH, L"%ls\\%ls", dir, fd.cFileName);
        n += fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? dir_size(p) : ((unsigned long long)fd.nFileSizeHigh << 32 | fd.nFileSizeLow);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return n;
}

static int make_link(const wchar_t *lnk, const wchar_t *target, const wchar_t *args, const wchar_t *dir, const wchar_t *desc) {
    IShellLinkW *sl;
    IPersistFile *pf;
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl))) return 0;
    sl->lpVtbl->SetPath(sl, target);
    if (args) sl->lpVtbl->SetArguments(sl, args);
    sl->lpVtbl->SetWorkingDirectory(sl, dir);
    sl->lpVtbl->SetIconLocation(sl, target, 0);
    sl->lpVtbl->SetDescription(sl, desc);
    HRESULT hr = sl->lpVtbl->QueryInterface(sl, &IID_IPersistFile, (void **)&pf);
    if (SUCCEEDED(hr)) hr = pf->lpVtbl->Save(pf, lnk, TRUE), pf->lpVtbl->Release(pf);
    sl->lpVtbl->Release(sl);
    return SUCCEEDED(hr);
}

static void link_paths(wchar_t *desk, wchar_t *menu_dir) {
    wchar_t d[MAX_PATH] = L"", p[MAX_PATH] = L"";
    SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, d);
    SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, p);
    swprintf(desk, MAX_PATH, L"%ls\\Harpia Builder.lnk", d);
    swprintf(menu_dir, MAX_PATH, L"%ls\\Harpia", p);
}

/* ---- the install (a thread; status() and fail() from setup.c report to the window) ---- */
static wchar_t g_dir[MAX_PATH];
static int g_desktop = 1, g_menu = 1;
static volatile LONG g_installing;
static Part g_parts[MAX_PARTS];
static int g_nparts;
static char g_tag[128], g_run[260];
static Mem g_list, g_notice;

static int parse_list(void) {
    char *copy = malloc(g_list.n + 1);
    if (!copy) return 0;
    memcpy(copy, g_list.p, g_list.n), copy[g_list.n] = 0;
    for (char *line = strtok(copy, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        if (!strncmp(line, "tag ", 4)) sscanf(line + 4, "%127s", g_tag);
        else if (!strncmp(line, "run ", 4)) sscanf(line + 4, "%259s", g_run);
        else if (!strncmp(line, "part ", 5) && g_nparts < MAX_PARTS) {
            Part *p = &g_parts[g_nparts];
            if (sscanf(line + 5, "%64s %llu %259s %259s %259s %1023s", p->sha, &p->size, p->dest, p->from, p->to, p->url) == 6) g_nparts++;
        }
    }
    free(copy);
    int ok = g_tag[0] && g_run[0] && g_nparts && inside(g_run) && inside(g_tag);
    for (int i = 0; i < g_nparts; i++) ok = ok && inside(g_parts[i].dest) && inside(g_parts[i].from) && inside(g_parts[i].to);
    return ok;
}

static unsigned long long parts_bytes(void) {
    unsigned long long n = 0;
    for (int i = 0; i < g_nparts; i++) n += g_parts[i].size;
    return n;
}

static DWORD WINAPI install_work(LPVOID unused) {
    (void)unused;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    wchar_t versions[MAX_PATH], downloads[MAX_PATH], install[MAX_PATH], final_dir[MAX_PATH], marker[MAX_PATH], exe[MAX_PATH];
    wchar_t wtag[128], wrun[260];
    w(g_tag, wtag, 128), w(g_run, wrun, 260);
    swprintf(versions, MAX_PATH, L"%ls\\versions", g_dir);
    swprintf(downloads, MAX_PATH, L"%ls\\downloads", g_dir);
    swprintf(final_dir, MAX_PATH, L"%ls\\%ls", versions, wtag);
    swprintf(install, MAX_PATH, L"%ls\\%ls.part", versions, wtag);   /* (renamed once complete, as setup.c) */
    mkdirs(downloads);
    rmtree(install);   /* (a setup stopped half way: its .part goes; the version installed before stays until the end) */
    mkdirs(install);
    const unsigned long long total = parts_bytes();
    unsigned long long done = 0;
    for (int i = 0; i < g_nparts; i++) {
        Part *p = &g_parts[i];
        const char *slash = strrchr(p->url, '/');
        const Entry *e = entry(slash ? slash + 1 : p->url);
        if (!e || e->size != p->size) fail(L("A parte %d de %d falta neste instalador. Baixe o HarpiaSetup.exe de novo.",
                                             "Part %d of %d is missing from this installer. Download HarpiaSetup.exe again."), i + 1, g_nparts);
        wchar_t zip[MAX_PATH], wsha[65];
        w(p->sha, wsha, 65);
        swprintf(zip, MAX_PATH, L"%ls\\%ls.zip", downloads, wsha);
        status(L("Conferindo a parte %d de %d (%llu MB)...", "Checking part %d of %d (%llu MB)..."), i + 1, g_nparts, p->size >> 20);
        char have[65] = "";
        if (!copy_range(e->off, e->size, zip, done, total))
            fail(L("Não foi possível escrever em %ls. Há espaço livre?", "Could not write to %ls. Is there free space?"), downloads);
        if (!file_sha256(zip, have) || strcmp(have, p->sha))
            DeleteFileW(zip), fail(L("A parte %d de %d está corrompida (a soma SHA-256 não confere). Baixe o HarpiaSetup.exe de novo.",
                                     "Part %d of %d is damaged (its SHA-256 differs). Download HarpiaSetup.exe again."), i + 1, g_nparts);
        done += p->size / 2;
        status(L("Instalando a parte %d de %d...", "Installing part %d of %d..."), i + 1, g_nparts);
        InterlockedExchange(&g_percent, (LONG)(done * 100 / total));
        wchar_t dest[MAX_PATH], wdest[260], tarcmd[2 * MAX_PATH + 64], sys[MAX_PATH];
        w(p->dest, wdest, 260);
        if (!strcmp(p->dest, ".")) wcscpy(dest, install);
        else swprintf(dest, MAX_PATH, L"%ls\\%ls", install, wdest);
        for (wchar_t *q = dest; *q; q++) if (*q == L'/') *q = L'\\';
        mkdirs(dest);
        GetSystemDirectoryW(sys, MAX_PATH);
        swprintf(tarcmd, 2 * MAX_PATH + 64, L"\"%ls\\tar.exe\" -xf \"%ls\" -C \"%ls\"", sys, zip, dest);
        if (run_wait(tarcmd, dest) != 0)
            fail(L("Não foi possível desempacotar a parte %d de %d (tar.exe do Windows). Há espaço livre?",
                   "Unpacking part %d of %d failed (Windows' tar.exe). Is there enough free space?"), i + 1, g_nparts);
        if (strcmp(p->from, "-") && strcmp(p->to, "-")) {
            wchar_t from[MAX_PATH], to[MAX_PATH], wf[260], wt[260];
            w(p->from, wf, 260), w(p->to, wt, 260);
            swprintf(from, MAX_PATH, L"%ls\\%ls", dest, wf);
            swprintf(to, MAX_PATH, L"%ls\\%ls", dest, wt);
            if (GetFileAttributesW(to) == INVALID_FILE_ATTRIBUTES && !MoveFileW(from, to))
                fail(L("Não foi possível preparar a parte %d de %d.", "Could not set up part %d of %d."), i + 1, g_nparts);
        }
        DeleteFileW(zip);
        done += p->size - p->size / 2;
        InterlockedExchange(&g_percent, (LONG)(done * 100 / total));
    }
    swprintf(exe, MAX_PATH, L"%ls\\%ls", install, wrun);
    for (wchar_t *q = exe; *q; q++) if (*q == L'/') *q = L'\\';
    if (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES)
        fail(L("O Harpia Builder sumiu depois de desempacotado (%ls). Seu antivírus pode tê-lo removido: libere a pasta e instale de novo.",
               "The builder is missing after unpacking (%ls). Your antivirus may have removed it: allow the folder and install again."), exe);
    /* (the same version installed again: the one there is moved aside, the new one put in place, and the old one deleted only
       then; a builder still open from it keeps it in place, and nothing changes) */
    wchar_t old[MAX_PATH];
    swprintf(old, MAX_PATH, L"%ls.old", final_dir);
    rmtree(old);
    if (GetFileAttributesW(final_dir) != INVALID_FILE_ATTRIBUTES && !MoveFileW(final_dir, old))
        fail(L("Feche o Harpia Builder e instale de novo: ele está aberto a partir de %ls.",
               "Close the Harpia Builder and install again: it is open from %ls."), final_dir);
    if (!MoveFileW(install, final_dir)) {
        MoveFileW(old, final_dir);
        fail(L("Não foi possível concluir a instalação em %ls.", "Could not finish installing into %ls."), final_dir);
    }
    rmtree(old);
    swprintf(marker, MAX_PATH, L"%ls\\installed.txt", final_dir);
    write_small(marker, g_tag);
    prune(versions, downloads, g_tag, g_parts, g_nparts);   /* (an earlier version in this folder goes; current.txt names this one) */
    RemoveDirectoryW(downloads);
    swprintf(exe, MAX_PATH, L"%ls\\%ls", final_dir, wrun);
    for (wchar_t *q = exe; *q; q++) if (*q == L'/') *q = L'\\';
    wchar_t exe_dir[MAX_PATH];
    wcscpy(exe_dir, exe);
    *wcsrchr(exe_dir, L'\\') = 0;

    status(L("Criando atalhos...", "Creating shortcuts..."));
    wchar_t uninst[MAX_PATH], desk[MAX_PATH], menu[MAX_PATH], lnk[MAX_PATH];
    swprintf(uninst, MAX_PATH, L"%ls\\HarpiaSetup.exe", g_dir);
    if (!copy_range(0, g_stub, uninst, 0, 0)) fail(L("Não foi possível criar o desinstalador.", "Could not write the uninstaller."));
    link_paths(desk, menu);
    if (g_desktop) make_link(desk, exe, NULL, exe_dir, L"Harpia Builder");
    if (g_menu) {
        mkdirs(menu);
        swprintf(lnk, MAX_PATH, L"%ls\\Harpia Builder.lnk", menu);
        make_link(lnk, exe, NULL, exe_dir, L"Harpia Builder");
        swprintf(lnk, MAX_PATH, L"%ls\\%ls.lnk", menu, L("Desinstalar o Harpia", "Uninstall Harpia"));
        make_link(lnk, uninst, L"--uninstall", g_dir, L("Desinstalar o Harpia", "Uninstall Harpia"));
    }
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, KEY_APP, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) == ERROR_SUCCESS) {
        reg_str(k, L"InstallDir", g_dir), reg_str(k, L"Version", wtag);
        reg_dword(k, L"DesktopShortcut", (DWORD)g_desktop), reg_dword(k, L"StartMenu", (DWORD)g_menu);
        RegCloseKey(k);
    }
    if (RegCreateKeyExW(HKEY_CURRENT_USER, KEY_UNINSTALL, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) == ERROR_SUCCESS) {
        wchar_t cmd[MAX_PATH + 32], icon[MAX_PATH + 4];
        swprintf(cmd, MAX_PATH + 32, L"\"%ls\" --uninstall", uninst);
        swprintf(icon, MAX_PATH + 4, L"%ls,0", exe);
        reg_str(k, L"DisplayName", L"Harpia"), reg_str(k, L"DisplayVersion", wtag), reg_str(k, L"Publisher", L"Harpia (fan project, GPL-3.0)");
        reg_str(k, L"DisplayIcon", icon), reg_str(k, L"InstallLocation", g_dir), reg_str(k, L"UninstallString", cmd);
        reg_dword(k, L"NoModify", 1), reg_dword(k, L"NoRepair", 1), reg_dword(k, L"EstimatedSize", (DWORD)(dir_size(g_dir) >> 10));
        RegCloseKey(k);
    }
    status(L("Pronto. Abrindo o Harpia Builder...", "Done. Opening the Harpia Builder..."));
    InterlockedExchange(&g_percent, 100);
    PostMessageW(g_win, WM_STATUS, 0, 0);
    STARTUPINFOW si = {sizeof si};
    PROCESS_INFORMATION pi;
    wchar_t cmd[MAX_PATH + 4];
    swprintf(cmd, MAX_PATH + 4, L"\"%ls\"", exe);
    if (!wcsstr(g_cmdline, L"--no-launch") && CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, exe_dir, &si, &pi))
        CloseHandle(pi.hThread), CloseHandle(pi.hProcess);
    Sleep(1200);
    InterlockedExchange(&g_installing, 0);
    PostMessageW(g_win, WM_DONE, 0, 0);
    return 0;
}

/* ---- the window: gold on dark, the harpia, three pages (licence, folder, progress) ---- */
/* (the game's Insert menu palette, gpu/vk/vk_menu.cpp, as the builder's App.xaml) */
#define C_BG RGB(0x10, 0x0E, 0x14)
#define C_RAISED RGB(0x1A, 0x16, 0x1E)
#define C_LINE RGB(0x68, 0x52, 0x30)
#define C_INK RGB(0xEC, 0xE0, 0xC6)
#define C_SOFT RGB(0xB0, 0xA0, 0x84)
#define C_GOLD RGB(0xE8, 0xB8, 0x60)
#define C_GOLD_INK RGB(0x1A, 0x12, 0x06)
enum { ID_NOTICE = 100, ID_PATH, ID_BROWSE, ID_DESK, ID_MENU, ID_NOTE, ID_PRIMARY, ID_SECOND, ID_TITLE, ID_LABEL };
static HWND g_notice_box, g_label, g_path, g_browse, g_desk, g_menu_box, g_note, g_primary, g_second, g_title;
static HFONT g_font, g_bold, g_big;
static HBRUSH g_bg, g_raised;
static HICON g_logo;
static int g_page, g_dpi = 96;
static int S(int v) { return MulDiv(v, g_dpi, 96); }

static void show_page(int page) {
    g_page = page;
    const HWND p1[] = {g_label, g_path, g_browse, g_desk, g_menu_box, g_note};
    ShowWindow(g_notice_box, page == 0 ? SW_SHOW : SW_HIDE);
    for (size_t i = 0; i < sizeof p1 / sizeof p1[0]; i++) ShowWindow(p1[i], page == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_text, page == 2 ? SW_SHOW : SW_HIDE), ShowWindow(g_bar, page == 2 ? SW_SHOW : SW_HIDE);
    SetWindowTextW(g_title, page == 0 ? L("Licença e avisos", "Licence and notices") : page == 1 ? L("Onde instalar", "Where to install")
                                                                                     : L("Instalando", "Installing"));
    SetWindowTextW(g_primary, page == 0 ? L("Continuar", "Continue") : page == 1 ? L("Instalar", "Install") : L("Instalando...", "Installing..."));
    SetWindowTextW(g_second, page == 0 ? L("Cancelar", "Cancel") : L("Voltar", "Back"));
    ShowWindow(g_second, page == 2 ? SW_HIDE : SW_SHOW);
    EnableWindow(g_primary, page != 2);
    InvalidateRect(g_win, NULL, TRUE);
}

static void update_note(void) {
    wchar_t dir[MAX_PATH], root[4], text[700], more[80] = L"";
    GetWindowTextW(g_path, dir, MAX_PATH);
    swprintf(root, 4, L"%.3ls", dir);
    ULARGE_INTEGER free_bytes;
    if (GetDiskFreeSpaceExW(root, &free_bytes, NULL, NULL))
        swprintf(more, 80, L(" (%.0f GB livres em %ls)", " (%.0f GB free on %ls)"), free_bytes.QuadPart / 1073741824.0, root);
    swprintf(text, 700, L("Precisa de cerca de %.1f GB aqui%ls. Depois, o Harpia Builder verifica o PC e monta o jogo a partir do seu "
                          "próprio .nup (mais ~60 GB durante a montagem, na pasta de arquivos de montagem).",
                          "Needs about %.1f GB here%ls. Then the Harpia Builder checks your PC and builds the game from your own .nup "
                          "(another ~60 GB while building, in its build files folder)."),
             parts_bytes() * 2.6 / 1073741824.0, more);   /* (unpacked: about 2.5 times the parts) */
    SetWindowTextW(g_note, text);
}

static void browse(void) {
    IFileOpenDialog *d;
    if (FAILED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void **)&d))) return;
    DWORD o = 0;
    d->lpVtbl->GetOptions(d, &o);
    d->lpVtbl->SetOptions(d, o | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    d->lpVtbl->SetTitle(d, L("Escolha onde instalar o Harpia", "Choose where to install Harpia"));
    IShellItem *it;
    if (SUCCEEDED(d->lpVtbl->Show(d, g_win)) && SUCCEEDED(d->lpVtbl->GetResult(d, &it))) {
        wchar_t *path;
        if (SUCCEEDED(it->lpVtbl->GetDisplayName(it, SIGDN_FILESYSPATH, &path))) {
            wchar_t dir[MAX_PATH];
            const size_t n = wcslen(path);
            /* (the folder picked is where the Harpia folder goes, unless it is one already) */
            if (n >= 6 && !_wcsicmp(path + n - 6, L"Harpia")) swprintf(dir, MAX_PATH, L"%ls", path);
            else swprintf(dir, MAX_PATH, L"%ls%lsHarpia", path, n && path[n - 1] == L'\\' ? L"" : L"\\");
            SetWindowTextW(g_path, dir);
            update_note();
            CoTaskMemFree(path);
        }
        it->lpVtbl->Release(it);
    }
    d->lpVtbl->Release(d);
}

static int folder_empty_or_ours(const wchar_t *dir) {
    wchar_t pat[MAX_PATH], p[MAX_PATH];
    if (GetFileAttributesW(dir) == INVALID_FILE_ATTRIBUTES) return 1;
    swprintf(p, MAX_PATH, L"%ls\\HarpiaSetup.exe", dir);
    if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES) return 1;   /* (an earlier install: updated in place) */
    swprintf(p, MAX_PATH, L"%ls\\versions", dir);
    if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES) return 1;   /* (a first install stopped half way: carried on) */
    WIN32_FIND_DATAW fd;
    swprintf(pat, MAX_PATH, L"%ls\\*", dir);
    HANDLE h = FindFirstFileW(pat, &fd);
    int empty = 1;
    if (h != INVALID_HANDLE_VALUE) {
        do if (wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L"..")) empty = 0;
        while (empty && FindNextFileW(h, &fd));
        FindClose(h);
    }
    return empty;
}

static void start_install(void) {
    wchar_t dir[MAX_PATH];
    GetWindowTextW(g_path, dir, MAX_PATH);
    size_t n = wcslen(dir);
    while (n > 3 && dir[n - 1] == L'\\') dir[--n] = 0;
    /* (a folder of its own: never a drive's root, and never one already holding other files, which the uninstaller would leave
       half-removed) */
    if (n < 4 || !iswalpha(dir[0]) || dir[1] != L':' || dir[2] != L'\\') {
        MessageBoxW(g_win, L("Escolha uma pasta completa, como C:\\Jogos\\Harpia (não a raiz de um disco).",
                             "Choose a full folder path, such as C:\\Games\\Harpia (not a drive's root)."), L"Harpia", MB_ICONWARNING);
        return;
    }
    if (!folder_empty_or_ours(dir)) {
        MessageBoxW(g_win, L("Essa pasta já tem outros arquivos. Escolha uma pasta nova ou vazia para o Harpia.",
                             "That folder already holds other files. Choose a new or empty folder for Harpia."), L"Harpia", MB_ICONWARNING);
        return;
    }
    wchar_t root[4] = {dir[0], L':', L'\\', 0};
    ULARGE_INTEGER free_bytes;
    if (GetDiskFreeSpaceExW(root, &free_bytes, NULL, NULL) && free_bytes.QuadPart < parts_bytes() * 3) {
        MessageBoxW(g_win, L("Não há espaço livre suficiente nesse disco.", "There is not enough free space on that drive."), L"Harpia", MB_ICONWARNING);
        return;
    }
    wcscpy(g_dir, dir);
    g_desktop = (int)GetWindowLongPtrW(g_desk, GWLP_USERDATA), g_menu = (int)GetWindowLongPtrW(g_menu_box, GWLP_USERDATA);
    mkdirs(g_dir);
    InterlockedExchange(&g_installing, 1);
    show_page(2);
    status(L("Preparando...", "Getting ready..."));
    CloseHandle(CreateThread(NULL, 0, install_work, NULL, 0, NULL));
}

static void draw_button(const DRAWITEMSTRUCT *d) {
    const int primary = d->CtlID == ID_PRIMARY, check = d->CtlID == ID_DESK || d->CtlID == ID_MENU;
    const int disabled = d->itemState & ODS_DISABLED, pressed = d->itemState & ODS_SELECTED;
    wchar_t text[200];
    GetWindowTextW(d->hwndItem, text, 200);
    RECT r = d->rcItem;
    FillRect(d->hDC, &r, g_bg);
    SetBkMode(d->hDC, TRANSPARENT);
    SelectObject(d->hDC, primary ? g_bold : g_font);
    if (check) {   /* (a box with a gold tick, the label beside it) */
        const int on = (int)GetWindowLongPtrW(d->hwndItem, GWLP_USERDATA), b = S(18), y = (r.top + r.bottom - b) / 2;
        HBRUSH fill = CreateSolidBrush(on ? C_GOLD : C_RAISED);
        HPEN pen = CreatePen(PS_SOLID, 1, on ? C_GOLD : C_LINE);
        SelectObject(d->hDC, fill), SelectObject(d->hDC, pen);
        RoundRect(d->hDC, r.left, y, r.left + b, y + b, S(6), S(6));
        if (on) {
            HPEN tick = CreatePen(PS_SOLID, S(2), C_GOLD_INK);
            SelectObject(d->hDC, tick);
            MoveToEx(d->hDC, r.left + S(4), y + S(9), NULL), LineTo(d->hDC, r.left + S(8), y + S(13)), LineTo(d->hDC, r.left + S(14), y + S(5));
            SelectObject(d->hDC, GetStockObject(BLACK_PEN)), DeleteObject(tick);
        }
        SelectObject(d->hDC, GetStockObject(NULL_BRUSH)), SelectObject(d->hDC, GetStockObject(BLACK_PEN));
        DeleteObject(fill), DeleteObject(pen);
        RECT t = r;
        t.left += b + S(10);
        SetTextColor(d->hDC, C_INK);
        DrawTextW(d->hDC, text, -1, &t, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        if (d->itemState & ODS_FOCUS) {
            RECT f = t;
            DrawTextW(d->hDC, text, -1, &f, DT_SINGLELINE | DT_CALCRECT);
            f.top = r.top, f.bottom = r.bottom;
            DrawFocusRect(d->hDC, &f);
        }
        return;
    }
    COLORREF fill = primary ? (disabled ? RGB(0x58, 0x40, 0x1A) : pressed ? RGB(0xD9, 0xA8, 0x48) : C_GOLD) : (pressed ? C_LINE : C_RAISED);
    HBRUSH b = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, primary ? fill : C_LINE);
    SelectObject(d->hDC, b), SelectObject(d->hDC, pen);
    RoundRect(d->hDC, r.left, r.top, r.right, r.bottom, S(10), S(10));
    SelectObject(d->hDC, GetStockObject(NULL_BRUSH)), SelectObject(d->hDC, GetStockObject(BLACK_PEN));
    DeleteObject(b), DeleteObject(pen);
    SetTextColor(d->hDC, primary ? C_GOLD_INK : disabled ? C_SOFT : C_INK);
    DrawTextW(d->hDC, text, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    if (d->itemState & ODS_FOCUS) {
        RECT f = r;
        InflateRect(&f, -S(4), -S(4));
        DrawFocusRect(d->hDC, &f);
    }
}

/* (every step and a failure's words in %TEMP%\HarpiaSetup.log: a player's report, or a test's) */
static int g_failed;
static void log_line(const wchar_t *text) {
    static wchar_t last[512];
    if (!wcscmp(last, text)) return;
    wcsncpy(last, text, 511);
    wchar_t path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"HarpiaSetup.log");
    FILE *f = _wfopen(path, L"a, ccs=UTF-8");
    if (!f) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    fwprintf(f, L"%02d:%02d:%02d %ls\n", t.wHour, t.wMinute, t.wSecond, text);
    fclose(f);
}

static LRESULT CALLBACK inst_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_STATUS: {
        SetWindowTextW(g_text, g_status);
        log_line(g_status);
        const LONG p = g_percent;
        SendMessageW(g_bar, PBM_SETPOS, (WPARAM)(p < 0 ? 0 : p), 0);
        return 0;
    }
    case WM_DONE:
        if (wp) g_failed = 1, InterlockedExchange(&g_installing, 0), log_line(L"FAILED:"), log_line(g_failure);
        DestroyWindow(h);
        return 0;
    case WM_CLOSE:
        if (g_installing) return 0;   /* (half an install would be left: it finishes, or fails with its message) */
        DestroyWindow(h);
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        DrawIconEx(dc, S(28), S(22), g_logo, S(44), S(44), 0, NULL, DI_NORMAL);
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, g_big);
        SetTextColor(dc, C_GOLD);
        RECT t = {S(84), S(16), S(600), S(50)};
        DrawTextW(dc, L"Harpia", -1, &t, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
        SelectObject(dc, g_font);
        SetTextColor(dc, C_SOFT);
        RECT s = {S(84), S(48), S(660), S(70)};
        DrawTextW(dc, L("Instalador do Harpia Builder · Ocarina of Time no PC, a partir do seu próprio jogo",
                        "Harpia Builder installer · Ocarina of Time on PC, from your own copy of the game"), -1, &s,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        HPEN line = CreatePen(PS_SOLID, 1, RGB(0x3A, 0x30, 0x26));
        SelectObject(dc, line);
        MoveToEx(dc, S(28), S(84), NULL), LineTo(dc, S(652), S(84));
        SelectObject(dc, GetStockObject(BLACK_PEN)), DeleteObject(line);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        const HWND c = (HWND)lp;
        SetBkMode(dc, OPAQUE);
        SetBkColor(dc, c == g_notice_box ? C_RAISED : C_BG);
        SetTextColor(dc, c == g_note || c == g_text || c == g_label ? C_SOFT : C_INK);
        return (LRESULT)(c == g_notice_box ? g_raised : g_bg);
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetBkColor(dc, C_RAISED), SetTextColor(dc, C_INK);
        return (LRESULT)g_raised;
    }
    case WM_DRAWITEM: draw_button((const DRAWITEMSTRUCT *)lp); return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_PRIMARY:
            if (g_page == 0) show_page(1), update_note(), SetFocus(g_primary);
            else if (g_page == 1) start_install();
            return 0;
        case ID_SECOND:
            if (g_page == 0) DestroyWindow(h);
            else if (g_page == 1) show_page(0);
            return 0;
        case ID_BROWSE: browse(); return 0;
        case ID_DESK: case ID_MENU: {
            HWND c = (HWND)lp;
            SetWindowLongPtrW(c, GWLP_USERDATA, !GetWindowLongPtrW(c, GWLP_USERDATA));
            InvalidateRect(c, NULL, TRUE);
            return 0;
        }
        case ID_PATH:
            if (HIWORD(wp) == EN_CHANGE && g_page == 1) update_note();
            return 0;
        }
        break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static HWND child(const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int cx, int cy, int id, HINSTANCE inst) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | style, S(x), S(y), S(cx), S(cy), g_win, (HMENU)(INT_PTR)id, inst, NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    return c;
}

/* ---- --uninstall: run from a copy in %TEMP% (the one in the install folder must go too); removes what the registry names ---- */
static int any_running_from(const wchar_t *dir) {
    DWORD pids[4096], bytes;
    if (!EnumProcesses(pids, sizeof pids, &bytes)) return 0;
    const size_t n = wcslen(dir);
    for (DWORD i = 0; i < bytes / sizeof(DWORD); i++) {
        if (pids[i] == GetCurrentProcessId()) continue;
        HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pids[i]);
        if (!p) continue;
        wchar_t img[MAX_PATH];
        DWORD len = MAX_PATH;
        const int hit = QueryFullProcessImageNameW(p, 0, img, &len) && !_wcsnicmp(img, dir, n) && img[n] == L'\\';
        CloseHandle(p);
        if (hit) return 1;
    }
    return 0;
}

static int uninstall(void) {
    wchar_t dir[MAX_PATH];
    if (!reg_get(KEY_APP, L"InstallDir", dir, MAX_PATH) || wcslen(dir) < 4 || dir[1] != L':' || dir[2] != L'\\') {
        MessageBoxW(NULL, L("O Harpia não está instalado neste usuário.", "Harpia is not installed for this user."), L"Harpia", MB_ICONINFORMATION);
        return 1;
    }
    /* (--quiet: no questions, for scripts and tests: uninstalled, the build files kept unless --purge, nothing removed while the
       builder is open) */
    const int quiet = wcsstr(g_cmdline, L"--quiet") != NULL, purge_asked = wcsstr(g_cmdline, L"--purge") != NULL;
    if (!wcsstr(g_cmdline, L"--from-temp")) {
        wchar_t tmp[MAX_PATH], copy[MAX_PATH], cmd[MAX_PATH + 80];
        GetTempPathW(MAX_PATH, tmp);
        swprintf(copy, MAX_PATH, L"%lsHarpiaUninstall-%lu.exe", tmp, GetCurrentProcessId());
        if (!CopyFileW(g_self, copy, FALSE)) return 1;
        swprintf(cmd, MAX_PATH + 80, L"\"%ls\" --uninstall --from-temp%ls%ls", copy, quiet ? L" --quiet" : L"", purge_asked ? L" --purge" : L"");
        STARTUPINFOW si = {sizeof si};
        PROCESS_INFORMATION pi;
        if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, tmp, &si, &pi)) return 1;
        CloseHandle(pi.hThread), CloseHandle(pi.hProcess);
        return 0;
    }
    MoveFileExW(g_self, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);   /* (this temporary copy goes at the next restart) */
    wchar_t ask[1200];
    swprintf(ask, 1200, L("Desinstalar o Harpia de %ls?\n\nOs jogos que você montou não são apagados.",
                          "Uninstall Harpia from %ls?\n\nThe games you built are not removed."), dir);
    if (!quiet && MessageBoxW(NULL, ask, L"Harpia", MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
    while (any_running_from(dir))
        if (quiet || MessageBoxW(NULL, L("Feche o Harpia Builder e tente de novo.", "Close the Harpia Builder and try again."), L"Harpia",
                                 MB_RETRYCANCEL | MB_ICONWARNING) != IDRETRY) return 1;
    wchar_t base[MAX_PATH] = L"";
    int purge = quiet && purge_asked;
    if (SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, base) == S_OK) {
        wcscat(base, L"\\HarpiaBuilder");
        if (!quiet && GetFileAttributesW(base) != INVALID_FILE_ATTRIBUTES) {
            swprintf(ask, 1200, L("Apagar também os arquivos de montagem do Harpia Builder (%.1f GB em %ls)?\n\nSem eles, a próxima "
                                  "montagem leva o tempo todo de novo.",
                                  "Also delete the Harpia Builder's build files (%.1f GB in %ls)?\n\nWithout them, the next build takes "
                                  "the whole time again."), dir_size(base) / 1073741824.0, base);
            purge = MessageBoxW(NULL, ask, L"Harpia", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES;
        }
    }
    wchar_t desk[MAX_PATH], menu[MAX_PATH], p[MAX_PATH];
    link_paths(desk, menu);
    DWORD on = 0, size = sizeof on;
    if (RegGetValueW(HKEY_CURRENT_USER, KEY_APP, L"DesktopShortcut", RRF_RT_REG_DWORD, NULL, &on, &size) == ERROR_SUCCESS && on)
        DeleteFileW(desk);
    swprintf(p, MAX_PATH, L"%ls\\Harpia Builder.lnk", menu), DeleteFileW(p);
    swprintf(p, MAX_PATH, L"%ls\\Desinstalar o Harpia.lnk", menu), DeleteFileW(p);
    swprintf(p, MAX_PATH, L"%ls\\Uninstall Harpia.lnk", menu), DeleteFileW(p);
    RemoveDirectoryW(menu);   /* (only when empty) */
    swprintf(p, MAX_PATH, L"%ls\\versions", dir), rmtree(p);
    swprintf(p, MAX_PATH, L"%ls\\downloads", dir), rmtree(p);
    swprintf(p, MAX_PATH, L"%ls\\HarpiaSetup.exe", dir), DeleteFileW(p);
    RemoveDirectoryW(dir);   /* (only when empty: anything else the player put there stays) */
    if (purge) rmtree(base);
    RegDeleteTreeW(HKEY_CURRENT_USER, KEY_UNINSTALL);
    RegDeleteTreeW(HKEY_CURRENT_USER, KEY_APP);
    if (!quiet) MessageBoxW(NULL, L("O Harpia foi desinstalado.", "Harpia has been uninstalled."), L"Harpia", MB_ICONINFORMATION);
    return 0;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmdline, int show) {
    (void)prev;
    wcsncpy(g_cmdline, cmdline ? cmdline : L"", 2047);
    GetModuleFileNameW(NULL, g_self, MAX_PATH);
    (void)pick_language;   /* (pt-BR always: see g_pt) */
    if (wcsstr(g_cmdline, L"--uninstall")) return uninstall();
    char have[65] = "";
    if (!payload_open() || !entry_mem("harpia-setup.txt", &g_list) || !mem_sha256(g_list.p, g_list.n, have) ||
        strcmp(have, HARPIA_OFFLINE_LIST_SHA256) || !parse_list()) {
        MessageBoxW(NULL, L("Este instalador está incompleto ou corrompido. Baixe o HarpiaSetup.exe de novo.",
                            "This installer is incomplete or damaged. Download HarpiaSetup.exe again."), L"Harpia", MB_ICONERROR);
        return 1;
    }
    entry_mem("NOTICE.txt", &g_notice);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc = {sizeof icc, ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    HDC screen = GetDC(NULL);
    g_dpi = GetDeviceCaps(screen, LOGPIXELSY);
    ReleaseDC(NULL, screen);
    g_font = CreateFontW(-S(14), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_bold = CreateFontW(-S(14), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_big = CreateFontW(-S(26), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT title_font = CreateFontW(-S(18), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_bg = CreateSolidBrush(C_BG), g_raised = CreateSolidBrush(C_RAISED);
    g_logo = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, S(44), S(44), 0);
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = inst_proc, wc.hInstance = inst, wc.lpszClassName = L"HarpiaInstaller";
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = g_bg;
    RegisterClassW(&wc);
    RECT r = {0, 0, S(680), S(480)};
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&r, style, FALSE);
    const int ww = r.right - r.left, wh = r.bottom - r.top;
    wchar_t caption[200], wtag[128];
    w(g_tag, wtag, 128);
    swprintf(caption, 200, L("Instalar o Harpia %ls", "Install Harpia %ls"), wtag);
    g_win = CreateWindowW(L"HarpiaInstaller", caption, style, (GetSystemMetrics(SM_CXSCREEN) - ww) / 2,
                          (GetSystemMetrics(SM_CYSCREEN) - wh) / 2, ww, wh, NULL, NULL, inst, NULL);
    BOOL dark = TRUE;
    DwmSetWindowAttribute(g_win, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof dark);

    g_title = child(L"STATIC", L"", WS_VISIBLE, 28, 96, 624, 28, ID_TITLE, inst);
    SendMessageW(g_title, WM_SETFONT, (WPARAM)title_font, TRUE);
    /* page 0: the licence and notices the release carries */
    g_notice_box = child(L"EDIT", L"", WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 28, 132, 624, 268, ID_NOTICE, inst);
    SendMessageW(g_notice_box, EM_SETLIMITTEXT, 0x7FFFFFFE, 0);
    SendMessageW(g_notice_box, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(S(10), S(10)));
    if (g_notice.p) {   /* (UTF-8; \n -> \r\n for the edit box) */
        const int n = MultiByteToWideChar(CP_UTF8, 0, g_notice.p, (int)g_notice.n, NULL, 0);
        wchar_t *wide = malloc((size_t)(n + 1) * sizeof(wchar_t)), *crlf = malloc((size_t)(2 * n + 1) * sizeof(wchar_t));
        if (wide && crlf) {
            MultiByteToWideChar(CP_UTF8, 0, g_notice.p, (int)g_notice.n, wide, n);
            int j = 0;
            for (int i = 0; i < n; i++) {
                if (wide[i] == L'\n' && (i == 0 || wide[i - 1] != L'\r')) crlf[j++] = L'\r';
                crlf[j++] = wide[i];
            }
            crlf[j] = 0;
            SetWindowTextW(g_notice_box, crlf);
        }
        free(wide), free(crlf);
    }
    /* page 1: the folder (an earlier install's, else %LOCALAPPDATA%\Harpia) and the shortcuts */
    wchar_t dir[MAX_PATH] = L"";
    if (!reg_get(KEY_APP, L"InstallDir", dir, MAX_PATH)) {
        SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, dir);
        wcscat(dir, L"\\Harpia");
    }
    g_label = child(L"STATIC", L("Pasta de instalação", "Install folder"), 0, 28, 134, 400, 22, ID_LABEL, inst);
    g_path = child(L"EDIT", dir, WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 28, 160, 500, 32, ID_PATH, inst);
    SendMessageW(g_path, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(S(8), S(8)));
    g_browse = child(L"BUTTON", L("Procurar...", "Browse..."), BS_OWNERDRAW | WS_TABSTOP, 540, 160, 112, 32, ID_BROWSE, inst);
    g_desk = child(L"BUTTON", L("Atalho na área de trabalho", "Desktop shortcut"), BS_OWNERDRAW | WS_TABSTOP, 28, 212, 420, 26, ID_DESK, inst);
    g_menu_box = child(L"BUTTON", L("Atalho no menu Iniciar", "Start menu shortcut"), BS_OWNERDRAW | WS_TABSTOP, 28, 244, 420, 26, ID_MENU, inst);
    {   /* (--no-shortcuts, for scripted tests: both boxes start unticked, so a test install leaves nothing on the desktop) */
        const LONG_PTR on = wcsstr(g_cmdline, L"--no-shortcuts") ? 0 : 1;
        SetWindowLongPtrW(g_desk, GWLP_USERDATA, on), SetWindowLongPtrW(g_menu_box, GWLP_USERDATA, on);
    }
    g_note = child(L"STATIC", L"", 0, 28, 292, 624, 90, ID_NOTE, inst);
    /* page 2: progress */
    g_text = child(L"STATIC", L"", 0, 28, 150, 624, 48, 0, inst);
    g_bar = child(PROGRESS_CLASSW, NULL, 0, 28, 208, 624, 8, 0, inst);
    SetWindowTheme(g_bar, L"", L"");   /* (unthemed, so it takes the gold) */
    SendMessageW(g_bar, PBM_SETBARCOLOR, 0, C_GOLD), SendMessageW(g_bar, PBM_SETBKCOLOR, 0, C_RAISED);
    SendMessageW(g_bar, PBM_SETRANGE32, 0, 100);
    g_primary = child(L"BUTTON", L"", BS_OWNERDRAW | WS_VISIBLE | WS_TABSTOP, 502, 424, 150, 38, ID_PRIMARY, inst);
    g_second = child(L"BUTTON", L"", BS_OWNERDRAW | WS_VISIBLE | WS_TABSTOP, 372, 424, 120, 38, ID_SECOND, inst);
    show_page(0);
    ShowWindow(g_win, show);
    SetFocus(g_primary);
    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0)
        if (!IsDialogMessageW(g_win, &m)) TranslateMessage(&m), DispatchMessageW(&m);
    return g_failed ? 2 : 0;
}
