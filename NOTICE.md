# Licenças e créditos de terceiros

Harpia é uma versão modificada do **Hookshot** (oot-recomp), de NotRexed e colaboradores, distribuída sob a GNU General
Public License, versão 3 ou posterior ([LICENSE](LICENSE)). A Harpia continua sob a mesma licença.

**Modificações:** em 2026, Leandro Felix renomeou o projeto para Harpia (executável, janela, menu, arquivos de opções,
de registro e de cache, Builder e ícone) e o adaptou para uso pessoal (menu em abas em pt-BR, escaladores FSR 1 e NIS,
proporção, escala de renderização, filtragem anisotrópica, VSync, correções para AMD e mudanças de colaboradores
incorporadas). O histórico do projeto original está no repositório dele (github.com/NotRexed/Hookshot).

Nos fontes, os comentários marcados `(hookshot)` indicam alterações do Hookshot sobre o código de que ele deriva (Eden,
Sirit, fmt, NIS e outros), e `Hookshot issue N` / `Hookshot PR N` remetem ao rastreador do projeto original. Eles foram
mantidos de propósito: são os avisos de modificação e de autoria que a GPL (seção 5a) e as outras licenças pedem. Os
marcados `(local)` são alterações desta versão.

## Créditos (do projeto original)

- **NotRexed**: Hookshot, BlueShell (Mario Kart World), Skybound (Tears of the Kingdom) e as ferramentas a64-kit.
- **[Eden](https://git.eden-emu.dev/eden-emu/eden)** e **Eglantine**: o recompilador de shaders e o trabalho com shaders
  do Switch 2.
- **suyu**: os decodificadores gráficos e o tradutor AArch64.
- **[seedds/nxnative](https://github.com/seedds/nxnative)**: referência de como funcionam task e mesh shaders.
- **[zortos293/Lichii](https://github.com/zortos293/Lichii)**: referência para o rebaixamento de shaders Ampere.
- **[kinnay/zbic](https://github.com/kinnay/zbic)**, **Opus** e **SDL**: descompressão, áudio e controles.
- **[sostenesfreitas/ocarina-recomp](https://github.com/sostenesfreitas/ocarina-recomp)**: o layout do README original e
  as notas sobre a seção de controle do estágio mesh.
- **[MythosOfs/Orchid](https://github.com/MythosOfs/Orchid)**: a correção de névoa e exposição de Ocarina of Time.
- **Mesa (NAK)**: a referência da codificação de instruções Ampere.
- Quem enviou correções: klimovich008, d3zessete, chrispository, vzeol, monsterlag, leonidasmark, joaoalevato, shayder,
  lev92, AyyBeeDee20738, entre outros.

## Componentes e as suas licenças

| Componente | Onde | Licença / aviso |
|---|---|---|
| Hookshot (base deste projeto) | todo o repositório | GPL-3.0-or-later, [LICENSE](LICENSE) |
| Eden, suyu, Orchid | `gpu/` | GPL; origem em [gpu/PROVENANCE.md](gpu/PROVENANCE.md) |
| Camada de sistema | `host/` | origem em [host/PROVENANCE.md](host/PROVENANCE.md) |
| Sirit | `gpu/vk/sirit/` | BSD-3-Clause, o que os cabeçalhos dizem: [LICENSE.BSD-3-Clause](gpu/vk/sirit/LICENSE.BSD-3-Clause); origem e os textos AGPL/GPL do Eden em [PROVENANCE.md](gpu/vk/sirit/PROVENANCE.md) |
| AMD FidelityFX Super Resolution 1 | `gpu/vk/shaders/ffx/`, `fsr_easu.comp`, `fsr_rcas.comp` | MIT, Advanced Micro Devices (cabeçalho dos arquivos) |
| NVIDIA Image Scaling | `gpu/vk/shaders/nis/` | MIT, NVIDIA Corporation (cabeçalho dos arquivos) |
| a64-kit | `third_party/a64-kit/` | [PROVENANCE.md](third_party/a64-kit/PROVENANCE.md), [NOTICE.TOTKPort.md](third_party/a64-kit/NOTICE.TOTKPort.md) |
| zbic (zstd com ZBIC; ferramenta separada, não ligada ao jogo) | `third_party/zbic/` | zstd: BSD-3-Clause ([LICENSE.zstd](third_party/zbic/LICENSE.zstd), a opção BSD da licença dupla BSD ou GPLv2); divsufsort: MIT (Yuta Mori), no próprio `zstd.c`; [PROVENANCE.md](third_party/zbic/PROVENANCE.md) |
| Opus | `third_party/opus/` | [COPYING](third_party/opus/COPYING) |
| SDL2 | `third_party/sdl2/` | [LICENSE.txt](third_party/sdl2/LICENSE.txt) |
| stb_truetype (só na versão Linux) | `third_party/stb/` | domínio público ou MIT, [PROVENANCE.md](third_party/stb/PROVENANCE.md) |
| Vulkan Memory Allocator, Vulkan Headers, SPIR-V Headers, FFmpeg, wgpu-native | baixados na compilação | as licenças dos próprios pacotes |

O menu do jogo (aba Info) mostra os créditos do FSR 1 e do NIS.

Harpia é um projeto de fã não oficial, sem vínculo com a Nintendo nem endossado por ela. The Legend of Zelda e Ocarina
of Time são marcas registradas da Nintendo.

## Textos das licenças

Cópias dos textos citados nos cabeçalhos SPDX estão em [LICENSES/](LICENSES/README.md).

## Arquivos sem cabeçalho próprio

Os arquivos de `host/`, `nx2/` e `third_party/a64-kit/` que não trazem cabeçalho de licença vêm do Hookshot (NotRexed e
colaboradores), que se declara GPL-3.0-or-later; estão cobertos pelo [LICENSE](LICENSE) da raiz. A Harpia integrou o
Hookshot até o commit `a1b19a3` do seu ramo `upstream-next` (e antes até `b96b5bc`): são as bases do histórico.
Os cabeçalhos de terceiros (Citra, Dolphin, PPSSPP, FasTC, Boost e outros em `gpu/third_party/suyu/`) foram mantidos
como vieram; os textos das licenças estão em [LICENSES/](LICENSES/README.md).

