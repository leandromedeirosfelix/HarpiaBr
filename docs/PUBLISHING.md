# Publicação no GitHub (interno)

O repositório do GitHub (`HarpiaBr`, privado) tem **um histórico próprio, de um commit raiz** (um *snapshot* da árvore), publicado a partir do ramo local `publish`. O histórico fino do `main` local (um commit por fase) **não** é publicado: ele continha metadados do dump do jogo em `docs/design.md` e caminhos locais nas mensagens de commit.

## Como as duas linhas se relacionam

- `main` local: fonte de trabalho das sessões de desenvolvimento. Nunca é reescrito.
- `publish` local: o snapshot e o que vier depois dele. Sua primeira árvore é a do `main` no momento do snapshot (commit indicado na mensagem do primeiro commit).
- Remoto: `origin/main` = `publish`.

## Levar uma fase nova para o GitHub

1. No `main` local, a fase vira um commit (como hoje).
2. Gerar o patch: `git format-patch -1 <sha> --stdout > fase.patch`.
3. Revisar o patch (sem dados do jogo, sem caminhos locais nas mensagens) e aplicar no ramo `publish`: `git switch publish && git am fase.patch` (ou `git apply -3`).
4. Rodar o conferido de sempre (`git ls-files` sem `*.bin`, sem dumps; testes que não precisam do jogo) e então `git push origin publish:main`. **Nunca** `--force` nem `--mirror`.

Mensagens de commit do patch: sem caminhos absolutos (`C:\...`).
