# Ajuda

[English](SUPPORT.en.md) · **Português**

Antes de abrir uma issue, veja se a resposta está aqui.

> **Não damos suporte** para obter o jogo, descriptografá-lo, chaves ou cópias piratas. Veja o [aviso legal](README.md).

## Problemas na instalação (Harpia Builder / `make_game.py`)

| Mensagem ou sintoma | O que fazer |
|---|---|
| *"This is build XXXXXXXX. Harpia supports build e0f3ccfa."* | O seu jogo não é a versão suportada (programa `0400486024DEC000`, atualização `v131072`). Use um dump dessa versão. |
| *"The file ... in this dump is not a game module"* / *"is not a game executable"* | O dump está incompleto, corrompido ou ainda criptografado. Refaça o dump a partir do seu console. |
| *"The package did not contain a romfs folder."* | O `.nup` não traz o `romfs`. Use outro `.nup` ou as pastas `romfs` e `exefs`. |
| *"Visual Studio's C++ tools were not found"* ou erro de clang/EMSDK não encontrado | No Visual Studio Installer escolha **Modificar** e marque **Desenvolvimento para desktop com C++**. |
| *"The Windows SDK was not found"* | Na mesma tela, marque um **Windows SDK** (10 ou 11). |
| *"Your Visual Studio is too old for this game's renderer"* | Abra o Visual Studio Installer e atualize. |
| *"The builder's own Python ... is missing"* | O antivírus provavelmente removeu o Python do pacote. Restaure da quarentena ou descompacte em uma pasta que o antivírus permita. |
| A compilação ficou parada quase sem usar CPU | O Windows pode ter reduzido a prioridade da janela em segundo plano. Deixe a janela do Builder visível. |
| Falta de espaço | A compilação usa cerca de 60 GB temporários. Libere espaço na unidade da pasta de trabalho (`%LOCALAPPDATA%\HarpiaBuilder`). |

## Problemas ao jogar

- **Tela preta ou fecha ao abrir:** atualize o driver da placa de vídeo (é preciso Vulkan 1.3 com mesh shaders). Abra o jogo pelo ícone e olhe o `harpia_log.txt`.
- **Travadas na primeira vez em cada área:** é a compilação de shaders; da segunda execução em diante melhora. Não apague os arquivos `harpia_*.bin` à toa.
- **Quadros escuros ocasionais:** é um [problema conhecido](README.md#problemas-conhecidos), em investigação.
- **Mudei uma opção e nada mudou:** algumas só valem ao reiniciar (o menu avisa).
- **Quero voltar tudo ao padrão:** feche o jogo e apague o `harpia_options.ini`.
- **Perdi os saves:** ficam em `user\save` ao lado do jogo. Faça cópia dessa pasta antes de reconstruir o jogo.

## Ainda preciso de ajuda

1. Procure nas [issues](../../issues?q=is%3Aissue) (inclusive as fechadas).
2. Se não achar, abra uma com o modelo **Bug**, preenchendo placa de vídeo, driver, Windows, `harpia_options.ini` e um trecho do `harpia_log.txt` (veja [como reportar](CONTRIBUTING.md#reportar-um-bug)).
3. **Nunca** poste arquivos do jogo, caches `*.bin` ou saves.

Respondemos quando possível: o projeto é mantido por voluntários.
