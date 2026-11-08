# Como contribuir

[English](CONTRIBUTING.en.md) · **Português**

Obrigado por querer ajudar! Este guia cobre como compilar, como trabalhar com ramos e commits, as regras de teste em GPU e como enviar um PR. Ao participar, você concorda com o [Código de Conduta](CODE_OF_CONDUCT.md).

## Regras que valem sempre

1. **Nenhum dado do jogo, em nenhum lugar.** Nada de `romfs`, `exefs`, `image.bin`, módulos `.nso`, caches (`*_shaders.bin`, `*_programs.bin`, `*_pipelines.bin`, `*_prepared.bin`), dumps, saves, chaves, capturas que mostrem o jogo inteiro nem logs com caminhos pessoais. O `.gitignore` bloqueia a maior parte; confira o `git status` antes de cada commit.
2. **Sem pirataria.** Não peça nem ofereça arquivos do jogo, chaves ou instruções para obtê-los.
3. **Mantenha os avisos de licença e autoria.** Comentários `(hookshot)`, `Hookshot issue/PR N`, cabeçalhos `SPDX-License-Identifier` e arquivos `LICENSE`/`PROVENANCE` não se removem. Mudanças nossas em código herdado levam o marcador `(local)`.
4. **Só entra código compatível com a GPL-3.0-or-later.** Ao trazer algo de outro projeto, acrescente uma linha em `gpu/PROVENANCE.md` ou `host/PROVENANCE.md` com o projeto, o commit de origem e a licença, e dê o crédito em [CREDITS.md](CREDITS.md).
5. **Não distribua binários proprietários** (por exemplo as DLLs do DLSS da NVIDIA).

## Como compilar

Requisitos e comandos estão no [README](README.md#requisitos) e em [docs/development.md](docs/development.md). Resumo:

```powershell
python -I nx2/make_game.py --nup "C:\caminho\seu-jogo.nup" --out "C:\Games\Harpia" --language pt-BR
```

Para desenvolver, use uma pasta de trabalho separada (`OOT_WORK`) e **nunca** a pasta do jogo que você usa para jogar. Testes que não precisam do jogo:

```powershell
python -I -B nx2/test_nup_paths.py
python -I -B tests/test_make_game_cache.py
python -I -B tests/test_translator_stamp.py
```

(são exemplos; a lista que roda no CI está em `.github/workflows/source-regressions.yml`).

Observação sobre o cache de shaders: mudanças no código do tradutor (`gpu/vk/eden`, `gpu/vk/sirit`, `gpu/vk/compat`, `vk_shader.cpp`, `vk_state.h`, `vk_api.h`) mudam o *stamp* e recompilam os caches de todo mundo. Comentários e renomeações não mudam. Evite mexer ali sem necessidade.

## Ramos e commits

- `main` é o ramo estável: só entra o que compila e foi testado.
- Trabalhe em um ramo próprio com nome curto: `feat/<assunto>`, `fix/<assunto>`, `docs/<assunto>`.
- Commits no formato **`tipo: descrição`**, em português ou inglês, no imperativo ou no passado, mas **um assunto por commit**:

  `feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `perf`, `ci`

  Exemplo: `fix: libera páginas do guest sob o lock de importação`.
- Explique o **porquê** no corpo do commit quando não for óbvio.

## Regras de teste em GPU

O jogo usa a GPU de verdade, e a máquina de desenvolvimento é a mesma em que se joga. Por isso:

1. **Um teste de GPU por vez.** **Nunca dois jogos na GPU ao mesmo tempo**: em testes, a concorrência já causou *device lost*. O `scripts/run-vk.ps1` se recusa a rodar se outro `harpia.exe` estiver aberto (`-Force` ignora as travas; leia o topo do script, ele tem travas pessoais do autor).
2. **Só o jogo real.** Nunca despache shaders sintéticos na GPU (já causou perda de dispositivo).
3. **Antes de uma execução que mude o stamp**, faça backup de `harpia_programs.bin`, `harpia_pipelines.bin`, `harpia_vk_pipelines.bin` e `harpia_prepared.bin` (ou use uma cópia da pasta de teste), senão a base "quente" vira "fria".
4. Qualquer lista em `RX_VK_SKIP_PROG` deve conter `53509044401d78d2`.
5. Execuções sem janela (`HOS_HEADLESS`) não exercitam geração de quadros, NIS, as barras do modo "Ajustar" nem a apresentação: isso só se testa com janela.
6. Apague as capturas de tela (`work\runs\<nome>\shots`) depois de olhar: chegam a gigabytes. Verifique se o Windows registrou eventos `nvlddmkm` (falhas de driver) ao fim da execução.
7. Mudanças só de código (sem GPU) podem ser revisadas sem teste em GPU, mas diga isso no PR.

## Como enviar um Pull Request

1. Faça um *fork*, crie o ramo e mantenha o PR **pequeno e de um assunto só**.
2. Preencha o modelo do PR: o que muda, por quê, como foi testado (GPU, driver, Windows) e o que **não** foi testado.
3. Confirme o checklist de licenças (acima) e que nenhum dado do jogo entrou.
4. Esteja pronto para ajustes: a revisão procura erros de lógica, de licença/crédito e riscos para a GPU.

## Reportar um bug

Abra uma [issue](../../issues/new/choose) com o modelo **Bug**. Ele pede:

- placa de vídeo, **versão do driver**, versão do Windows e da Harpia;
- o conteúdo de `harpia_options.ini`;
- um **trecho** do `harpia_log.txt` (as últimas ~100 linhas e as linhas com `error`/`device lost`), **não o arquivo inteiro**;
- os passos para reproduzir e, se ajudar, uma captura de tela ou vídeo.

Antes de anexar, **leia o log**: ele pode ter nomes de pastas do seu PC. **Nunca anexe** caches (`*.bin`), o `image.bin`, saves ou arquivos do jogo.

## Pedir ajuda ou sugerir algo

Dúvidas de uso: [SUPPORT.md](SUPPORT.md). Ideias: o modelo **Sugestão** das issues. Vulnerabilidades: [SECURITY.md](SECURITY.md).
