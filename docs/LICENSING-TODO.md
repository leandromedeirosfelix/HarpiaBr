# Pendências de licenciamento (interno)

Documento **interno**. O repositório pode ficar **privado** com estas pendências; **elas bloqueiam torná-lo público**. Nada abaixo foi resolvido de propósito: cada item precisa de decisão do autor e, se possível, de confirmação com o autor original.

## 1. Sirit: cabeçalhos e textos de licença não batem

- Os 22 arquivos de `gpu/vk/sirit/` dizem "Copyright (c) 2019 sirit ... 3-Clause BSD License".
- A pasta traz `LICENSE` (texto AGPL-3.0) e `LICENSE.txt` (texto GPL-3.0, idêntico ao da raiz) e **nenhum** texto BSD-3.
- **Fazer:** conferir a licença no commit original do Sirit (ReinUsesLisp/sirit) e no fork do Eden; trocar os arquivos por um texto que corresponda aos cabeçalhos, ou documentar por que a pasta é GPL/AGPL. Atualizar `NOTICE.md` e `CREDITS`.

## 2. zbic / Atmosphere-NX: possível GPL-2.0-only

- `third_party/zbic/PROVENANCE.md` diz que as partes ZBIC vêm do Atmosphere-NX (GPLv2) e o `LICENSE` é GPLv2. Se for **somente** 2.0, não combina com GPL-3.0-or-later.
- Mitigação atual: é uma ferramenta separada (`zbic.dll`, usada por `nx2/unpack.py` via ctypes), não ligada ao jogo.
- **Fazer:** confirmar "2.0-only" ou "or-later" na origem; se for só 2.0, dizer em `NOTICE.md` que `third_party/zbic` é um programa separado sob GPL-2.0, ou substituir por outra rota de descompressão.

## 3. Cabeçalhos herdados e arquivos sem cabeçalho

- Arquivos do NotRexed (a64-kit, `host/*.c`, `nx2/*.py` herdados) **não têm** cabeçalho de copyright/licença; o status GPL é herdado do `LICENSE` do Hookshot e do suyu. **Fazer:** confirmar o `LICENSE` do Hookshot, citá-lo em `NOTICE.md` e, se o autor concordar, acrescentar cabeçalhos SPDX.
- `gpu/third_party/suyu/**` traz cabeçalhos de Citra, Dolphin, PPSSPP, Gekko, FasTC/UNC, Boost, degasus (WTFPL), Austin Appleby: mantidos, listados em `CREDITS`. **Fazer:** conferir que nenhum cabeçalho foi cortado.
- `third_party/a64-kit/NOTICE.TOTKPort.md` e `README.a64-kit.md` são cópias do TOTKPort, com caminhos que não existem aqui e dois links quebrados. **Fazer:** marcar como aviso original reproduzido, ou reescrever para esta árvore.
- `gpu/vk/host_shaders/*_comp_spv.h`: só os arrays SPIR-V; faltam os `.comp` (GPL-3.0-or-later, Eden). **Fazer:** acrescentar as fontes GLSL ou a referência exata ao commit do Eden.

## 4. Antes de distribuir binários (pacote do Builder, AppImage)

- Incluir os textos de licença do SDL2, Opus, FFmpeg e dav1d junto de qualquer binário embutido; conferir se o FFmpeg usado foi compilado sem `--enable-nonfree`.
- DLLs do DLSS/Streamline da NVIDIA: **nunca** embutir.

## 5. Outros itens de higiene antes de ficar público

- Reavaliar o e-mail do autor nos commits (usar o endereço noreply do GitHub, se preferir).
- Decidir se os comentários com endereços de funções do jogo em `host/` e `nx2/` ficam (decisão atual: ficam).
- Revisar se os nomes "Hookshot / oot-recomp / ocarina-recomp" nos créditos estão no formato desejado.
