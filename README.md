# Project ARA: Reborn

Modpack para **ARA: History Untold** (Oxide Games / Microsoft), build **2.1.0.542** (Steam).
Cada mod é **independente**: baixe só os que quiser, em qualquer combinação e ordem.

*Modpack for ARA: History Untold, build 2.1.0.542 (Steam). Every mod is independent: download only the ones you want, in any combination and order. English summary at the end.*

## Mods

| Mod | Versão | O que faz | DLL do jogo que usa |
|---|---|---|---|
| [AraQoL](mods/AraQoL/README.md) | 2.0 | Câmera mais alta, bandeiras do mapa, clique mais fácil nas unidades, botões "Aprimorar Tudo" | `swresample-4.dll` |
| [ARA CPU Fix](mods/CpuFix/README.md) | 1.0 | Evita o congelamento do PC na passagem de turno (reserva núcleos para o Windows) | `avutil-58.dll` |
| [Ferrovias (Railroads)](mods/Ferrovias/README.md) | 1.9 | Ferrovias construídas pelo jogador e a nova melhoria Train Station | `avformat-60.dll` |
| [Brasil — Getúlio Vargas](mods/Brasil/README.md) | 1.0 | Nova nação Brasil com o líder Getúlio Vargas | `swscale-7.dll` + pacotes de personagens* |
| [URSS — Josef Stalin](mods/URSS/README.md) | 1.0 | Nova nação União Soviética com o líder Josef Stalin (2 skins) | `swscale-7.dll` + pacotes de personagens* |

Cada mod substitui uma DLL **diferente** do jogo (a original é guardada como `<nome>_orig.dll` e continua sendo usada), então os mods não brigam entre si. Os ganchos em funções do jogo são encadeáveis: dois mods podem enganchar a mesma função.

\* Brasil e URSS dividem a mesma [base dos líderes](mods/Lideres/README.md), que vai dentro dos dois zips: instale um, outro ou os dois. A base aplica as peças 3D por patch a partir dos arquivos originais da Steam (conferidos pelo SHA-1) e guarda os trechos originais para o desinstalador devolver.

**Em preparação:** Vozes dos Líderes.

## Como instalar

1. Baixe o zip do mod na página **Releases** do repositório.
2. Feche o jogo e extraia o zip em qualquer pasta.
3. Clique duas vezes em `INSTALAR.bat`. O instalador acha o jogo pela Steam; se não achar, rode
   `powershell -ExecutionPolicy Bypass -File install.ps1 -GamePath "C:\caminho\Ara History Untold"`.
4. Para remover: `DESINSTALAR.bat` (devolve a DLL original do jogo).

Observações:
- **Verificar integridade dos arquivos** na Steam (ou uma atualização do jogo) devolve as DLLs originais e desliga os mods. Basta rodar o `INSTALAR.bat` de novo.
- As DLLs conferem a versão do executável; em outra build do jogo as partes que dependem dela ficam desligadas e o log avisa.
- Logs e arquivos `.ini` de cada mod ficam na pasta `bin` do jogo.
- Os mods não mexem em saves. Mods com dados de jogo (Ferrovias) precisam ser marcados ao criar um **jogo novo**: o jogo grava as regras dentro do save.

## Para quem quer compilar

Fontes em `mods/<mod>/src` e o gancho compartilhado em `common/ara_hook.h`. Compilador usado: MinGW-w64 (GCC 16, WinLibs).
`python tools/make_release.py` compila tudo e monta os zips em `dist/`. O `.def` de cada proxy sai de `tools/gen_def.py`.

## Aviso

Projeto de fãs, sem ligação com a Oxide Games ou a Microsoft. Use por sua conta; faça backup dos seus saves.
Licença: a definir pelo autor.

---

## English summary

| Mod | What it does | Game DLL it replaces |
|---|---|---|
| AraQoL 2.0 | Higher camera ceiling, map banner visibility, easier unit clicking, "Upgrade All" buttons | `swresample-4.dll` |
| ARA CPU Fix 1.0 | Stops the whole-PC freeze during end-of-turn processing by reserving logical processors for Windows | `avutil-58.dll` |
| Railroads 1.9 | Player-built railroads and a new Train Station improvement | `avformat-60.dll` |
| Brazil — Getúlio Vargas 1.0 | New nation and leader | `swscale-7.dll` + character packages (shared leader base) |
| Soviet Union — Joseph Stalin 1.0 | New nation and leader, two 3D skins | `swscale-7.dll` + character packages (shared leader base) |

Install: download the zip from **Releases**, close the game, run `INSTALAR.bat` (or `install.ps1 -GamePath "..."`). Uninstall with `DESINSTALAR.bat`.
A Steam file verification restores the original DLLs; just run the installer again. Each mod works alone or together with the others.
