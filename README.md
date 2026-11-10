<p align="center">
  <img src="docs/assets/banner.png" alt="Harpia" width="100%">
</p>

<p align="center">
  <b>Português</b> &nbsp;·&nbsp; <a href="README.en.md">English</a>
</p>

<p align="center">
  <img alt="Licença GPL-3.0-or-later" src="https://img.shields.io/badge/licen%C3%A7a-GPL--3.0--or--later-6b4fbb">
  <img alt="Windows 10/11 x64" src="https://img.shields.io/badge/Windows-10%2F11%20x64-2b2b3a">
  <img alt="Vulkan 1.3" src="https://img.shields.io/badge/Vulkan-1.3-b08d45">
  <img alt="DLSS" src="https://img.shields.io/badge/NVIDIA-DLSS%20%2B%20DLSS--G-76b900">
  <img alt="Estado: em desenvolvimento" src="https://img.shields.io/badge/estado-em%20desenvolvimento-c0762b">
</p>

# Harpia

**Harpia** roda *The Legend of Zelda: Ocarina of Time* (versão Nintendo Switch 2) **nativamente no Windows, sem emulador**, a partir da **sua própria cópia do jogo**.

Não é um emulador: o código do jogo é traduzido para C **uma vez, no seu PC**, e compilado como um programa comum do Windows (recompilação estática). O software de sistema do console é reimplementado e os gráficos do Switch 2 são refeitos em Vulkan.

> [!IMPORTANT]
> ### Aviso legal
> - Este repositório **não contém o jogo**, nem partes dele, nem chaves, nem executável pronto. Tudo o que a compilação gera (`harpia.exe`, o código gerado, caches de shader, saves) é derivado do **seu** jogo e fica **no seu PC**. **Nunca compartilhe esses arquivos.**
> - Você precisa ter **uma cópia legítima do jogo**, extraída por você do seu próprio console (um arquivo `.nup`, ou as pastas `romfs` e `exefs`). Este projeto **não explica como obter o jogo**, não descriptografa nada e não pede nem aceita chaves.
> - Harpia é um projeto de fã, **sem vínculo com a Nintendo e sem endosso dela**. *The Legend of Zelda* e *Ocarina of Time* são marcas registradas da Nintendo. NVIDIA, GeForce, RTX, DLSS e Reflex são marcas da NVIDIA Corporation, que também não tem vínculo com este projeto. Os demais nomes pertencem aos seus donos.
> - Pedidos de ajuda que envolvam piratear o jogo serão fechados sem resposta.

## Índice

[O que é](#o-que-é-e-de-quem-é-o-trabalho) · [Recursos](#recursos) · [Estado atual](#estado-atual) · [Requisitos](#requisitos) · [Instalar](#instalar) · [DLSS](#dlss-como-obter-as-dlls-da-nvidia) · [Jogando e o menu](#jogando-e-o-menu) · [Atualizar e desinstalar](#atualizar-e-desinstalar) · [Problemas conhecidos](#problemas-conhecidos) · [Roteiro](#roteiro) · [Compilar pelo código](#compilar-pelo-código) · [Contribuir e pedir ajuda](#contribuir-pedir-ajuda-e-reportar-bugs) · [Créditos](#créditos-e-agradecimentos) · [Licença](#licença) · [Apoie o projeto](#apoie-o-projeto)

## O que é e de quem é o trabalho

A Harpia é uma **versão pessoal, com identidade própria**, do projeto de código aberto **[Hookshot](https://github.com/NotRexed/Hookshot)** (GPL-3.0-or-later), de **NotRexed e colaboradores**. **Quase todo o trabalho pesado vem dele**: o tradutor de ARM64 para C, o sistema Horizon reimplementado, a camada gráfica NVN2 e o recompilador de shaders. Sem esse projeto, a Harpia não existiria; veja os [créditos completos](CREDITS.md).

O que a Harpia acrescenta sobre a base está na lista de [recursos](#recursos). O código de cada mudança fica neste repositório, sob a mesma licença da base.

## Recursos

**Imagem**
- **NVIDIA DLSS Super Resolution** no lugar da ampliação do jogo: Auto, Qualidade, Equilibrado, Desempenho, Ultra desempenho e **AA nativo (DLAA)**. A escala de entrada é a que o próprio DLSS recomenda para o modo escolhido.
- **Resolução de saída 2.5K (2560x1440)** e textura **Ultra** (sem recompressão das texturas grandes).
- Ampliadores **AMD FSR 1** e **NVIDIA NIS**, Catmull-Rom e Lanczos para quem não usa DLSS; filtragem anisotrópica de 2x a 16x; proporção (esticar, ajustar 16:9, preencher); VSync.
- **Predefinições de qualidade** por faixa de placa (Mínima 4 GB, Baixa 6 GB, Equilibrada 8 GB, Alta 12 GB, Ultra 16 GB+).

**Quadros**
- **DLSS Frame Generation com Reflex**: 2x, 3x ou 4x os quadros exibidos (4x exige placa RTX 50). A lógica do jogo continua a mesma: só os quadros mostrados aumentam.
- Geração de quadros própria (2x a 8x), para placas sem DLSS-G.
- Ritmo de quadros regular e escolha do modo de velocidade do jogo (por quadro ou em tempo real).

**Menu no visual da Harpia** (<kbd>Insert</kbd> ou <kbd>F1</kbd>; <kbd>Q</kbd>/<kbd>E</kbd> ou LB/RB trocam de aba), em português e inglês, com abas Tela, Gráficos, Desempenho, Entrada, Controles e Info. Inclui escolha da placa de vídeo, buffers protegidos, velocidade do texto dos diálogos, câmera pelo mouse com sensibilidade e inversão, predefinição de layout do controle e associação de botões.

**Estabilidade**
- Correções do projeto original para perda de dispositivo da GPU, quadros pretos e folhagem escura.
- **Cache de shaders** que sobrevive a atualizações (notas de pipelines e lista de shaders preparada antes de jogar) e marcador de falha que não perde o cache.

**Instalação**
- **`HarpiaSetup.exe`**: instalador de um arquivo só, offline, sem administrador, com atalhos, entrada em "Aplicativos instalados" e desinstalador.
- **Harpia Builder**: janela que transforma o seu `.nup` em uma pasta jogável, com **Verificação do PC** antes (processador, memória, tipo e espaço do disco, placa com Vulkan 1.3 e mesh shaders, compilador e Windows SDK), veredito e tempo estimado, e velocidade **Automático** ou **Máximo**.

**Medido no PC do autor** (Intel Core Ultra 9 275HX, NVIDIA RTX 5080), como referência e não como garantia:

| Medida | Resultado |
|---|---|
| Compilação do jogo | 21 min no modo Automático, cerca de 16 min no Máximo |
| Jogo em 2.5K com DLSS | 60 quadros por segundo |
| Com DLSS Frame Generation 4x | 240 quadros por segundo exibidos |

## Estado atual

Harpia está **em desenvolvimento ativo**. O jogo roda e é jogável; ainda há arestas (veja [Problemas conhecidos](#problemas-conhecidos)).

| Recurso | Estado |
|---|---|
| Jogo nativo em Windows, Vulkan 1.3 | Funciona |
| Menu em abas, em pt-BR e inglês | Funciona |
| DLSS Super Resolution (todos os modos) | Funciona em placas RTX, com o DLSS ativado no Builder (veja [DLSS](#dlss-como-obter-as-dlls-da-nvidia)) |
| DLSS Frame Generation 2x a 4x com Reflex | Funciona em placas RTX com o DLSS ativado; 4x só em RTX 50 |
| Saída 2.5K e textura Ultra | Funciona na TV (não no modo portátil); vale ao reiniciar; muita VRAM |
| FSR 1, NIS, anisotropia, proporção, VSync, predefinições | Funciona |
| Cache de shaders que sobrevive a atualizações | Funciona |
| `HarpiaSetup.exe` e Harpia Builder com Verificação do PC | Funciona |
| Lista de pipelines distribuída pronta, opção de latência | **Em andamento** |

## Requisitos

- **Windows 10 ou 11, 64 bits.**
- **Placa de vídeo com Vulkan 1.3** (o jogo usa mesh shaders: placas com `VK_EXT_mesh_shader` são o alvo), com driver atualizado. Os testes são feitos principalmente em uma **NVIDIA GeForce RTX 5080**. **DLSS** pede placa **RTX**; há correções para AMD feitas a partir de relatos, e GPUs Intel não foram testadas.
- **16 GB de RAM ou mais** e cerca de **60 GB livres** durante a compilação (use SSD; HDD é muito lento).
- **Microsoft Visual Studio Build Tools** com a carga "Desenvolvimento para desktop com C++" e o Windows SDK que ela seleciona ([Ferramentas para Visual Studio](https://visualstudio.microsoft.com/downloads/)). A Microsoft não permite embutir o compilador e o SDK no instalador: o Builder avisa, em uma tela própria, se faltarem.
- **A sua cópia do jogo, na versão suportada:** programa `0400486024DEC000`, atualização `v131072`. Outras versões são recusadas com uma mensagem clara.

## Instalar

1. **Baixe o `HarpiaSetup.exe`** na aba [Releases](../../releases/latest) (versão 1.0.0). A página traz também o `SHA256SUMS.txt`, para conferir o arquivo baixado.
2. **Execute-o.** Ele mostra a licença e os avisos, pede uma pasta (padrão `%LOCALAPPDATA%\Harpia`), instala sem precisar de administrador, cria atalhos e abre o Builder. Nada é baixado: tudo vem dentro do arquivo.
3. No **Builder**, veja a **Verificação do PC**: ela lê processador, memória, disco, placa e ferramentas de compilação, diz se dá para compilar e quanto deve demorar. Se faltarem o Build Tools ou o Windows SDK, uma tela explica e leva ao download.
4. **Aponte para o seu jogo.** O Builder procura arquivos `.nup` em Downloads, Desktop e Documentos (até três pastas abaixo) e também aceita que você **arraste o `.nup`** para a janela, ou as pastas `romfs` e `exefs`. O `.nup` é um pacote já descriptografado que traz, para cada título, as árvores `exefs/` e `romfs/`; ele vem de um *dump feito por você, do seu próprio jogo*, e este projeto não ensina a fazê-lo.
5. Escolha a **velocidade** (Automático ou Máximo), a pasta de destino (por exemplo `C:\Games\Harpia`) e o **idioma do jogo** (16 idiomas, incluindo pt-BR), e inicie. A tradução e a compilação rodam no seu PC; o progresso aparece por etapas.
6. Abra o **`harpia.exe`** na pasta criada (ou pelo atalho).

Algo deu errado? Veja [SUPPORT.md](SUPPORT.md).

### Arquivos criados ao lado do jogo

| Arquivo | O que é |
|---|---|
| `harpia_options.ini` | opções do menu e dos controles |
| `harpia_log.txt` | registro da última execução (quando aberto pelo ícone) |
| `harpia_programs.bin`, `harpia_pipelines.bin`, `harpia_vk_pipelines.bin`, `harpia_prepared.bin` | caches de shader: podem ser apagados, são refeitos |
| `harpia_shaders.bin` | lista de shaders do jogo, preparada antes de jogar |
| `harpia_env.txt` | opcional: linhas `NOME=VALOR` lidas como variáveis de ambiente |
| `user\save` | seus saves |
| `language.txt` | código do idioma do jogo (por exemplo `pt-BR`) |
| `image.bin`, `romfs\`, `.harpia_exefs` | **dados do SEU jogo**, usados pelo programa: **nunca compartilhe** |

Os caches e o log **derivam do jogo ou do seu PC**: não os anexe a relatórios sem olhar antes (veja [como reportar](CONTRIBUTING.md#reportar-um-bug)).

## DLSS: como obter as DLLs da NVIDIA

O DLSS é **opcional** e **exclusivo de placas NVIDIA RTX**. As bibliotecas dele são propriedade da NVIDIA e **não estão incluídas** neste repositório, no `HarpiaSetup.exe` nem em nenhuma versão da Harpia: o Builder as baixa do pacote oficial da NVIDIA **só se você pedir**, e **você mesmo lê e aceita os termos da NVIDIA**.

**Ativar**
1. No Builder, clique em **Ativar NVIDIA DLSS (opcional)**. O botão fica na página "O seu jogo está pronto", depois de montar o jogo, e ao lado de "Jogar" na primeira página quando a pasta já tem um jogo montado por este Builder. Ele só aparece se a Verificação do PC achou uma placa **NVIDIA RTX**; senão, uma linha explica que o DLSS precisa de uma.
2. O Builder baixa o pacote oficial **Streamline SDK 2.14.16** ([release da NVIDIA no GitHub](https://github.com/NVIDIA-RTX/Streamline/releases/tag/v2.14.16)): cerca de 276 MB, com o SHA-256 fixo no Builder. Você pode cancelar; o arquivo fica em `%LOCALAPPDATA%\HarpiaBuilder\downloads`.
3. **Antes de copiar qualquer coisa**, ele mostra os termos da NVIDIA tirados do pacote (DLSS/RTX SDK, Reflex e a licença MIT do Streamline). **Marque "Li e aceito os termos da NVIDIA"**: só então o botão **Ativar DLSS** é liberado.
4. Ele copia para a pasta do jogo **apenas** `sl.interposer`, `sl.common`, `sl.dlss`, `nvngx_dlss`, `sl.dlss_g`, `nvngx_dlssg`, `sl.reflex`, `sl.pcl` e `NvLowLatencyVk` (`.dll`), mais `nvngx_dlss.license.txt` e `reflex.license.txt`, e confere a **assinatura digital** de cada DLL (válida e emitida para a NVIDIA Corporation). Em qualquer falha (hash, assinatura, sem internet) ele apaga o que copiou e avisa que o jogo funciona sem DLSS.
5. Abra o jogo: as linhas de DLSS aparecem no menu (abas Gráficos e Desempenho). Ligar o DLSS vale depois de **reiniciar** o jogo.

**Sem as DLLs** (ou em placa que não seja RTX), o jogo funciona normalmente e o menu **não mostra** o DLSS. A geração de quadros e as demais opções da Harpia continuam disponíveis.

**Remover:** clique em **Remover DLSS** no Builder; ele apaga exatamente esses 11 arquivos da pasta do jogo. O jogo volta a rodar sem DLSS, sem perder opções nem saves.

## Jogando e o menu

| Tecla | Ação |
|---|---|
| <kbd>F1</kbd> ou <kbd>Insert</kbd> | Menu de opções (no controle: Back + Start, ou Home; LB / RB trocam de aba) |
| <kbd>F11</kbd> | Tela cheia |
| <kbd>W</kbd> <kbd>A</kbd> <kbd>S</kbd> <kbd>D</kbd> e setas | Andar (analógico esquerdo) e girar a câmera: valores fixos. A aba Controles associa os botões do jogo |

Dentro do menu: <kbd>Q</kbd> / <kbd>E</kbd> trocam de aba, <kbd>↑</kbd> <kbd>↓</kbd> escolhem e <kbd>←</kbd> <kbd>→</kbd> mudam o valor. Algumas opções (Preencher, Resolução de saída, Qualidade de textura, Névoa, Modo do console, Placa de vídeo, Buffers protegidos e ligar o DLSS) só valem **ao reiniciar**; o menu avisa.

| Aba | Opções |
|---|---|
| **Tela** | Tela cheia · Proporção · VSync · Contador de FPS · Placa de vídeo |
| **Gráficos** | Predefinição de qualidade · Escala de renderização · Ampliação (Linear, Catmull-Rom, Lanczos, FSR 1, NIS) · **NVIDIA DLSS** (Desligado, Auto, AA nativo, Qualidade, Equilibrado, Desempenho, Ultra desempenho) · Nitidez · Filtragem de texturas · Resolução de saída · Qualidade de textura · Névoa · Modo do console |
| **Desempenho** | Geração de quadros · **Geração de quadros DLSS** (2x a 4x) · Ritmo de quadros · Velocidade do jogo · Velocidade do texto · Buffers protegidos |
| **Entrada** | Controle em segundo plano · Layout do controle · Câmera pelo mouse, sensibilidade e inversões |
| **Controles** | Associar botões do controle e teclas |
| **Info** | Placa de vídeo, saída, teclas e créditos |

## Atualizar e desinstalar

- **Atualizar:** instale a nova versão do `HarpiaSetup.exe` e, no Builder, escolha a pasta do jogo que já existe: ele avisa "Esta pasta já tem um jogo Harpia" e o botão vira **Atualizar o jogo nesta pasta**. **Só o programa do jogo** (`harpia.exe`, `harpia_vk.dll`, `SDL2.dll`, `SDL3.dll`, `wgpu_native.dll`) **e o `image.bin` são trocados**, cada um copiado ao lado e trocado só no fim. Suas **configurações** (`harpia_options.ini`), **saves** (`user\save`), o idioma (`language.txt`), os **caches de shader** e as **DLLs da NVIDIA** ficam; a lista de shaders (`harpia_shaders.bin`) e `harpia_version.txt` são reescritos. Por segurança, copie `user\save` antes de qualquer atualização. A verificação automática de versão do Builder fica **desligada** por padrão.
- **Desinstalar:** pelo Windows em *Aplicativos instalados*, pelo atalho "Desinstalar o Harpia" ou com `HarpiaSetup.exe --uninstall`. Ele remove a instalação, os atalhos e a entrada do Windows e, se você quiser, os arquivos de compilação do Builder. **As pastas de jogo que você construiu nunca são tocadas**; apague-as à mão se quiser liberar o espaço.

## Problemas conhecidos

- **Chama de tochas e varas** (Deku Stick) sem animação: tratada nesta versão; se ainda aparecer, reporte. **Iluminação um pouco clara demais** em algumas cenas: ajuste em andamento.
- **Piscadas:** de vez em quando um quadro sai sem iluminação (silhuetas). Existe um detector (`RX_VK_DARKLOG`) e várias causas já foram descartadas; a causa raiz ainda é investigada.
- **Travadas na primeira vez:** os shaders de uma área nova são compilados durante o jogo. A partir da segunda execução, o cache evita quase tudo.
- **Perda de dispositivo da GPU** (*device lost*), rara, observada em testes de desenvolvimento; as causas prováveis já foram corrigidas, e o caso segue em observação.
- **Geração de quadros** (a própria, não a do DLSS) pode cintilar em movimento rápido, no HUD e em cortes de câmera.
- A saída 2.5K usa muita VRAM (cerca de 13 GB em testes) e foi testada quase só com textura Ultra; com textura Alta houve mais quadros escuros.
- O build Linux existe no código mas **não é suportado**.

Achou outro? Veja [como reportar um bug](CONTRIBUTING.md#reportar-um-bug).

## Roteiro

**Concluído** (uma etapa por commit no histórico): atualização do projeto original (folhagem, iluminação, filmes, correção de perda de dispositivo) · cache de shaders robusto · correções do tradutor e detector de quadros escuros · desempenho de CPU · saída 2.5K nativa e textura Ultra · DLSS Super Resolution e DLSS Frame Generation com Reflex · menu no visual da Harpia · Harpia Builder com Verificação do PC, `HarpiaSetup.exe` e a etapa opcional "Ativar NVIDIA DLSS".

**Em andamento:** lista de pipelines distribuída pronta (para menos travadas na primeira vez) · limite de quadros em voo (latência) · a iluminação um pouco clara · a causa das piscadas.

**Planejado:** ajuste de cor e painel de desempenho · documentação técnica em português.

## Compilar pelo código

Quem quer compilar pelo código-fonte (em vez do instalador) precisa de Python 3 com `lz4`, `numpy`, `capstone` e `zstandard` (e `unicorn` 2.1 só para os testes de instruções), do clang do emsdk e do Build Tools acima; detalhes em [docs/development.md](docs/development.md). O repositório não guarda binários: as ferramentas pré-compiladas e os cabeçalhos baixados na compilação vêm no pacote do Builder.

```powershell
# conferir só a versão do jogo (precisa das ferramentas de compilação já instaladas)
python -I nx2/make_game.py --check --nup "C:\caminho\seu-jogo.nup"

# construir a pasta do jogo
python -I nx2/make_game.py --nup "C:\caminho\seu-jogo.nup" --out "C:\Games\Harpia" --language pt-BR
python -I nx2/make_game.py --romfs "C:\caminho\romfs" --exefs "C:\caminho\exefs" --out "C:\Games\Harpia" --language pt-BR
```

A pasta de trabalho é `%LOCALAPPDATA%\HarpiaBuilder`. `--update <pasta do jogo>` refaz a pasta (só em pastas feitas por uma versão recente, que guardam uma cópia dos arquivos do jogo). O código de `--language` deve ser um dos 16 da lista do Builder, como `pt-BR` ou `en-US`.

### Como funciona

1. `nx2/lift.py` traduz o código ARM64 do jogo para C.
2. `nx2/build.py` compila esse C junto com `host/`, que faz o papel do sistema do console.
3. `host/nvn_native.c` grava as chamadas gráficas NVN2 do jogo e `gpu/vk/` as reproduz em Vulkan.
4. Os shaders Ampere do Switch 2 são rebaixados para Maxwell e traduzidos para SPIR-V pelo recompilador de shaders do Eden.

```
nx2/         tradutor, compilação e ferramentas (Python)
host/        sistema do console reimplementado (C)
gpu/vk/      renderizador Vulkan, DLSS e recompilador de shaders
builder/     Harpia Builder (WPF, .NET 8) e instalador
third_party/ componentes de terceiros, com as suas licenças
docs/        projeto técnico e imagens
tests/       testes que não precisam do jogo
```

Mais em [docs/development.md](docs/development.md) e [docs/design.md](docs/design.md) (em inglês).

## Contribuir, pedir ajuda e reportar bugs

- **Contribuir:** [CONTRIBUTING.md](CONTRIBUTING.md) (como compilar, ramos, commits, regras de teste em GPU e PRs).
- **Preciso de ajuda:** [SUPPORT.md](SUPPORT.md).
- **Achei um bug:** [abra uma issue](../../issues/new/choose) com o modelo de bug (placa, driver, Windows, `harpia_options.ini` e trecho do `harpia_log.txt`).
- **Segurança:** [SECURITY.md](SECURITY.md).
- **Convivência:** [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

## Créditos e agradecimentos

Obrigado, de coração, a quem fez o trabalho mais difícil:

- **NotRexed** e os colaboradores do **[Hookshot](https://github.com/NotRexed/Hookshot)**, base de tudo aqui (recompilação, sistema, NVN2, a64-kit).
- **Eden**, **yuzu/suyu** e **Eglantine**: o recompilador de shaders e o trabalho com shaders do Switch 2.
- **sostenesfreitas/ocarina-recomp**, **Lichii**, **nxnative**, **MacNativeGames**, **TotK Recomp** e **dkb-pc-recomp**: pesquisa aberta e correções que guiam as nossas.
- **NVIDIA Streamline** (cabeçalhos sob licença MIT, no pacote do Builder) para o DLSS; **AMD FidelityFX FSR** (MIT); **NVIDIA Image Scaling** (MIT); **Inter**, a fonte do Builder (SIL Open Font License); **Ryujinx**, **zbic**, **SDL2**, **Vulkan Memory Allocator**, **Sirit**, **Opus** e muitos outros.

A lista completa, com links e licenças, está em **[CREDITS.md](CREDITS.md)**, em [NOTICE.md](NOTICE.md) e em [LICENSES/](LICENSES/README.md). Se o seu trabalho está aqui e o crédito está errado ou faltando, abra uma issue: corrigimos na hora.

## Licença

Harpia é software livre sob a **[GNU General Public License, versão 3 ou posterior](LICENSE)** (GPL-3.0-or-later), como o projeto em que se baseia. Partes de terceiros sob outras licenças compatíveis mantêm os seus avisos nas suas pastas; veja [NOTICE.md](NOTICE.md). Os comentários `(hookshot)` e `Hookshot issue/PR N` nos fontes são avisos de autoria e modificação exigidos pelas licenças: **não os remova**.

## Apoie o projeto

A Harpia é e continuará **gratuita**. Se quiser ajudar a manter o desenvolvimento, pode fazer uma doação voluntária por **PIX**:

<p align="center">
  <img src="docs/assets/pix-qrcode.png" alt="QR Code PIX" width="260">
</p>

> Doações são voluntárias, **não compram acesso, o jogo nem suporte prioritário**, e não têm relação com a Nintendo nem com a NVIDIA. Considere também apoiar os projetos que tornam a Harpia possível (links em [CREDITS.md](CREDITS.md)).
