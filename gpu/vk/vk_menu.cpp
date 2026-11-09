// The F1 / Insert menu. Drawn on the CPU (GDI text into a bitmap) and copied over the window's image at the last step of a
// present, so it costs nothing while closed and it never touches the game's picture, nor what frame interpolation sees. Keys:
// F1 or Insert opens and closes it, Q and E (or Page Up and Page Down) change the tab, Up and Down choose, Left and Right
// change, Enter as Right, Esc closes; the game gets no keyboard input while it is open.
// (hookshot) A gamepad's Home button, or Back and Start together, opens it too; LB and RB change the tab, the D-pad chooses
// and changes, the bottom face button selects, the right one closes. Its Controls tab binds the Switch's buttons to gamepad
// inputs and keys.
// (local: tabs; rows with value lists and ini keys of their own, apart from their labels; a save that keeps the lines it does
// not own; Portuguese (Brazil) when language.txt beside the executable says pt-BR; a layout scaled with the window's height)
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "vk_paths.h"
#include "vk_menu.h"
#include "vk_work.h"   // (after the others: counts the commands that do GPU work, for full_barrier)
#ifndef HARPIA_VERSION   // (nx2/build.py gives it to this file: nx2/version.py)
#define HARPIA_VERSION "unknown"
#endif
#include "vk_dlss.h"
#ifndef _WIN32
#include "vk_gdi_linux.h"   // (hookshot) Linux: the GDI it draws with
#endif

namespace mkwvk {
HWND app_window();

Options opts;

namespace {

// ---- (local) every word it shows, English and Portuguese (Brazil) side by side (nullptr: the English one) ----
#define MENU_STRINGS(X)                                                                                                   \
    X(TAB_DISPLAY, "Display", "Tela")                                                                                    \
    X(TAB_GRAPHICS, "Graphics", "Gráficos")                                                                              \
    X(TAB_PERF, "Performance", "Desempenho")                                                                             \
    X(TAB_INPUT, "Input", "Entrada")                                                                                     \
    X(TAB_CONTROLS, "Controls", "Controles")                                                                             \
    X(TAB_INFO, "Info", nullptr)                                                                                         \
    X(OFF, "Off", "Desligado")                                                                                           \
    X(ON, "On", "Ligado")                                                                                                \
    X(FULLSCREEN, "Fullscreen", "Tela cheia")                                                                            \
    X(FULLSCREEN_D, "Borderless, at the monitor's size. F11 does the same.",                                             \
      "Sem bordas, no tamanho do monitor. F11 faz o mesmo.")                                                             \
    X(ASPECT, "Aspect ratio", "Proporção")                                                                               \
    X(ASPECT_D, "Stretch fills the window, Fit keeps 16:9 with bars, Fill shows more at the monitor's shape.",           \
      "Esticar preenche a janela, Ajustar mantém 16:9 com barras, Preencher mostra mais no formato do monitor.")         \
    X(STRETCH, "Stretch", "Esticar")                                                                                     \
    X(FIT, "Fit 16:9", "Ajustar 16:9")                                                                                   \
    X(FILL, "Fill (restart)", "Preencher (reiniciar)")                                                                   \
    X(VSYNC, "VSync", nullptr)                                                                                           \
    X(VSYNC_D, "Off: no tearing, low latency. On: waits for the display. Off (tearing): the lowest latency.",            \
      "Desligado: sem cortes, pouco atraso. Ligado: espera o monitor. Com cortes: o menor atraso.")                      \
    X(TEARING, "Off (tearing)", "Desligado (com cortes)")                                                                \
    X(FPS, "FPS counter", "Contador de FPS")                                                                             \
    X(FPS_D, "Frames shown each second, top right; with frame generation, the game's own below.",                       \
      "Quadros por segundo no canto superior direito; com geração de quadros, os do jogo embaixo.")                      \
    X(SCALE, "Render scale", "Escala de renderização")                                                                   \
    X(SCALE_D, "The 3D's resolution. Auto lets the game choose, as on the console; lower is faster.",                   \
      "A resolução do 3D. Auto deixa o jogo escolher, como no console; menor é mais rápido.")                            \
    X(AUTO, "Auto", nullptr)                                                                                             \
    X(UPSCALER, "Upscaler", "Ampliação")                                                                                 \
    X(UPSCALER_D, "How the picture is scaled up to the window: FSR 1 and NIS keep the edges sharp.",                     \
      "Como a imagem é ampliada para a janela: FSR 1 e NIS mantêm as bordas nítidas.")                                   \
    X(DLSS, "NVIDIA DLSS", nullptr)                                                                                      \
    X(DLSS_D, "In place of the game's upscale; needs NVIDIA's sl.*.dll beside the executable. Sets the render scale.",   \
      "No lugar da ampliação do jogo; precisa das sl.*.dll da NVIDIA ao lado do executável. Define a escala.")            \
    X(DLSS_Q, "Quality", "Qualidade")                                                                                    \
    X(DLSS_B, "Balanced", "Equilibrado")                                                                                 \
    X(DLSS_P, "Performance", "Desempenho")                                                                               \
    X(DLSS_UP, "Ultra Performance", "Ultra desempenho")                                                                  \
    X(DLSS_AA, "Native AA (DLAA)", "AA nativo (DLAA)")                                                                   \
    X(SHARP, "Sharpness", "Nitidez")                                                                                     \
    X(SHARP_D, "After the scaling: RCAS with FSR 1, NIS's own with NIS, a light one otherwise.",                         \
      "Após a ampliação: RCAS com FSR 1, a do próprio NIS com NIS, uma leve nos outros.")                                \
    X(ANISO, "Texture filtering", "Filtragem de texturas")                                                               \
    X(ANISO_D, "Anisotropic filtering: textures stay sharp at a slant. Game keeps the game's own.",                     \
      "Filtragem anisotrópica: texturas nítidas em ângulo. Jogo mantém a do próprio jogo.")                              \
    X(GAME, "Game", "Jogo")                                                                                              \
    X(CONSOLE, "Console mode", "Modo do console")                                                                        \
    X(CONSOLE_D, "The game's docked or handheld profile; handheld is lighter and softer. Read at the start.",           \
      "O perfil do jogo na TV ou portátil; portátil é mais leve e mais suave. Lido ao iniciar.")                         \
    X(TEXQ, "Texture quality", "Qualidade de textura")                                                                   \
    X(TEXQ_D, "High keeps the largest textures whole and re-encodes fewer of them; Ultra re-encodes none. Both use more " \
      "VRAM. Read at the start.",                                                                                        \
      "Alta mantém as maiores texturas inteiras e recomprime menos; Ultra não recomprime nenhuma. As duas usam mais "     \
      "VRAM. Lido ao iniciar.")                                                                                          \
    X(OUTRES, "Output resolution", "Resolução de saída")                                                                 \
    X(OUTRES_D, "The size the game composes its picture at. 2560x1440: the 3D at 1920x1080, the game's TAAU brings it "  \
      "to 2560x1440 and the HUD is sharp. Docked only. Read at the start.",                                              \
      "O tamanho em que o jogo compõe a imagem. 2560x1440: o 3D a 1920x1080, a TAAU do jogo leva a 2560x1440 e o HUD "    \
      "fica nítido. Só na TV. Lido ao iniciar.")                                                                         \
    X(OUTRES_GAME, "Game (2048x1152)", "Jogo (2048x1152)")                                                               \
    X(OUTRES_25K, "2560x1440 (2.5K)", "2560x1440 (2.5K)")                                                                \
    X(FOG, "Fog", "Névoa")                                                                                                \
    X(FOG_D, "Classic: the thinner fog this build had. Game's: the game's own haze (EvilOOT's fix). Read at the start.",  \
      "Clássica: a névoa mais fina que esta versão tinha. Do jogo: a névoa do próprio jogo (correção do EvilOOT). Lido ao iniciar.") \
    X(FOG_CLASSIC, "Classic", "Clássica")                                                                                 \
    X(FOG_GAME, "Game's", "Do jogo")                                                                                      \
    X(TQ_NORMAL, "Normal", "Normal")                                                                                        \
    X(TQ_HIGH, "High", "Alta")                                                                                              \
    X(TQ_ULTRA, "Ultra", "Ultra")                                                                                           \
    X(DOCKED, "Docked", "Na TV")                                                                                         \
    X(HANDHELD, "Handheld", "Portátil")                                                                                  \
    X(FRAMEGEN, "Frame generation", "Geração de quadros")                                                                \
    X(FRAMEGEN_D, "Frames made between the game's own: 2x up to 8x its frame rate.",                                    \
      "Quadros criados entre os do jogo: de 2x até 8x a sua taxa de quadros.")                                           \
    X(DLSSG, "Frame generation DLSS", "Geração de quadros DLSS")                                                         \
    X(DLSSG_D, "NVIDIA DLSS with Reflex: 2x, 3x or 4x the frames shown (4x: RTX 50). Turns the other one off.",          \
      "NVIDIA DLSS com Reflex: 2x, 3x ou 4x os quadros exibidos (4x: RTX 50). Desliga a outra.")                         \
    X(PACE, "Frame pacing", "Ritmo de quadros")                                                                          \
    X(PACE_D, "Frames shown on an even beat, for smoother motion.",                                                      \
      "Quadros mostrados em um ritmo regular, para um movimento mais suave.")                                            \
    X(SPEED, "Game speed", "Velocidade do jogo")                                                                         \
    X(SPEED_D, "Per frame: as on the console. Real time: game time follows the clock when frames drop.",                \
      "Por quadro: como no console. Tempo real: o tempo do jogo segue o relógio quando os quadros caem.")                \
    X(PER_FRAME, "Per frame", "Por quadro")                                                                              \
    X(REAL_TIME, "Real time", "Tempo real")                                                                              \
    X(RESTART, "restart", "reiniciar")                                                                                   \
    X(HINT, "Q / E  tabs      Up / Down  choose      Left / Right  change      Esc  close",                              \
      "Q / E  abas      Cima / Baixo  escolher      Esquerda / Direita  mudar      Esc  fechar")                         \
    X(C_BUTTON, "Button", "Botão")                                                                                       \
    X(C_GAMEPAD, "Gamepad", "Controle")                                                                                  \
    X(C_KEYBOARD, "Keyboard", "Teclado")                                                                                 \
    X(C_RESET, "Reset to defaults", "Restaurar padrões")                                                                 \
    X(C_PRESS_BUTTON, "Press a button...", "Pressione um botão...")                                                      \
    X(C_PRESS_KEY, "Press a key...", "Pressione uma tecla...")                                                           \
    X(C_NONE, "None", "Nenhum")                                                                                          \
    X(C_HINT, "Enter binds      Delete clears      Left / Right  gamepad or keyboard      Esc  close",                   \
      "Enter associa      Delete limpa      Esquerda / Direita  controle ou teclado      Esc  fecha")                    \
    X(C_HINT_LISTEN, "Press the new one      Esc cancels", "Pressione o novo      Esc cancela")                          \
    X(C_HINT_PAD, "Gamepad: the D-pad moves, the bottom button binds, the right one closes",                            \
      "Controle: o direcional move, o botão de baixo associa, o da direita fecha")                                       \
    X(C_UNAVAILABLE, "The controls cannot be changed here.", "Os controles não podem ser mudados aqui.")                 \
    X(PLUS, "Plus", "Mais")                                                                                              \
    X(MINUS, "Minus", "Menos")                                                                                           \
    X(PAD_SOUTH, "Bottom button", "Botão de baixo")                                                                      \
    X(PAD_EAST, "Right button", "Botão da direita")                                                                      \
    X(PAD_WEST, "Left button", "Botão da esquerda")                                                                      \
    X(PAD_NORTH, "Top button", "Botão de cima")                                                                          \
    X(PAD_BACK, "Select / Back", nullptr)                                                                                \
    X(PAD_GUIDE, "Home", nullptr)                                                                                        \
    X(PAD_START, "Start", nullptr)                                                                                       \
    X(PAD_LSTICK, "Left stick press", "Analógico esq. (clique)")                                                         \
    X(PAD_RSTICK, "Right stick press", "Analógico dir. (clique)")                                                        \
    X(PAD_LSHOULDER, "L1 / LB", nullptr)                                                                                 \
    X(PAD_RSHOULDER, "R1 / RB", nullptr)                                                                                 \
    X(PAD_UP, "D-pad up", "Direcional cima")                                                                             \
    X(PAD_DOWN, "D-pad down", "Direcional baixo")                                                                        \
    X(PAD_LEFT, "D-pad left", "Direcional esquerda")                                                                     \
    X(PAD_RIGHT, "D-pad right", "Direcional direita")                                                                    \
    X(PAD_LTRIGGER, "L2 / LT", nullptr)                                                                                  \
    X(PAD_RTRIGGER, "R2 / RT", nullptr)                                                                                  \
    X(I_GPU, "Graphics card", "Placa de vídeo")                                                                          \
    X(I_OUTPUT, "Output", "Saída")                                                                                       \
    X(I_KEYS, "Keys", "Teclas")                                                                                          \
    X(I_KEY_MENU, "F1 or Insert: this menu", "F1 ou Insert: este menu")                                                  \
    X(I_KEY_FULL, "F11 or Alt+Enter: fullscreen", "F11 ou Alt+Enter: tela cheia")                                        \
    X(I_KEY_PAD, "Gamepad: Back + Start (or Home): this menu, LB / RB: tabs",                                           \
      "Controle: Back + Start (ou Home): este menu, LB / RB: abas")                                                      \
    X(I_CREDITS, "Credits", "Créditos")                                                                                  \
    X(I_FSR, "AMD FidelityFX Super Resolution 1: AMD, MIT license", "AMD FidelityFX Super Resolution 1: AMD, licença MIT") \
    X(I_NIS, "NVIDIA Image Scaling: NVIDIA, MIT license", "NVIDIA Image Scaling: NVIDIA, licença MIT")                   \
    X(I_HINT, "Q / E  tabs      Esc  close", "Q / E  abas      Esc  fechar")                                           \
    X(PRESET, "Quality preset", "Predefinição de qualidade")                                                             \
    X(PRESET_D, "Sets render scale, upscaler, sharpness, frame generation and texture quality for a GPU tier (FSR 1 and " \
      "NIS are kept). Changing any of those rows makes it Custom.",                                                      \
      "Ajusta escala, ampliação, nitidez, geração de quadros e qualidade de textura para uma faixa de GPU (FSR 1 e NIS " \
      "são mantidos). Mudar qualquer uma dessas linhas a deixa Personalizada.")                                          \
    X(PR_CUSTOM, "Custom", "Personalizada")                                                                              \
    X(PR_MIN, "Minimum (4GB)", "Mínima (4GB)")                                                                           \
    X(PR_LOW, "Low (6GB)", "Baixa (6GB)")                                                                                \
    X(PR_BALANCED, "Balanced (8GB)", "Equilibrada (8GB)")                                                                \
    X(PR_HIGH, "High (12GB)", "Alta (12GB)")                                                                             \
    X(PR_ULTRA, "Ultra (16GB+)", nullptr)                                                                                \
    X(PAD_BG, "Controller in background", "Controle em segundo plano")                                                   \
    X(PAD_BG_D, "On: the controller keeps playing while another window has the focus, for streaming. The keyboard and " \
      "mouse still need the game's window.",                                                                             \
      "Ligado: o controle continua jogando com outra janela em foco, para streaming. Teclado e mouse ainda precisam da " \
      "janela do jogo.")                                                                                                 \
    X(TEXT_SPEED, "Text speed", "Velocidade do texto")                                                                   \
    X(TEXT_SPEED_D, "How fast dialogue letters appear. Normal is the game's own pace; Instant shows each page at once, " \
      "as pressing A does.",                                                                                             \
      "A rapidez com que as letras dos diálogos aparecem. Normal é o ritmo do jogo; Instantâneo mostra cada página de "  \
      "uma vez, como apertar A.")                                                                                        \
    X(TS_NORMAL, "Normal", nullptr)                                                                                      \
    X(TS_INSTANT, "Instant", "Instantâneo")                                                                              \
    X(GPUDEV_D, "Which graphics card Vulkan uses. Auto picks the best dedicated one. Read at the start.",                \
      "Qual placa de vídeo o Vulkan usa. Auto escolhe a melhor dedicada. Lido ao iniciar.")                              \
    X(ROBUST, "Robust buffers", "Buffers protegidos")                                                                    \
    X(ROBUST_D, "Bounds-checked buffer reads: a read past a buffer's end is dropped instead of stopping the GPU. Auto "  \
      "is on, on every card; Off can be faster on some NVIDIA cards. Read at the start.",                                \
      "Leituras de buffer com limite: uma leitura além do fim é descartada em vez de travar a GPU. Auto é ligado em "    \
      "toda placa; Desligado pode ser mais rápido em algumas NVIDIA. Lido ao iniciar.")                                  \
    X(MOUSE_CAM, "Mouse camera", "Câmera do mouse")                                                                      \
    X(MOUSE_CAM_D, "Native: the captured mouse turns the camera itself, one to one, as in a PC game. As the stick: the " \
      "mouse stands in for the right stick, with its curve and top speed.",                                              \
      "Nativa: o mouse capturado gira a própria câmera, um para um, como num jogo de PC. Como o analógico: o mouse faz " \
      "o papel do analógico direito, com a sua curva e velocidade máxima.")                                              \
    X(MC_NATIVE, "Native", "Nativa")                                                                                     \
    X(MC_STICK, "As the stick", "Como o analógico")                                                                      \
    X(MOUSE_SENS, "Mouse sensitivity", "Sensibilidade do mouse")                                                         \
    X(MOUSE_SENS_D, "How far the camera turns for a mouse movement, with either mouse camera. At 1.00 native look turns " \
      "a tenth of a degree a count. The - and = keys change it in the game too.",                                        \
      "Quanto a câmera gira para um movimento do mouse, nos dois modos. Em 1,00 a câmera nativa gira um décimo de grau " \
      "por contagem. As teclas - e = também mudam no jogo.")                                                             \
    X(MOUSE_INV, "Invert mouse vertical", "Inverter mouse vertical")                                                     \
    X(MOUSE_INV_D, "Swaps up and down for the mouse: moving it forward tilts the camera down.",                          \
      "Troca cima e baixo no mouse: movê-lo para a frente inclina a câmera para baixo.")                                 \
    X(INV_X, "Invert camera X", "Inverter câmera X")                                                                     \
    X(INV_X_D, "Inverts horizontal look (left / right) on the right stick.",                                            \
      "Inverte o olhar horizontal (esquerda / direita) no analógico direito.")                                           \
    X(INV_Y, "Invert camera Y", "Inverter câmera Y")                                                                     \
    X(INV_Y_D, "Inverts vertical look (up / down) on the right stick.",                                                 \
      "Inverte o olhar vertical (cima / baixo) no analógico direito.")                                                   \
    X(LAYOUT, "Layout preset", "Layout do controle")                                                                     \
    X(LAYOUT_D, "Nintendo (the Switch's order), Xbox / PC (A to act and talk), or the face buttons swapped. Binding a "  \
      "button by hand makes it Custom.",                                                                                 \
      "Nintendo (a ordem do Switch), Xbox / PC (A para agir e falar) ou os botões de face trocados. Associar um botão "  \
      "à mão o deixa Personalizado.")                                                                                    \
    X(PL_NINTENDO, "Nintendo", nullptr)                                                                                  \
    X(PL_XBOX, "Xbox / PC", nullptr)                                                                                     \
    X(PL_SWAP, "Swap face", "Trocar face")                                                                               \
    X(PL_CUSTOM, "Custom", "Personalizado")                                                                              \
    X(C_MOVE_UP, "Move forward", "Andar para frente")                                                                    \
    X(C_MOVE_DOWN, "Move back", "Andar para trás")                                                                       \
    X(C_MOVE_LEFT, "Move left", "Andar para a esquerda")                                                                 \
    X(C_MOVE_RIGHT, "Move right", "Andar para a direita")                                                                \
    X(C_CAM_UP, "Camera up", "Câmera para cima")                                                                         \
    X(C_CAM_DOWN, "Camera down", "Câmera para baixo")                                                                    \
    X(C_CAM_LEFT, "Camera left", "Câmera para a esquerda")                                                               \
    X(C_CAM_RIGHT, "Camera right", "Câmera para a direita")

enum Sid {
#define X(id, en, pt) S_##id,
    MENU_STRINGS(X)
#undef X
    S_N
};
const char* const strings[S_N][2] = {
#define X(id, en, pt) {en, pt},
    MENU_STRINGS(X)
#undef X
};
static_assert(S_PAD_RTRIGGER - S_PAD_SOUTH + 1 == HOS_PAD_N, "a name for each gamepad input, in HOS_PAD_* order");
static_assert(S_TAB_INFO - S_TAB_DISPLAY == 5, "the tabs' names in order");
int lang = 0;   // 1: Portuguese (Brazil)
const char* T(int id) { return strings[id][lang] ? strings[id][lang] : strings[id][0]; }

std::string exe_dir() {
    char exe[MAX_PATH];
    const DWORD n = game_module_path(exe, MAX_PATH);   // (upstream 81f2084: the game folder, not bin beside the launcher)
    std::string dir(exe, n);
    return dir.substr(0, dir.find_last_of("\\/") + 1);
}
std::string ini_path() { return exe_dir() + "harpia_options.ini"; }
/// (local) language.txt beside the executable: its first line that is not a comment ("#") holds the code; pt-BR is Portuguese
void language_load() {
    FILE* f = fopen((exe_dir() + "language.txt").c_str(), "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char* s = line;
        if (!strncmp(s, "\xEF\xBB\xBF", 3)) s += 3;   // (a byte order mark)
        while (*s == ' ' || *s == '\t') s++;
        if (!*s || *s == '#' || *s == '\r' || *s == '\n') continue;
        char code[8] = {};
        for (size_t n = 0; s[n] && !isspace((unsigned char)s[n]) && n + 1 < sizeof code; n++) code[n] = (char)tolower((unsigned char)s[n]);
        lang = !strcmp(code, "pt-br") || !strcmp(code, "pt_br");
        break;
    }
    fclose(f);
}

// ---- the options: what each row shows, the values it takes, the tab it is on ----
char num[16];   // (the one number a row's text makes at a time)
const char* tx_onoff(int v) { return T(v ? S_ON : S_OFF); }
const char* tx_aspect(int v) { return T(v == 2 ? S_FILL : v == 1 ? S_FIT : S_STRETCH); }
const char* tx_vsync(int v) { return T(v == 2 ? S_TEARING : v == 1 ? S_ON : S_OFF); }
const char* tx_scale(int v) { return v ? (snprintf(num, sizeof num, "%d%%", v), num) : T(S_AUTO); }
} // namespace
// (hookshot: the scale DLSS SR fixes while it runs, or DLSS-G alone while loaded and on (no dynamic resolution: Auto is 100%,
// a fixed percent stays), as vk_replay.cpp's t.option gives the host; -1: the player's render scale)
int menu_dlss_scale() {
    if (dlss_active()) return dlss_scale_percent(opts.dlss, dlss_out_h, dlss_base());
    if (dlssg_on() && opts.dlssg >= 2) return opts.render_scale ? opts.render_scale : 100;
    return -1;
}
namespace {
const char* tx_render_scale(int v) {   // (the render scale row and the Info tab: DLSS's while it overrides the player's)
    const int d = menu_dlss_scale();
    return d < 0 || d == v ? tx_scale(v) : (snprintf(num, sizeof num, "%d%% (DLSS)", d), num);
}
const char* tx_upscale(int v) {
    static const char* t[] = {"Linear", "Catmull-Rom", "Lanczos", "AMD FSR 1", "NVIDIA NIS"};
    return t[std::clamp(v, 0, 4)];
}
const char* tx_dlss(int v) {   // (Options::dlss)
    static const int s[] = {S_OFF, S_AUTO, S_DLSS_AA, S_DLSS_Q, S_DLSS_B, S_DLSS_P, S_DLSS_UP};
    return T(s[std::clamp(v, 0, 6)]);
}
const char* tx_percent(int v) { return snprintf(num, sizeof num, "%d%%", v), num; }
const char* tx_times(int v) { return snprintf(num, sizeof num, "%dx", v), num; }
const char* tx_aniso(int v) { return v ? tx_times(v) : T(S_GAME); }
const char* tx_framegen(int v) { return v ? tx_times(v) : T(S_OFF); }
const char* tx_console(int v) { return T(v ? S_HANDHELD : S_DOCKED); }
const char* tx_fog(int v) { return T(v ? S_FOG_CLASSIC : S_FOG_GAME); }
const char* tx_texq(int v) { return T(v >= 2 ? S_TQ_ULTRA : v ? S_TQ_HIGH : S_TQ_NORMAL); }
const char* tx_outres(int v) { return T(v ? S_OUTRES_25K : S_OUTRES_GAME); }
const char* tx_speed(int v) { return T(v ? S_REAL_TIME : S_PER_FRAME); }
const char* tx_text_speed(int v) { return v == 1 ? "2x" : v == 2 ? "4x" : T(v == 3 ? S_TS_INSTANT : S_TS_NORMAL); }
const char* tx_preset(int v) { return T(S_PR_CUSTOM + std::clamp(v, 0, 5)); }
static_assert(S_PR_ULTRA - S_PR_CUSTOM == 5, "the presets' names in order");
int gpu_device_at_start = 0, robust_start = 0;   // (the values the game started with: menu_set_gpu_count, menu_init)
const char* tx_gpu(int v) {   // (upstream b20ea5f: 0 Auto, else the device's place in vk_main.cpp's list and its name)
    static char b[300];
    if (v <= 0) return T(S_AUTO);
    const int i = v - 1;
    if (i < (int)gpu_device_names.size()) snprintf(b, sizeof b, "%d: %s", i, gpu_device_names[i].c_str());
    else snprintf(b, sizeof b, "%d", i);
    return b;
}
const char* tx_robust(int v) { return v == 1 ? T(S_OFF) : v == 2 ? T(S_ON) : T(S_AUTO); }
const char* tx_mousecam(int v) { return T(v ? S_MC_NATIVE : S_MC_STICK); }
const char* tx_sens(int v) { return snprintf(num, sizeof num, "%.2f", v / 100.0), num; }
const char* tx_pad_layout(int v) { return T(S_PL_NINTENDO + std::clamp(v, 0, 3)); }
static_assert(S_PL_CUSTOM - S_PL_NINTENDO == 3, "the layouts' names in order");

// (upstream f447272) Quality presets: the rows they set, and only those. (local, review: render scale from upstream's
// resolution 4/2/1 as 50/75/100, texture quality for its VRAM tiers, whose budget comes from that row alone (vk_texture.cpp's
// own, never lowered by a preset); console mode, output resolution, pacing and game speed are the player's; an upscaler
// chosen as FSR 1 or NIS stays)
void apply_preset(int p) {
    struct P { int scale, upscale, sharpness, framegen, tex_quality; };
    static const P presets[] = {{50, 2, 50, 2, 0}, {75, 2, 35, 2, 0}, {100, 1, 25, 2, 0}, {100, 2, 30, 2, 1}, {100, 2, 40, 3, 2}};
    if (p < 1 || p > 5) return;
    const P& s = presets[p - 1];
    opts.render_scale = s.scale, opts.sharpness = s.sharpness, opts.tex_quality = s.tex_quality;
    opts.framegen = dlssg_on() && opts.dlssg >= 2 ? 0 : s.framegen;   // (hookshot: never with DLSS Frame Generation loaded)
    if (opts.upscale < 3) opts.upscale = s.upscale;   // (3 FSR 1, 4 NIS: the player's)
}
bool preset_row(const int* v) {   // (a row a preset sets: changed by hand, the preset is Custom)
    return v == &opts.render_scale || v == &opts.upscale || v == &opts.sharpness || v == &opts.framegen || v == &opts.tex_quality;
}

const int l_scale[] = {0, 100, 85, 75, 67, 58, 50, 44, 33};
const int l_aniso[] = {0, 2, 4, 8, 16};
const int l_framegen[] = {0, 2, 3, 4, 5, 6, 7, 8};
const int l_dlssg[] = {0, 2, 3, 4};
int aspect_start = 0;
int fog_start = 1;   // (the fog the programs were translated with this run: RX_VK_OOT_VISUAL, read once)   // (Fill is read once, at the start: the badge while a change to or from it waits for a restart)

struct Row {
    const char* key;   // (local) its ini key, apart from its label
    int* value;
    int tab, name, desc;   // (S_*)
    const char* (*text)(int v);
    const int* list;   // the values it takes, in order; else lo to hi by step
    int n, lo, hi, step;
    bool (*restart)(int v);   // (the "restart" badge, while it says so)
};
enum { TAB_DISPLAY, TAB_GRAPHICS, TAB_PERF, TAB_INPUT, TAB_CONTROLS, TAB_INFO, kTabs };
Row rows[] = {
    {"fullscreen", &opts.fullscreen, TAB_DISPLAY, S_FULLSCREEN, S_FULLSCREEN_D, tx_onoff, nullptr, 0, 0, 1, 1, nullptr},
    {"aspect", &opts.aspect, TAB_DISPLAY, S_ASPECT, S_ASPECT_D, tx_aspect, nullptr, 0, 0, 2, 1,
     [](int v) { return v == 2 || (aspect_start == 2 && v != 2); }},
    {"vsync", &opts.vsync, TAB_DISPLAY, S_VSYNC, S_VSYNC_D, tx_vsync, nullptr, 0, 0, 2, 1, nullptr},
    {"fps_counter", &opts.show_fps, TAB_DISPLAY, S_FPS, S_FPS_D, tx_onoff, nullptr, 0, 0, 1, 1, nullptr},
    // (upstream b20ea5f; vk_main.cpp picks the device with it; hi: the devices there are, menu_set_gpu_count)
    {"gpu_device", &opts.gpu_device, TAB_DISPLAY, S_I_GPU, S_GPUDEV_D, tx_gpu, nullptr, 0, 0, 8, 1,
     [](int v) { return v != gpu_device_at_start; }},
    {"quality_preset", &opts.preset, TAB_GRAPHICS, S_PRESET, S_PRESET_D, tx_preset, nullptr, 0, 0, 5, 1, nullptr},
    {"render_scale", &opts.render_scale, TAB_GRAPHICS, S_SCALE, S_SCALE_D, tx_render_scale, l_scale, 9, 0, 0, 0, nullptr},
    {"upscaling", &opts.upscale, TAB_GRAPHICS, S_UPSCALER, S_UPSCALER_D, tx_upscale, nullptr, 0, 0, 4, 1, nullptr},
    {"dlss", &opts.dlss, TAB_GRAPHICS, S_DLSS, S_DLSS_D, tx_dlss, nullptr, 0, 0, 6, 1,
     [](int v) { return v != 0 && !dlss_active(); }},   // (restart: on, and Streamline not loaded at the start; off is at once)
    {"sharpness", &opts.sharpness, TAB_GRAPHICS, S_SHARP, S_SHARP_D, tx_percent, nullptr, 0, 0, 100, 10, nullptr},
    {"anisotropy", &opts.anisotropy, TAB_GRAPHICS, S_ANISO, S_ANISO_D, tx_aniso, l_aniso, 5, 0, 0, 0, nullptr},
    {"output_res", &opts.output_res, TAB_GRAPHICS, S_OUTRES, S_OUTRES_D, tx_outres, nullptr, 0, 0, 1, 1,
     [](int) { return true; }},
    {"texture_quality", &opts.tex_quality, TAB_GRAPHICS, S_TEXQ, S_TEXQ_D, tx_texq, nullptr, 0, 0, 2, 1,
     [](int) { return true; }},
    {"classic_fog", &opts.classic_fog, TAB_GRAPHICS, S_FOG, S_FOG_D, tx_fog, nullptr, 0, 0, 1, 1,
     [](int v) { return v != fog_start; }},
    {"handheld_profile", &opts.handheld, TAB_GRAPHICS, S_CONSOLE, S_CONSOLE_D, tx_console, nullptr, 0, 0, 1, 1,
     [](int) { return true; }},
    {"frame_generation", &opts.framegen, TAB_PERF, S_FRAMEGEN, S_FRAMEGEN_D, tx_framegen, l_framegen, 8, 0, 0, 0, nullptr},
    // (hookshot: NVIDIA DLSS Frame Generation, vk_dlss.cpp; only with its DLLs and an RTX 40 or 50)
    {"dlss_frame_generation", &opts.dlssg, TAB_PERF, S_DLSSG, S_DLSSG_D, tx_framegen, l_dlssg, 4, 0, 0, 0,
     [](int v) { return v >= 2 && !dlssg_on(); }},   // (restart: on, and DLSS-G not loaded at the start; off and 2x-4x at once)
    {"frame_pacing", &opts.pace, TAB_PERF, S_PACE, S_PACE_D, tx_onoff, nullptr, 0, 0, 1, 1, nullptr},
    {"game_speed", &opts.realtime, TAB_PERF, S_SPEED, S_SPEED_D, tx_speed, nullptr, 0, 0, 1, 1, nullptr},
    // (upstream 52a4478; option 5, HOS_TEXT_SPEED over it)
    {"text_speed", &opts.text_speed, TAB_PERF, S_TEXT_SPEED, S_TEXT_SPEED_D, tx_text_speed, nullptr, 0, 0, 3, 1, nullptr},
    // (upstream 10ecfa9, abc38ff; vk_main.cpp takes opts.robust after menu_init, before the device is made)
    {"robust_buffers", &opts.robust, TAB_PERF, S_ROBUST, S_ROBUST_D, tx_robust, nullptr, 0, 0, 2, 1,
     [](int v) { return v != robust_start; }},
    // (upstream 373a0c0; the host reads it as option 4, HOS_INPUT_BACKGROUND over it)
    {"controller_in_background", &opts.pad_background, TAB_INPUT, S_PAD_BG, S_PAD_BG_D, tx_onoff, nullptr, 0, 0, 1, 1, nullptr},
    // (upstream 1bfabdf: the face buttons' order, set on the Controls tab's bindings; 3 Custom is shown, not chosen)
    {"layout_preset", &opts.pad_layout, TAB_INPUT, S_LAYOUT, S_LAYOUT_D, tx_pad_layout, nullptr, 0, 0, 2, 1, nullptr},
    // (upstream 25bba9a, 64c4a4e: the host reads them as options 6, 7 and 8; 16 and 17 the right stick's inversions)
    {"mouse_camera", &opts.mouse_native, TAB_INPUT, S_MOUSE_CAM, S_MOUSE_CAM_D, tx_mousecam, nullptr, 0, 0, 1, 1, nullptr},
    {"mouse_sensitivity", &opts.mouse_sens, TAB_INPUT, S_MOUSE_SENS, S_MOUSE_SENS_D, tx_sens, nullptr, 0, 10, 500, 10, nullptr},
    {"invert_mouse_vertical", &opts.mouse_invert, TAB_INPUT, S_MOUSE_INV, S_MOUSE_INV_D, tx_onoff, nullptr, 0, 0, 1, 1, nullptr},
    {"invert_camera_x", &opts.invert_cam_x, TAB_INPUT, S_INV_X, S_INV_X_D, tx_onoff, nullptr, 0, 0, 1, 1, nullptr},
    {"invert_camera_y", &opts.invert_cam_y, TAB_INPUT, S_INV_Y, S_INV_Y_D, tx_onoff, nullptr, 0, 0, 1, 1, nullptr},
};
constexpr int kRows = sizeof rows / sizeof rows[0];
int tab_rows(int tab, int* out) {
    int n = 0;
    for (int i = 0; i < kRows; i++)
        if (rows[i].tab == tab && (rows[i].value != &opts.dlss || dlss_dll_present()) &&
            (rows[i].value != &opts.dlssg || dlssg_dll_present()))   // (DLSS, DLSS-G: only with their DLLs)
            out[n++] = i;
    return n;
}
int row_of(const int* value) {
    for (int i = 0; i < kRows; i++)
        if (rows[i].value == value) return i;
    return 0;
}
/// (local, review) The legacy fields kept to the new ones, for the readers not yet moved (replay's t.option reads resolution)
// shortcut: resolution has only 100/75/67/50, so 85 runs at 75, 58/44/33 at 50; drop when replay reads render_scale
void legacy_sync() {
    const int s = opts.render_scale;
    opts.resolution = s == 0 ? 0 : s >= 100 ? 1 : s >= 75 ? 2 : s >= 67 ? 3 : 4;
    opts.ultrawide = opts.aspect == 2;
}
/// A value it can take: the nearest of its list, or within lo and hi
int snap(const Row& r, int v) {
    if (!r.list) return std::clamp(v, r.lo, r.hi);
    int best = r.list[0];
    for (int i = 1; i < r.n; i++)
        if (std::abs(r.list[i] - v) < std::abs(best - v)) best = r.list[i];
    return best;
}
/// The next value (dir 1) or the one before (-1), round
int stepped(const Row& r, int dir) {
    const int v = *r.value;
    if (!r.list) {
        const int s = v + dir * r.step;
        return s > r.hi ? r.lo : s < r.lo ? r.hi : s;
    }
    int at = 0;
    while (at < r.n && r.list[at] != snap(r, v)) at++;
    return r.list[(at + dir + r.n) % r.n];
}

std::atomic<bool> open_now{false};
int cur_tab = TAB_DISPLAY;
int sel_row[kTabs] = {};   // (each options tab's chosen row)
bool menu_refresh = false;
// (hookshot) the controls, kept by the host (host/nn_hid.c reads them back from here): pad_<button>, key_<button>
const char* const bind_names[HOS_KEY_N] = {"a", "b", "x", "y", "ls", "rs", "l", "r", "zl", "zr", "plus", "minus",
                                           "left", "up", "right", "down", "move_up", "move_down", "move_left",
                                           "move_right", "cam_up", "cam_down", "cam_left", "cam_right"};
bool can_bind() { return host && host->binding && host->bind && host->pad_raw; }
// (upstream 1bfabdf, f1b5725) The face buttons' layouts, as the gamepad inputs of A, B, X and Y: set as bindings, and the
// row shows what the bindings are
const int layouts[3][4] = {{HOS_PAD_EAST, HOS_PAD_SOUTH, HOS_PAD_NORTH, HOS_PAD_WEST},    // (Nintendo)
                           {HOS_PAD_SOUTH, HOS_PAD_EAST, HOS_PAD_WEST, HOS_PAD_NORTH},    // (Xbox / PC)
                           {HOS_PAD_SOUTH, HOS_PAD_EAST, HOS_PAD_NORTH, HOS_PAD_WEST}};   // (the face buttons swapped)
const int face[4] = {HOS_SW_A, HOS_SW_B, HOS_SW_X, HOS_SW_Y};
void apply_pad_layout(int layout) {
    if (!can_bind() || layout < 0 || layout > 2) return;
    for (int i = 0; i < 4; i++) host->bind(HOS_BIND_PAD, face[i], 0, layouts[layout][i]);
}
int detect_pad_layout() {
    if (!can_bind()) return opts.pad_layout;
    for (int l = 0; l < 3; l++) {
        int i = 0;
        while (i < 4 && host->binding(HOS_BIND_PAD, face[i], 0) == layouts[l][i]) i++;
        if (i == 4) return l;
    }
    return 3;
}
/// (local) The file's own lines kept: the ones it owns (its rows', the bindings') replaced where they are, the rest left as they are
void save() {
    // (local, review: the host reads the bindings from this file the first time it is asked; asked now, before "w" empties it)
    if (can_bind()) host->binding(HOS_BIND_PAD, 0, 0);
    std::vector<std::pair<std::string, std::string>> mine;   // (key, its value)
    for (const Row& r : rows) mine.emplace_back(r.key, std::to_string(*r.value));
    // (local, review: the host reads only ultrawide= at the start, so it is ours too, kept to Fill; else a stale 1 wins)
    mine.emplace_back("ultrawide", std::to_string(opts.aspect == 2));
    // (2: game_speed below 2 was only ever the old default, see menu_init; 3, upstream be960bb: the mouse's buttons are
    // bindings, host/nn_hid.c binds_load)
    mine.emplace_back("options_version", "3");
    for (int i = 0; i < HOS_KEY_N && can_bind(); i++) {   // (the keys-only controls, HOS_KEY_*, have no pad_ line)
        if (i < HOS_SW_N) mine.emplace_back(std::string("pad_") + bind_names[i], std::to_string(host->binding(HOS_BIND_PAD, i, 0)));
        mine.emplace_back(std::string("key_") + bind_names[i], std::to_string(host->binding(HOS_BIND_KEY, i, 0)) + "," +
                                                                  std::to_string(host->binding(HOS_BIND_KEY, i, 1)));
    }
    std::vector<bool> done(mine.size());
    std::string out;
    auto take = [&](std::string line) {
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
        const size_t eq = line.find('=');
        for (size_t i = 0; eq != std::string::npos && i < mine.size(); i++) {
            if (line.compare(0, eq, mine[i].first) || eq != mine[i].first.size()) continue;
            if (!done[i]) out += mine[i].first + "=" + mine[i].second + "\n", done[i] = true;
            return;   // (one of its own: written once, where it was)
        }
        out += line + "\n";
    };
    const std::string path = ini_path();
    if (FILE* f = fopen(path.c_str(), "r")) {
        char buf[256];
        std::string line;
        while (fgets(buf, sizeof buf, f)) {
            line += buf;
            if (line.back() == '\n') take(line), line.clear();   // (else a longer line: the rest of it next)
        }
        if (!line.empty()) take(line);
        fclose(f);
    }
    for (size_t i = 0; i < mine.size(); i++)
        if (!done[i]) out += mine[i].first + "=" + mine[i].second + "\n";
    if (FILE* f = fopen(path.c_str(), "w")) {
        fwrite(out.data(), 1, out.size(), f);
        fclose(f);
    }
}

// ---- (hookshot) the Controls tab: what presses each of the Switch's buttons, a gamepad input and up to two keys ----
struct Control {
    int button;   // HOS_SW_*, or HOS_KEY_* (keys only)
    const char* fixed;   // (its name in every language, or nullptr: name)
    int name;
};
const Control controls[] = {{HOS_SW_A, "A", 0}, {HOS_SW_B, "B", 0}, {HOS_SW_X, "X", 0}, {HOS_SW_Y, "Y", 0},
                            {HOS_SW_L, "L", 0}, {HOS_SW_R, "R", 0}, {HOS_SW_ZL, "ZL", 0}, {HOS_SW_ZR, "ZR", 0},
                            {HOS_SW_PLUS, nullptr, S_PLUS}, {HOS_SW_MINUS, nullptr, S_MINUS},
                            {HOS_SW_LS, nullptr, S_PAD_LSTICK}, {HOS_SW_RS, nullptr, S_PAD_RSTICK},
                            {HOS_SW_UP, nullptr, S_PAD_UP}, {HOS_SW_DOWN, nullptr, S_PAD_DOWN},
                            {HOS_SW_LEFT, nullptr, S_PAD_LEFT}, {HOS_SW_RIGHT, nullptr, S_PAD_RIGHT},
                            // (upstream cd3735f: the left stick's and the camera's directions, keys only)
                            {HOS_KEY_MOVE_UP, nullptr, S_C_MOVE_UP}, {HOS_KEY_MOVE_DOWN, nullptr, S_C_MOVE_DOWN},
                            {HOS_KEY_MOVE_LEFT, nullptr, S_C_MOVE_LEFT}, {HOS_KEY_MOVE_RIGHT, nullptr, S_C_MOVE_RIGHT},
                            {HOS_KEY_CAM_UP, nullptr, S_C_CAM_UP}, {HOS_KEY_CAM_DOWN, nullptr, S_C_CAM_DOWN},
                            {HOS_KEY_CAM_LEFT, nullptr, S_C_CAM_LEFT}, {HOS_KEY_CAM_RIGHT, nullptr, S_C_CAM_RIGHT}};
constexpr int kControls = sizeof controls / sizeof controls[0];   // (then "Reset to defaults")
bool keys_only(int i) { return i < kControls && controls[i].button >= HOS_SW_N; }   // (no gamepad input for those)
const char* pad_name(int p) { return p >= 0 && p < HOS_PAD_N ? T(S_PAD_SOUTH + p) : T(S_C_NONE); }
const char* key_name(int vk) {
    static char b[16];
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return b[0] = (char)vk, b[1] = 0, b;
    if (vk >= VK_F1 && vk <= VK_F12) return snprintf(b, sizeof b, "F%d", vk - VK_F1 + 1), b;
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD0 + 9) return snprintf(b, sizeof b, "Num %d", vk - VK_NUMPAD0), b;
    switch (vk) {
    case VK_SPACE: return "Space";
    case VK_SHIFT: return "Shift";
    case VK_CONTROL: return "Ctrl";
    case VK_MENU: return "Alt";
    case VK_TAB: return "Tab";
    case VK_RETURN: return "Enter";
    case VK_BACK: return "Backspace";
    case VK_CAPITAL: return "Caps Lock";
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "Page Up";
    case VK_NEXT: return "Page Down";
    case VK_ADD: return "Num +";
    case VK_SUBTRACT: return "Num -";
    case VK_MULTIPLY: return "Num *";
    case VK_DIVIDE: return "Num /";
    case VK_DECIMAL: return "Num .";
    case VK_OEM_1: return ";";
    case VK_OEM_PLUS: return "=";
    case VK_OEM_COMMA: return ",";
    case VK_OEM_MINUS: return "-";
    case VK_OEM_PERIOD: return ".";
    case VK_OEM_2: return "/";
    case VK_OEM_3: return "`";
    case VK_OEM_4: return "[";
    case VK_OEM_5: return "\\";
    case VK_OEM_6: return "]";
    case VK_OEM_7: return "'";
    case VK_LBUTTON: return "Left click";   // (upstream ce23f69: the mouse's buttons, while it is captured)
    case VK_RBUTTON: return "Right click";
    case VK_MBUTTON: return "Middle click";
    case 0x05: return "Mouse 4";   // (VK_XBUTTON1)
    case 0x06: return "Mouse 5";
    }
    return snprintf(b, sizeof b, "Key %d", vk), b;
}
// the keys a binding can take (Esc cancels, Insert and F1 are the menu's, Delete clears)
const std::vector<int>& bindable_keys() {
    static std::vector<int> k;
    if (k.empty()) {
        for (int c = 'A'; c <= 'Z'; c++) k.push_back(c);
        for (int c = '0'; c <= '9'; c++) k.push_back(c);
        for (int c = VK_F2; c <= VK_F12; c++) k.push_back(c);
        for (int c = VK_NUMPAD0; c <= VK_NUMPAD0 + 9; c++) k.push_back(c);
        for (int c : {VK_SPACE, VK_SHIFT, VK_CONTROL, VK_MENU, VK_TAB, VK_RETURN, VK_BACK, VK_CAPITAL, VK_UP, VK_DOWN,
                      VK_LEFT, VK_RIGHT, VK_HOME, VK_END, VK_PRIOR, VK_NEXT, VK_ADD, VK_SUBTRACT, VK_MULTIPLY,
                      VK_DIVIDE, VK_DECIMAL, VK_OEM_1, VK_OEM_PLUS, VK_OEM_COMMA, VK_OEM_MINUS, VK_OEM_PERIOD,
                      VK_OEM_2, VK_OEM_3, VK_OEM_4, VK_OEM_5, VK_OEM_6, VK_OEM_7, VK_LBUTTON, VK_RBUTTON, VK_MBUTTON,
                      0x05, 0x06})   // (the mouse's buttons too: they count while the mouse is captured)
            k.push_back(c);
    }
    return k;
}
int csel = 0, ccol = 0;   // the controls' row (kControls: reset) and column (0 the gamepad, 1 the keyboard)
int listening = 0;     // waiting for the new input: 1 a gamepad's, 2 a key
bool listen_armed = false;   // (once what started it was let go)
int64_t listen_end = 0;
std::string keys_text(int button) {
    const int a = host->binding(HOS_BIND_KEY, button, 0), b = host->binding(HOS_BIND_KEY, button, 1);
    std::string t = a ? key_name(a) : "";
    if (b) t += std::string(a ? ", " : "") + key_name(b);
    return t.empty() ? T(S_C_NONE) : t;
}

// ---- what is drawn: bitmaps of the menu and the counter, in the window's byte order ----
struct Bitmap {
    uint32_t w = 0, h = 0;
    int top = -1;                         // (local) where it goes, from the window's top: -1 the middle
    std::vector<uint8_t> px;
    uint64_t version = 0;
    bool keyed = false;                   // (black is see-through: copied as the runs of pixels in between)
    std::vector<VkBufferImageCopy> runs;
};
Bitmap menu_bmp, fps_bmp;
std::mutex bmp_mu;
int scale_of(uint32_t h) { return std::max(1u, h / 720); }

template <class Paint>
void gdi_draw(Bitmap& out, uint32_t w, uint32_t h, Paint paint, int top = -1) {
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = (LONG)w;
    bi.bmiHeader.biHeight = -(LONG)h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    HDC dc = CreateCompatibleDC(nullptr);
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp) { DeleteDC(dc); return; }
    HGDIOBJ old = SelectObject(dc, bmp);
    paint(dc, w, h);
    GdiFlush();
    const bool rgba = dev.swap_format == VK_FORMAT_R8G8B8A8_UNORM || dev.swap_format == VK_FORMAT_R8G8B8A8_SRGB;
    std::lock_guard lk(bmp_mu);
    out.w = w, out.h = h, out.top = top;
    out.px.assign((const uint8_t*)bits, (const uint8_t*)bits + (size_t)w * h * 4);
    if (out.keyed) {
        out.runs.clear();
        for (uint32_t y = 0; y < h; y++) {
            for (uint32_t x = 0; x < w;) {
                const uint8_t* p = out.px.data() + ((size_t)y * w + x) * 4;
                if (!(p[0] | p[1] | p[2])) { x++; continue; }
                uint32_t e = x + 1;
                for (; e < w; e++) {
                    const uint8_t* q = out.px.data() + ((size_t)y * w + e) * 4;
                    if (!(q[0] | q[1] | q[2])) break;
                }
                VkBufferImageCopy c{};
                c.bufferOffset = ((uint64_t)y * w + x) * 4;
                c.bufferRowLength = w;
                c.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                c.imageOffset = {(int32_t)x, (int32_t)y, 0};
                c.imageExtent = {e - x, 1, 1};
                out.runs.push_back(c);
                x = e;
            }
        }
    }
    for (size_t i = 0; i < out.px.size(); i += 4) {
        if (rgba) std::swap(out.px[i], out.px[i + 2]);
        out.px[i + 3] = 255;
    }
    out.version++;
    SelectObject(dc, old);
    DeleteObject(bmp);
    DeleteDC(dc);
}
HFONT font(int px, int weight) {
    return CreateFontA(-px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH, "Segoe UI");
}
void fill(HDC dc, int x, int y, int w, int h, COLORREF c) {
    RECT r{x, y, x + w, y + h};
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, &r, b);
    DeleteObject(b);
}
// (local) the words are UTF-8 (the Portuguese ones' accents): drawn as UTF-16
std::wstring wide(const char* s) {
    const int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    std::wstring w(n > 1 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s, -1, w.data(), n);
    return w;
}
void put(HDC dc, int x0, int y0, int x1, int y1, const char* s, COLORREF c, UINT align) {
    const std::wstring t = wide(s);
    RECT r{x0, y0, x1, y1};
    SetTextColor(dc, c);
    DrawTextW(dc, t.c_str(), (int)t.size(), &r, align | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}
int width(HDC dc, const char* s) {
    const std::wstring t = wide(s);
    SIZE sz{};
    GetTextExtentPoint32W(dc, t.c_str(), (int)t.size(), &sz);
    return (int)sz.cx;
}

// ---- (local) the panel: laid out in units of a 1080-line window, scaled to the window's height (ui) ----
constexpr int kW = 820, kBar = 64, kTabH = 50, kTop = kBar + 3 + kTabH + 14, kRow = 46, kCRow = 32, kIRow = 34;
constexpr int kDescTop = 12, kDescW = kW - 72, kHintH = 38;   // the footer: the chosen row's description, then the keys' line
int foot_lines = 3, foot_line_h = 27;   // (measure_foot: the longest description's wrapped lines, at least 3)
constexpr int kInfoLines = 12;
// (local: Harpia) the eagle badge's ink ground and bronze rim, the game's parchment text and gold. Every text pair is WCAG AA
// (the lowest 5.3:1: gold on the selected row), and the selected row has a gold edge, for a controller's focus
const COLORREF C_BG = RGB(16, 14, 20), C_BAR = RGB(26, 22, 30), C_TABON = RGB(52, 40, 28), C_LINE = RGB(104, 82, 48),
               C_SEL = RGB(88, 64, 26), C_ACCENT = RGB(232, 184, 96), C_CHIP = RGB(118, 86, 38), C_LISTEN = RGB(150, 78, 28),
               C_TEXT = RGB(236, 224, 198), C_HI = RGB(255, 246, 222), C_DIM = RGB(176, 160, 132),
               C_BADGE_BG = RGB(92, 60, 24), C_BADGE = RGB(246, 204, 120);
float ui = 1;
int px(float v) { return (int)(v * ui + 0.5f); }
int th(float v) { return std::max(1, px(v)); }   // (a line's or a font's: never nothing)
int content_h(int tab) {
    int idx[kRows];
    return tab == TAB_CONTROLS ? kCRow * (kControls + 2) : tab == TAB_INFO ? kIRow * kInfoLines : kRow * tab_rows(tab, idx);
}
int foot_h() { return kDescTop + foot_line_h * foot_lines + 4 + kHintH; }
int panel_h(int tab) { return kTop + content_h(tab) + 8 + foot_h(); }
/// (local) The footer holds every row's description, in either language, as paint_menu wraps it (2 lines cut pt-BR's
/// Graphics text): measured at scale 1 and a little narrower than drawn, so the wrap at another scale never needs more
void measure_foot() {
    HDC dc = CreateCompatibleDC(nullptr);
    HFONT f = font(19, FW_NORMAL);   // (paint_menu's f.mid)
    HGDIOBJ old = SelectObject(dc, f);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    const int line = std::max(1, (int)tm.tmHeight);
    int most = 3;
    auto measure = [&](int id) {
        for (int l = 0; l < 2; l++) {
            const char* s = strings[id][l] ? strings[id][l] : strings[id][0];
            if (!s || !*s) continue;
            const std::wstring w = wide(s);
            RECT r{0, 0, kDescW - 16, 0};
            DrawTextW(dc, w.c_str(), (int)w.size(), &r, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
            most = std::max(most, ((int)r.bottom + line - 1) / line);
        }
    };
    for (const Row& r : rows) measure(r.desc);
    measure(S_C_HINT), measure(S_C_HINT_LISTEN);
    SelectObject(dc, old), DeleteObject(f), DeleteDC(dc);
    if (most != foot_lines || std::max(27, line) != foot_line_h) logf("vk: menu footer: %d lines of %d", most, std::max(27, line));
    foot_lines = most, foot_line_h = std::max(27, line);
}
struct Fonts {
    HFONT title, tab, body, mid, small, badge;
};

void paint_rows(HDC dc, const Fonts& f, int W) {
    int idx[kRows];
    const int n = tab_rows(cur_tab, idx), vx1 = W - px(36), vx0 = vx1 - px(300);
    for (int k = 0; k < n; k++) {
        const Row& r = rows[idx[k]];
        const int y = px(kTop + k * kRow), y1 = px(kTop + (k + 1) * kRow);
        const bool sel = k == sel_row[cur_tab];
        if (sel) fill(dc, px(16), y, W - px(32), y1 - y, C_SEL), fill(dc, px(16), y, th(4), y1 - y, C_ACCENT);
        SelectObject(dc, f.body);
        const char* label = T(r.name);
        put(dc, px(36), y, vx0 - px(8), y1, label, sel ? C_HI : C_TEXT, DT_LEFT | DT_END_ELLIPSIS);
        if (r.restart && r.restart(*r.value)) {
            const int bx = px(36) + width(dc, label) + px(12);
            SelectObject(dc, f.badge);
            const char* b = T(S_RESTART);
            const int bw = width(dc, b) + px(14), bh = px(22), by = y + (y1 - y - bh) / 2;
            if (bx + bw < vx0) fill(dc, bx, by, bw, bh, C_BADGE_BG), put(dc, bx, by, bx + bw, by + bh, b, C_BADGE, DT_CENTER);
            SelectObject(dc, f.body);
        }
        const COLORREF arrows = sel ? C_ACCENT : C_DIM;   // (< value >: the arrows in columns of their own)
        put(dc, vx0, y, vx1, y1, "<", arrows, DT_LEFT);
        put(dc, vx0, y, vx1, y1, ">", arrows, DT_RIGHT);
        put(dc, vx0 + px(20), y, vx1 - px(20), y1, r.text(*r.value), sel ? C_HI : C_TEXT, DT_CENTER | DT_END_ELLIPSIS);
    }
}
void paint_controls(HDC dc, const Fonts& f, int W) {
    const int x0 = px(36), xp = px(290), xk = px(560), x1 = W - px(36);
    if (!can_bind()) {
        SelectObject(dc, f.mid);
        put(dc, x0, px(kTop), x1, px(kTop + kCRow), T(S_C_UNAVAILABLE), C_TEXT, DT_LEFT);
        return;
    }
    SelectObject(dc, f.small);
    put(dc, x0, px(kTop), xp, px(kTop + kCRow), T(S_C_BUTTON), C_DIM, DT_LEFT);
    put(dc, xp, px(kTop), xk, px(kTop + kCRow), T(S_C_GAMEPAD), C_DIM, DT_LEFT);
    put(dc, xk, px(kTop), x1, px(kTop + kCRow), T(S_C_KEYBOARD), C_DIM, DT_LEFT);
    SelectObject(dc, f.mid);
    for (int i = 0; i <= kControls; i++) {
        const int y = px(kTop + (i + 1) * kCRow), y1 = px(kTop + (i + 2) * kCRow);
        const bool sel = i == csel;
        if (sel) fill(dc, px(16), y, W - px(32), y1 - y, C_SEL), fill(dc, px(16), y, th(4), y1 - y, C_ACCENT);
        if (i == kControls) {
            put(dc, x0, y, x1, y1, T(S_C_RESET), sel ? C_HI : C_TEXT, DT_LEFT);
            continue;
        }
        const Control& c = controls[i];
        put(dc, x0, y, xp - px(8), y1, c.fixed ? c.fixed : T(c.name), sel ? C_HI : C_TEXT, DT_LEFT | DT_END_ELLIPSIS);
        for (int col = 0; col < 2; col++) {
            std::string v = col == 0 ? (keys_only(i) ? "-" : pad_name(host->binding(HOS_BIND_PAD, c.button, 0)))
                                     : keys_text(c.button);
            if (sel && col == ccol && listening) v = T(col ? S_C_PRESS_KEY : S_C_PRESS_BUTTON);
            const int cx = col ? xk : xp, cx1 = col ? x1 : xk - px(8);
            if (sel && col == ccol)
                fill(dc, cx - px(8), y + th(3), std::min(width(dc, v.c_str()) + px(16), cx1 - cx + px(8)), y1 - y - 2 * th(3),
                     listening ? C_LISTEN : C_CHIP);
            put(dc, cx, y, cx1, y1, v.c_str(), sel ? C_HI : C_TEXT, DT_LEFT | DT_END_ELLIPSIS);
        }
    }
}
void paint_info(HDC dc, const Fonts& f, int W) {
    const int x0 = px(36), x1 = W - px(36);
    char out[32];
    snprintf(out, sizeof out, "%u x %u", dev.swap_extent.width, dev.swap_extent.height);
    const struct { int label; const char* value; } lines[kInfoLines] = {
        {S_I_GPU, dev.props.deviceName}, {S_SCALE, tx_render_scale(opts.render_scale)}, {S_I_OUTPUT, out}, {-1, nullptr},
        {S_I_KEYS, nullptr}, {S_I_KEY_MENU, ""}, {S_I_KEY_FULL, ""}, {S_I_KEY_PAD, ""}, {-1, nullptr},
        {S_I_CREDITS, nullptr}, {S_I_FSR, ""}, {S_I_NIS, ""}};
    for (int k = 0; k < kInfoLines; k++) {
        const int y = px(kTop + k * kIRow), y1 = px(kTop + (k + 1) * kIRow);
        if (lines[k].label < 0) continue;
        if (!lines[k].value) {   // (a heading)
            SelectObject(dc, f.tab);
            put(dc, x0, y, x1, y1, T(lines[k].label), C_ACCENT, DT_LEFT);
        } else if (!*lines[k].value) {   // (a line of its own)
            SelectObject(dc, f.mid);
            put(dc, x0 + px(12), y, x1, y1, T(lines[k].label), C_TEXT, DT_LEFT | DT_END_ELLIPSIS);
        } else {   // (a name and its value)
            SelectObject(dc, f.mid);
            put(dc, x0, y, x1, y1, T(lines[k].label), C_DIM, DT_LEFT);
            put(dc, x0 + px(260), y, x1, y1, lines[k].value, C_HI, DT_RIGHT | DT_END_ELLIPSIS);
        }
    }
}
void paint_menu(HDC dc, uint32_t bw, uint32_t bh) {
    const int W = (int)bw, H = (int)bh, bar = px(kBar), ty = bar + th(3), tb = ty + px(kTabH), foot = H - px(foot_h());
    fill(dc, 0, 0, W, H, C_BG);
    fill(dc, 0, 0, W, bar, C_BAR);
    fill(dc, 0, bar, W, th(3), C_ACCENT);
    const Fonts f{font(th(30), FW_SEMIBOLD), font(th(21), FW_SEMIBOLD), font(th(22), FW_NORMAL),
                  font(th(19), FW_NORMAL),   font(th(16), FW_NORMAL),   font(th(14), FW_BOLD)};
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ old = SelectObject(dc, f.title);
    put(dc, px(28), 0, W, bar, "Harpia", C_HI, DT_LEFT);
    const int title_w = width(dc, "Harpia");
    SelectObject(dc, f.small);
    put(dc, 0, 0, W - px(28), bar, "F1 / Insert", C_DIM, DT_RIGHT);
    // (upstream 40c3f85: which build, beside the title, so a report can say it without opening the log)
    put(dc, px(28) + title_w + px(14), 0, W - px(28) - width(dc, "F1 / Insert") - px(14), bar, HARPIA_VERSION, C_DIM, DT_LEFT);
    SelectObject(dc, f.tab);
    for (int t = 0, x = px(16); t < kTabs; t++) {   // the tab strip
        const char* s = T(S_TAB_DISPLAY + t);
        const int tw = width(dc, s) + px(36);
        if (t == cur_tab) fill(dc, x, ty, tw, tb - ty, C_TABON), fill(dc, x, tb - th(3), tw, th(3), C_ACCENT);
        put(dc, x, ty, x + tw, tb, s, t == cur_tab ? C_HI : C_DIM, DT_CENTER);
        x += tw + px(4);
    }
    fill(dc, 0, tb, W, th(1), C_LINE);
    if (cur_tab == TAB_CONTROLS) paint_controls(dc, f, W);
    else if (cur_tab == TAB_INFO) paint_info(dc, f, W);
    else paint_rows(dc, f, W);
    // the footer: what the chosen row does (as many lines as the longest description needs: measure_foot), then the keys
    const char* desc = "";
    const char* hint = T(S_I_HINT);
    if (cur_tab < TAB_CONTROLS) {
        int idx[kRows];
        if (tab_rows(cur_tab, idx)) desc = T(rows[idx[sel_row[cur_tab]]].desc);
        hint = T(S_HINT);
    } else if (cur_tab == TAB_CONTROLS && can_bind()) {
        desc = T(listening ? S_C_HINT_LISTEN : S_C_HINT);
        hint = T(S_C_HINT_PAD);
    }
    fill(dc, px(16), foot, W - px(32), th(1), C_LINE);
    SelectObject(dc, f.mid);
    SetTextColor(dc, C_TEXT);
    const std::wstring d = wide(desc);
    const int desc_end = kDescTop + foot_line_h * foot_lines;
    RECT dr{px(36), foot + px(kDescTop), W - px(36), foot + px(desc_end)};
    DrawTextW(dc, d.c_str(), (int)d.size(), &dr, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(dc, f.small);
    put(dc, px(36), foot + px(desc_end + 4), W - px(36), H - px(6), hint, C_DIM, DT_LEFT | DT_END_ELLIPSIS);
    fill(dc, 0, 0, W, th(1), C_LINE), fill(dc, 0, H - th(1), W, th(1), C_LINE);   // (a thin edge)
    fill(dc, 0, 0, th(1), H, C_LINE), fill(dc, W - th(1), 0, th(1), H, C_LINE);
    SelectObject(dc, old);
    for (HFONT h : {f.title, f.tab, f.body, f.mid, f.small, f.badge}) DeleteObject(h);
}
/// (local) At the window's height over 1080, smaller when the tallest tab would not fit in it; the panel's top stays where
/// the tallest one's would be, so it grows down from one line when the tab changes
void redraw_menu(uint32_t w, uint32_t h) {
    if (w < 64 || h < 64) return;
    measure_foot();
    int tallest = 0;
    for (int t = 0; t < kTabs; t++) tallest = std::max(tallest, panel_h(t));
    ui = std::min({h / 1080.0f, 0.94f * (float)h / (float)tallest, 0.94f * (float)w / (float)kW});
    gdi_draw(menu_bmp, (uint32_t)px(kW), (uint32_t)px(panel_h(cur_tab)), paint_menu, ((int)h - px(tallest)) / 2);
    if (const char* shot = getenv("RX_VK_MENU_SHOT")) {   // (hookshot 314cd6b) diagnosis: the menu as drawn, a .bmp; the frame's screenshots come before it
        std::lock_guard lk(bmp_mu);
        if (FILE* f = fopen(shot, "wb")) {
            const uint32_t bytes = menu_bmp.w * menu_bmp.h * 4;
            BITMAPFILEHEADER fh{0x4D42, (DWORD)(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + bytes), 0, 0,
                                sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)};
            BITMAPINFOHEADER ih{sizeof ih, (LONG)menu_bmp.w, -(LONG)menu_bmp.h, 1, 32, BI_RGB};
            std::vector<uint8_t> bgra(menu_bmp.px);   // (a .bmp is BGRA: gdi_draw swapped R and B for an RGBA swapchain)
            if (dev.swap_format == VK_FORMAT_R8G8B8A8_UNORM || dev.swap_format == VK_FORMAT_R8G8B8A8_SRGB)
                for (size_t i = 0; i + 3 < bgra.size(); i += 4) std::swap(bgra[i], bgra[i + 2]);
            fwrite(&fh, sizeof fh, 1, f), fwrite(&ih, sizeof ih, 1, f), fwrite(bgra.data(), 1, bytes, f);
            fclose(f);
        }
    }
}

char fps_text[48] = "";
char fps_sub[32] = "";
/// The usual overlay: a bold green number with a dark edge, nothing behind it. Hard-edged letters: see-through pixels are copied or not.
void outlined(HDC dc, int x, int y, const char* text, COLORREF colour, int edge) {
    SetTextColor(dc, RGB(6, 8, 6));
    const int n = (int)strlen(text);
    for (int dy = -edge; dy <= edge; dy++)
        for (int dx = -edge; dx <= edge; dx++)
            if (dx || dy) TextOutA(dc, x + dx, y + dy, text, n);
    SetTextColor(dc, colour);
    TextOutA(dc, x, y, text, n);
}
HFONT hard_font(int px, int weight) {
    return CreateFontA(-px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                       DEFAULT_PITCH, "Segoe UI");
}
void paint_fps(HDC dc, uint32_t w, uint32_t h, int s) {
    fill(dc, 0, 0, (int)w, (int)h, RGB(0, 0, 0));
    SetBkMode(dc, TRANSPARENT);
    SetTextAlign(dc, TA_RIGHT | TA_TOP);
    const COLORREF green = RGB(72, 255, 110);
    const int right = (int)w - 2 * s;
    HFONT label = hard_font(13 * s, FW_BOLD), big = hard_font(28 * s, FW_BOLD);
    HGDIOBJ old = SelectObject(dc, label);
    outlined(dc, right, 14 * s, "FPS", green, s);
    SelectObject(dc, big);
    outlined(dc, right - 30 * s, 0, fps_text, green, s);
    if (fps_sub[0]) {
        SelectObject(dc, label);
        outlined(dc, right, 32 * s, fps_sub, RGB(226, 232, 226), s);
    }
    SelectObject(dc, old);
    DeleteObject(label);
    DeleteObject(big);
}

std::atomic<int64_t> game_frames{0}, shown_frames{0};
int cur_scale = 1;

struct Upload {
    VkBuffer buf = VK_NULL_HANDLE;
    VmaAllocation alloc = nullptr;
    void* map = nullptr;
    uint64_t size = 0, version = 0;
};
Upload ups[2][3];   // (the menu's, the counter's; a frame in flight's each)

void copy_over(VkCommandBuffer cmd, VkImage dst, Bitmap& b, Upload& u, int x, int y, uint32_t w, uint32_t h) {
    std::lock_guard lk(bmp_mu);
    if (!b.w || (uint32_t)x + b.w > w || (uint32_t)y + b.h > h) return;
    const uint64_t size = (uint64_t)b.w * b.h * 4;
    if (u.size < size) {
        if (!create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, u.buf, u.alloc, &u.map)) return;
        u.size = size, u.version = 0;
    }
    if (u.version != b.version) std::memcpy(u.map, b.px.data(), size), u.version = b.version;
    if (b.keyed) {
        static thread_local std::vector<VkBufferImageCopy> at;
        at = b.runs;
        for (auto& r : at) r.imageOffset.x += x, r.imageOffset.y += y;
        if (!at.empty()) vkCmdCopyBufferToImage(cmd, u.buf, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)at.size(), at.data());
        return;
    }
    VkBufferImageCopy c{};
    c.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    c.imageOffset = {x, y, 0};
    c.imageExtent = {b.w, b.h, 1};
    vkCmdCopyBufferToImage(cmd, u.buf, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &c);
}

// ---- the keys: pressed this frame ----
enum { K_MENU, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_ESC, K_F1, K_ENTER, K_DEL, K_BACK, K_PREV, K_NEXT, K_Q, K_E, K_N };
/// An options tab's: Up and Down choose, Left and Right change (Enter as Right)
bool rows_input(const bool* press) {
    int idx[kRows];
    const int n = tab_rows(cur_tab, idx);
    if (!n) return false;
    int& s = sel_row[cur_tab];
    bool changed = false;
    if (press[K_UP]) s = (s + n - 1) % n, changed = true;
    if (press[K_DOWN]) s = (s + 1) % n, changed = true;
    const int dir = press[K_RIGHT] || press[K_ENTER] ? 1 : press[K_LEFT] ? -1 : 0;
    if (dir) {
        Row& r = rows[idx[s]];
        *r.value = r.value == &opts.mouse_sens ? std::clamp(*r.value + dir * r.step, r.lo, r.hi) : stepped(r, dir);
        if (r.value == &opts.pad_layout) apply_pad_layout(opts.pad_layout);
        if (r.value == &opts.dlssg && opts.dlssg) opts.framegen = 0;   // (hookshot: one frame generation at a time)
        if (r.value == &opts.framegen && opts.framegen) opts.dlssg = 0;
        changed = true;
        // (upstream f447272: the preset sets its rows; a change of one of those makes it Custom)
        if (r.value == &opts.preset) apply_preset(opts.preset);
        else if (preset_row(r.value)) opts.preset = 0;
        legacy_sync();
        save();
    }
    return changed;
}
/// (hookshot) The Controls tab's: Enter waits for the new input, Delete clears, Esc cancels the wait or closes
bool controls_input(const bool* press, uint32_t raw, uint32_t pad_press, bool mine) {
    if (!can_bind()) {
        if (press[K_ESC]) open_now = false;
        return false;
    }
    bool changed = false;
    if (listening) {
        if (press[K_ESC] || prof_now() > listen_end) listening = 0, changed = true;   // (Esc, or nothing for a while)
        else if (listening == 1) {
            if (!listen_armed) listen_armed = (raw & ~(1u << HOS_PAD_GUIDE)) == 0;
            else if (const uint32_t p = pad_press & ~(1u << HOS_PAD_GUIDE)) {
                int bit = 0;
                while (!(p >> bit & 1)) bit++;
                host->bind(HOS_BIND_PAD, controls[csel].button, 0, bit);
                listening = 0, changed = true;
                opts.pad_layout = detect_pad_layout();
                save();
            }
        } else {
            int down = 0;
            for (int k : bindable_keys())
                if (mine && (GetAsyncKeyState(k) & 0x8000)) {
                    down = k;
                    break;
                }
            if (!listen_armed) listen_armed = !down;
            else if (down) {
                host->bind(HOS_BIND_KEY, controls[csel].button, 0, down);
                host->bind(HOS_BIND_KEY, controls[csel].button, 1, 0);
                listening = 0, changed = true;
                save();
            }
        }
        return changed;
    }
    if (press[K_ESC]) {
        open_now = false;
        return false;
    }
    if (press[K_UP]) csel = (csel + kControls) % (kControls + 1), changed = true;
    if (press[K_DOWN]) csel = (csel + 1) % (kControls + 1), changed = true;
    if ((press[K_LEFT] || press[K_RIGHT]) && csel < kControls && !keys_only(csel)) ccol ^= 1, changed = true;
    if (keys_only(csel)) ccol = 1;   // (the stick's and the camera's directions: keys only)
    if (press[K_ENTER]) {
        if (csel == kControls) host->bind(HOS_BIND_PAD, -1, 0, 0), opts.pad_layout = 0, save();
        else listening = ccol ? 2 : 1, listen_armed = false, listen_end = prof_now() + 6000000000LL;
        changed = true;
    }
    if ((press[K_DEL] || press[K_BACK]) && csel < kControls) {
        if (ccol == 0) host->bind(HOS_BIND_PAD, controls[csel].button, 0, -1), opts.pad_layout = detect_pad_layout();
        else for (int k = 0; k < 2; k++) host->bind(HOS_BIND_KEY, controls[csel].button, k, 0);
        changed = true;
        save();
    }
    return changed;
}

} // namespace

void menu_init() {
    // (local: also from the program cache's start, vk_shader.cpp disk_init, maybe on a worker: once, the others wait for it)
    static std::mutex mu;
    std::lock_guard lk(mu);
    static bool done;
    if (done) return;
    done = true;
    language_load();
    bool seen[kRows] = {};
    bool has_options = false;
    if (FILE* f = fopen(ini_path().c_str(), "r")) {
        has_options = true;
        char line[256];
        bool has_resolution = false;
        bool v2 = false;
        int old_dynres = -1;   // (the row before Resolution: 0, the full scale, is its 100%; 1 is Auto)
        while (fgets(line, sizeof line, f)) {
            char* eq = strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            const int v = atoi(eq + 1);
            for (int i = 0; i < kRows; i++)
                if (!strcmp(line, rows[i].key)) *rows[i].value = v, seen[i] = true;
            if (!strcmp(line, "resolution")) opts.resolution = std::clamp(v, 0, 4), has_resolution = true;
            if (!strcmp(line, "ultrawide")) opts.ultrawide = v != 0;
            if (!strcmp(line, "dynamic_resolution")) old_dynres = v;
            v2 |= !strcmp(line, "options_version");
        }
        fclose(f);
        if (!has_resolution && old_dynres >= 0) opts.resolution = old_dynres ? 0 : 1;
        // (Game speed is Real time by default now: Per frame played at half speed at 30 fps, the "slow motion" players
        // reported. An options file from before kept the old default whether chosen or not: Real time for it too)
        if (!v2) opts.realtime = 1;
    }
    // (local) the legacy keys, read when the new ones are not there: resolution's 0 Auto, 1 100%, 2 75%, 3 67%, 4 50% as the
    // render scale, ultrawide's 1 as Fill
    static const int from_resolution[] = {0, 100, 75, 67, 50};
    if (!seen[row_of(&opts.render_scale)]) opts.render_scale = from_resolution[opts.resolution];
    if (!seen[row_of(&opts.aspect)]) opts.aspect = opts.ultrawide ? 2 : 0;
    // (upstream 068b505) An opt-in profile for 4 GB cards, only for a first start: an options file stays as it is.
    // (local: the preset's rows, no budget, 598226f)
    if (!has_options && getenv("RX_VK_LOW_SPEC") && atoi(getenv("RX_VK_LOW_SPEC")) != 0) {
        opts.preset = 1, apply_preset(1);
        opts.framegen = 0, opts.upscale = 0, opts.sharpness = 0;
    }
    if (opts.framegen == 1) opts.framegen = 2;
    if (const char* e = getenv("RX_VK_FRAMEGEN_MULT")) opts.framegen = std::min(std::max(atoi(e), 2), 8);
    if (const char* e = getenv("RX_VK_PACE")) opts.pace = atoi(e) != 0;
    if (const char* e = getenv("RX_VK_UPSCALE")) opts.upscale = atoi(e);
    if (const char* e = getenv("HOS_REALTIME_STEP")) opts.realtime = atoi(e) != 0;
    if (const char* e = getenv("HOS_DYNRES")) opts.resolution = atoi(e) != 0 ? 0 : 1, opts.render_scale = opts.resolution ? 100 : 0;
    for (Row& r : rows) *r.value = snap(r, *r.value);   // (local: only the values a row takes)
    legacy_sync();   // (local, review: a stale resolution= beside render_scale= no longer decides the scale)
    aspect_start = opts.aspect;
    robust_start = opts.robust;
    // (local) Fog: RX_VK_OOT_VISUAL decides when it is set (harpia_env.txt); else the option, put in the environment before any
    // program is translated, so disk_stamp's RX_VK_OOT prefix keeps the program cache to it
    if (const char* e = getenv("RX_VK_OOT_VISUAL")) opts.classic_fog = atoi(e) != 0;
    else {
#ifdef _WIN32
        _putenv_s("RX_VK_OOT_VISUAL", opts.classic_fog ? "1" : "0");
#else
        setenv("RX_VK_OOT_VISUAL", opts.classic_fog ? "1" : "0", 0);
#endif
    }
    fog_start = opts.classic_fog;
}

// (hookshot: DLSS-G's 3x and 4x need an RTX 50 (Blackwell); other cards are offered 2x only. The device is known by now)
static void dlssg_row_limit() {
    const char* n = dev.props.deviceName;
    rows[row_of(&opts.dlssg)].n = strstr(n, "RTX 50") || strstr(n, "Blackwell") ? 4 : 2;
}

void menu_set_gpu_count(int count) {   // (vk_main.cpp, once it has listed the devices)
    Row& r = rows[row_of(&opts.gpu_device)];
    r.hi = std::max(count, 0);
    if (opts.gpu_device > r.hi) opts.gpu_device = 0;
    gpu_device_at_start = opts.gpu_device;
}

void menu_option_set(int id, int value) {   // (from the game's side: 7 the mouse's sensitivity, the - and = keys)
    if (id != 7) return;
    opts.mouse_sens = std::clamp(value, 10, 500);   // (while the menu is closed: it shows it when opened)
    menu_refresh = true;
    save();
}

bool menu_is_open() { return open_now.load(std::memory_order_relaxed); }

void menu_note_frame(bool game_frame) {
    shown_frames.fetch_add(1, std::memory_order_relaxed);
    if (game_frame) game_frames.fetch_add(1, std::memory_order_relaxed);
}

// The Fullscreen row and F11 are one setting: the row asks the window's thread (host/display.c), F11 shows in the row
void fullscreen_tick() {
    static int applied = 0, settle = 0;
    const HWND main_window = app_window();
    if (!main_window) return;
    static const bool env = getenv("HOS_FULLSCREEN") != nullptr;   // (a test's own choice: the row is left alone)
    if (env) return;
    if (opts.fullscreen != applied) {
        applied = opts.fullscreen;
        settle = 60;
#ifdef _WIN32
        PostMessageA(main_window, WM_APP + 7, (WPARAM)applied, 0);
#else   // (hookshot) Linux: the window's own call, which hands it to the window's thread (host/linux/display_sdl.c)
        const HosLinuxWindow* lw = (const HosLinuxWindow*)main_window;
        if (lw->set_fullscreen) lw->set_fullscreen(lw->native, applied);
#endif
        return;
    }
    if (settle > 0) { settle--; return; }
#ifdef _WIN32
    const int now = !(GetWindowLongA(main_window, GWL_STYLE) & WS_CAPTION);
#else
    const HosLinuxWindow* lw = (const HosLinuxWindow*)main_window;
    const int now = lw->fullscreen ? lw->fullscreen(lw->native) : applied;
#endif
    if (now != applied) {
        opts.fullscreen = applied = now;
        menu_refresh = true;
        save();
    }
}

/// A rate over the half-second that just ended. The first call only starts that interval: the frames since launch,
/// divided by one second, were shown as the rate the moment the counter was turned on.
bool fps_take(int64_t now, int64_t shown, int64_t game, int64_t* next, int64_t* last_shown, int64_t* last_game,
              double* shown_rate, double* game_rate) {
    if (now < *next) return false;
    const bool have = *next != 0;
    if (have) {
        const double secs = (now - *next + 500000000) / 1e9;
        *shown_rate = (shown - *last_shown) / secs;
        *game_rate = (game - *last_game) / secs;
    }
    *last_shown = shown;
    *last_game = game;
    *next = now + 500000000;
    return have;
}

void menu_tick() {
    fullscreen_tick();
    static bool held[K_N];
    static const int keys[K_N] = {VK_INSERT, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_ESCAPE, VK_F1,   // (F1: for keyboards without Insert)
                                  VK_RETURN, VK_DELETE, VK_BACK, VK_PRIOR, VK_NEXT, 'Q', 'E'};
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    if (fg) GetWindowThreadProcessId(fg, &pid);
    const bool mine = pid == GetCurrentProcessId();
    bool press[K_N] = {};
    for (int i = 0; i < K_N; i++) {
        const bool down = mine && (GetAsyncKeyState(keys[i]) & 0x8000) != 0;
        press[i] = down && !held[i];
        held[i] = down;
    }
    // (review: Alt+Enter is fullscreen, host/display.c; not Enter on the chosen row too, as nn_hid.c's kmap_down)
    if (press[K_ENTER] && (GetAsyncKeyState(VK_MENU) & 0x8000)) press[K_ENTER] = false;
    // (hookshot) the gamepads, by position: Home, or Back and Start together, as Insert; LB and RB as Q and E, the D-pad
    // as the arrows, the bottom face button as Enter, the right one as Esc (not while a binding waits for its button)
    static uint32_t pad_held = 0;
    const uint32_t raw = mine && can_bind() ? host->pad_raw() : 0, pad_press = raw & ~pad_held;
    const uint32_t chord = 1u << HOS_PAD_BACK | 1u << HOS_PAD_START;
    const bool chord_now = (raw & chord) == chord && (pad_held & chord) != chord;
    pad_held = raw;
    auto pad = [&](int p) { return listening != 1 && (pad_press >> p & 1) != 0; };
    if (pad(HOS_PAD_GUIDE) || chord_now) press[K_MENU] = true;
    if (open_now.load() && listening != 1) {
        press[K_UP] |= pad(HOS_PAD_UP), press[K_DOWN] |= pad(HOS_PAD_DOWN), press[K_LEFT] |= pad(HOS_PAD_LEFT);
        press[K_RIGHT] |= pad(HOS_PAD_RIGHT), press[K_ESC] |= pad(HOS_PAD_EAST), press[K_ENTER] |= pad(HOS_PAD_SOUTH);
        press[K_PREV] |= pad(HOS_PAD_LSHOULDER), press[K_NEXT] |= pad(HOS_PAD_RSHOULDER);
    }
    static uint32_t last_w = 0, last_h = 0;   // (local: a new window size draws it again, at its new scale)
    const uint32_t sw = dev.swap_extent.width, sh = dev.swap_extent.height;
    bool changed = sw != last_w || sh != last_h || menu_refresh;
    last_w = sw, last_h = sh;
    menu_refresh = false;
    const int scale = cur_scale = scale_of(sh);
    if (press[K_MENU] || press[K_F1]) {
        open_now = !open_now.load(), changed = true;
        listening = 0;
        // (upstream f1b5725: the host keeps the bindings, a saved layout is not applied over them at start)
        if (open_now.load()) opts.pad_layout = detect_pad_layout(), dlssg_row_limit();
    }
    {   // (hookshot) RX_VK_MENU_OPEN=1 (diagnosis): the menu opened at the first frame, as Insert would, with no keyboard
        static bool forced = false;
        if (!forced && getenv("RX_VK_MENU_OPEN")) {   // (=<tab>: on that tab, e.g. controls, for screenshots of each)
            forced = true, open_now = true, changed = true;
            static const char* const tab_names[kTabs] = {"display", "graphics", "performance", "input", "controls", "info"};
            for (int t = 0; t < kTabs; t++)
                if (!strcmp(getenv("RX_VK_MENU_OPEN"), tab_names[t])) cur_tab = t;
            // (hookshot: the rows each options tab shows, logged: the DLSS rows appear only with their DLLs)
            for (int tab : {TAB_DISPLAY, TAB_GRAPHICS, TAB_PERF, TAB_INPUT}) {
                int idx[kRows];
                const int n = tab_rows(tab, idx);
                std::string keys;
                for (int i = 0; i < n; i++) keys += std::string(" ") + rows[idx[i]].key;
                logf("vk: menu tab %d rows:%s", tab, keys.c_str());
            }
            // (local) RX_VK_MENU_ROW=<n>: that row of the tab chosen, so a screenshot shows its description in the footer
            if (const char* r = getenv("RX_VK_MENU_ROW"); r && cur_tab < TAB_CONTROLS) {
                int idx[kRows];
                const int n = tab_rows(cur_tab, idx), want = atoi(r);
                if (want >= 0 && want < n) sel_row[cur_tab] = want;
            }
        }
    }
    if (open_now.load()) {
        // (local) Q and E, Page Up and Page Down, LB and RB: the tab before or after (not while a binding waits: Q is a key too)
        const int dtab = listening ? 0 : (press[K_NEXT] || press[K_E]) - (press[K_PREV] || press[K_Q]);
        if (dtab) cur_tab = (cur_tab + dtab + kTabs) % kTabs, changed = true;
        else if (cur_tab == TAB_CONTROLS) changed |= controls_input(press, raw, pad_press, mine);
        else if (press[K_ESC]) open_now = false;
        else if (cur_tab != TAB_INFO) changed |= rows_input(press);
        if (changed && open_now.load()) redraw_menu(sw, sh);
    }
    if (opts.show_fps) {
        static int64_t next = 0, last_game = 0, last_shown = 0;
        const int64_t now = prof_now();
        double shown = 0, game = 0;
        if (fps_take(now, shown_frames.load(), game_frames.load(), &next, &last_shown, &last_game, &shown, &game)) {
            snprintf(fps_text, sizeof fps_text, "%.0f", shown);
            if (opts.framegen >= 2) snprintf(fps_sub, sizeof fps_sub, "game %.0f", game);
            else fps_sub[0] = 0;
            fps_bmp.keyed = true;
            gdi_draw(fps_bmp, 120u * scale, 54u * scale, [scale](HDC dc, uint32_t w, uint32_t h) { paint_fps(dc, w, h, scale); });
        }
    }
}

void menu_record(VkCommandBuffer cmd, VkImage dst, int slot, uint32_t w, uint32_t h) {
    if (opts.show_fps) {
        // The game image was just blitted (or cleared) into dst. Its write must
        // finish before the FPS pixels overwrite it; command order alone does
        // not order these transfer writes. The menu below has its own barrier.
        VkMemoryBarrier2 mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
        mb.srcStageMask = mb.dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
        mb.srcAccessMask = mb.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        VkDependencyInfo di{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        di.memoryBarrierCount = 1, di.pMemoryBarriers = &mb;
        vkCmdPipelineBarrier2(cmd, &di);
        std::unique_lock lk(bmp_mu);
        const uint32_t fw = fps_bmp.w;
        lk.unlock();
        if (fw) copy_over(cmd, dst, fps_bmp, ups[1][slot], (int)w - (int)fw - (int)(12 * cur_scale), (int)(10 * cur_scale), w, h);
    }
    if (open_now.load(std::memory_order_relaxed)) {
        std::unique_lock lk(bmp_mu);
        const uint32_t mw = menu_bmp.w, mh = menu_bmp.h;
        const int top = menu_bmp.top;
        lk.unlock();
        // (one copy finished before the next: on a Radeon the menu came out with its top torn away beside the FPS counter)
        VkMemoryBarrier2 mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
        mb.srcStageMask = mb.dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
        mb.srcAccessMask = mb.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        VkDependencyInfo di{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        di.memoryBarrierCount = 1, di.pMemoryBarriers = &mb;
        vkCmdPipelineBarrier2(cmd, &di);
        // (local: at the top it was drawn for; the middle when that no longer fits, a window size it was not drawn for yet)
        const int y = top >= 0 && (uint64_t)top + mh <= h ? top : ((int)h - (int)mh) / 2;
        if (mw) copy_over(cmd, dst, menu_bmp, ups[0][slot], ((int)w - (int)mw) / 2, y, w, h);
    }
}

} // namespace mkwvk
