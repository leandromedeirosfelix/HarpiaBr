# Créditos e agradecimentos

[English](CREDITS.en.md) · **Português**

A Harpia só existe porque muita gente abriu o seu trabalho. **Este arquivo diz com clareza quem fez o quê.** A Harpia é uma versão do **Hookshot** (de NotRexed e colaboradores): ela **não esconde** a origem e **mantém todos os avisos de licença e autoria**, como a GPL exige. O texto jurídico de cada componente está em [NOTICE.md](NOTICE.md) e nos arquivos `PROVENANCE.md` / `NOTICE.*` de cada pasta; este arquivo é o resumo humano.

Se você é autor de algo aqui e o crédito está errado, incompleto ou faltando, [abra uma issue](../../issues/new/choose): corrigimos na hora.

## A base

| Projeto | Quem | O que a Harpia herda | Licença |
|---|---|---|---|
| **[Hookshot](https://github.com/NotRexed/Hookshot)** (oot-recomp) | **NotRexed** e colaboradores | A recompilação estática inteira, o sistema Horizon reimplementado (`host/`), a camada gráfica NVN2, o renderizador Vulkan e o Builder. **A Harpia é uma versão modificada dele.** | GPL-3.0-or-later |
| **a64-kit / Rexcomp / TOTKPort / BlueShell / Skybound** | NotRexed | O kit de tradução AArch64 para C e o runtime do qual o `host/` descende (`third_party/a64-kit/`, `host/PROVENANCE.md`) | GPL (via suyu) |

Colaboradores do Hookshot que enviaram correções (citados no projeto original): klimovich008, d3zessete, chrispository, vzeol, monsterlag, leonidasmark, joaoalevato, shayder, lev92, AyyBeeDee20738, entre outros.

## Gráficos e shaders

| Projeto | Papel | Licença |
|---|---|---|
| **[Eden](https://git.eden-emu.dev/eden-emu/eden)** e **Eglantine** | O recompilador de shaders (`gpu/vk/eden/`) e o trabalho de rebaixar shaders Ampere do Switch 2 para Maxwell | GPL-3.0-or-later |
| **yuzu / suyu** | Recompilador de shaders, decodificadores gráficos e o tradutor AArch64 para C (`gpu/third_party/suyu/`, `third_party/a64-kit/lift/suyu/`) | GPL-2.0-or-later / GPL-3.0-or-later por arquivo |
| **Sirit** (ReinUsesLisp; fork do Eden) | Gerador de SPIR-V usado pelo recompilador (`gpu/vk/sirit/`) | BSD-3-Clause (`LICENSE.BSD-3-Clause`; o Eden acrescentou textos AGPL/GPL, veja `PROVENANCE.md`) |
| **Mesa (NAK)** | Referência da codificação de instruções Ampere | consulta de referência |
| **Vulkan Memory Allocator** (AMD) | Alocação de memória de vídeo; baixado na compilação | MIT |
| **AMD FidelityFX Super Resolution 1** | Ampliador FSR 1 (`gpu/vk/shaders/ffx/`) | MIT, Advanced Micro Devices |
| **NVIDIA Image Scaling** | Ampliador NIS (`gpu/vk/shaders/nis/`) | MIT, NVIDIA Corporation |
| **RenderDoc** | Cabeçalho da API de captura | MIT |
| **h1k421** e **jhaizhou** (d2r-mobile / MacNativeGames) | Tipos da API NVN e o desenho da camada NVN nativa | MIT |
| **fincs** (deko3d) | Layout de armazenamento de texturas NVN | zlib |

## Projetos de pesquisa e correção

Estes projetos abertos foram lidos, medidos e usados como referência; o que foi levado para cá tem a linha correspondente em [gpu/PROVENANCE.md](gpu/PROVENANCE.md) ou [host/PROVENANCE.md](host/PROVENANCE.md), com o commit de origem.

| Projeto | Contribuição para a Harpia |
|---|---|
| **[sostenesfreitas/ocarina-recomp](https://github.com/sostenesfreitas/ocarina-recomp)** | Pesquisa sobre o jogo (seção de controle do estágio mesh, DEPBAR, captura de programa, máscaras de escrita de cor). O layout do README original também veio daqui. |
| **[zortos293/Lichii](https://github.com/zortos293/Lichii)** | Referência do rebaixamento de shaders Ampere e do caminho de DLSS no jogo |
| **[seedds/nxnative](https://github.com/seedds/nxnative)** | Documentação de task e mesh shaders (mapa de índices por blocos de 32 primitivas) e do censo de shaders |
| **MacNativeGames** (jhaizhou / ly05010419) | O desenho da camada NVN e dos núcleos virtuais |
| **[TotK Recomp](https://github.com/valentderah/totk-pc-recomp)** (valentderah) | Referência de desempenho e de núcleos virtuais (a origem MIT está nos avisos do a64-kit) |
| **EvilOOT** | Correções de folhagem e iluminação (acesso a buffers por ponteiro, `OpArrayLength`) incorporadas pela atualização do projeto original |
| **[MythosOfs/Orchid](https://github.com/MythosOfs/Orchid)** | A correção de névoa e exposição do jogo |
| **dkb-pc-recomp**, **LASRecomp**, **SavageWindRecomp**, **Ryujinx** | Estudados como referência de pesquisa (diagnóstico de perda de dispositivo, descritores, painel de desempenho, comportamento do hardware). Nenhuma cópia de código deles é afirmada aqui sem uma linha nos `PROVENANCE.md`. |

### Código herdado do yuzu / suyu / Eden

Os cabeçalhos desses arquivos citam, e nós mantemos: Citra Emulator Project, Dolphin Emulator Project, PPSSPP Project, Gekko Emulator, Tony Wasserka, Jannik Vogel, degasus (WTFPL), a Universidade da Carolina do Norte / FasTC (Apache-2.0), Boost (BSL-1.0) e Austin Appleby (domínio público), entre outros. Os textos das licenças estão nos próprios arquivos ou em `gpu/third_party/suyu/`.

**mkw-recomp** (de NotRexed, o port de Mario Kart World) é a linha de que o Hookshot descende; vários caminhos e comentários o citam.

## Bibliotecas

| Biblioteca | Uso | Licença |
|---|---|---|
| **[SDL2](https://www.libsdl.org)** | Janela, áudio e controles (`third_party/sdl2/`) | zlib |
| **[Opus](https://opus-codec.org)** 1.5.2 | Áudio | BSD-3-Clause (`third_party/opus/COPYING`) |
| **zbic** (kinnay) / **Atmosphere-NX** | Descompressão dos módulos do jogo; ferramenta separada, não ligada ao jogo | zstd e o código ZBIC (hexkyz): BSD ou GPLv2, usado sob a BSD-3-Clause; divsufsort (Yuta Mori): MIT |
| **zstd** | Decodificação | BSD ou GPLv2 |
| **stb_truetype** (Sean Barrett) | Texto no menu da versão Linux | domínio público ou MIT |
| **wgpu-native**, **FFmpeg**, **dav1d**, **Vulkan / SPIR-V Headers** | Baixados ou usados na compilação, com as suas próprias licenças | MIT/Apache-2.0, LGPL, BSD-2, Apache-2.0 |

## O que é da Harpia

**Leandro Felix**: o nome, a identidade visual, o menu em abas em português e inglês, os ampliadores FSR 1 e NIS no menu, as opções de proporção, escala, filtragem e VSync, as correções para AMD, a integração das atualizações do projeto original, o cache de shaders robusto, o detector de quadros escuros e esta documentação.

Ferramentas de desenvolvimento: parte do código e da documentação foi feita com a ajuda de assistentes de IA (Claude, da Anthropic), com as mudanças integradas e testadas no PC do autor.

### Arte

O emblema e o banner foram criados com ajuda de IA generativa e compostos para este projeto. Não usam arte oficial da Nintendo.

## Licença

GPL-3.0-or-later; veja [LICENSE](LICENSE) e [NOTICE.md](NOTICE.md).

## Textos das licenças

Veja [LICENSES/](LICENSES/README.md).
