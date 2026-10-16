# ARA CPU Fix 1.0

Evita que o PC inteiro congele durante a passagem de turno de ARA: History Untold. Parte do **Project ARA: Reborn**; funciona sozinho.

## O problema
O jogo cria uma thread de trabalho por processador lógico (menos uma), prende cada uma num processador, deixa elas girando quando não há trabalho, com prioridade alta, e sobe o processo para "acima do normal" quando a janela tem foco. Na passagem de turno todos os processadores ficam ocupados acima do Windows e o sistema pode travar por completo.

## O que o mod faz (só na memória; nenhum arquivo do jogo muda)
1. **Reserva processadores lógicos para o Windows.** O jogo passa a ver e a usar só os outros: afinidade do processo, as chamadas de afinidade do jogo, a contagem de processadores (o jogo cria menos threads) e o boost dinâmico de prioridade.
   `bin\ARA_CpuFix.ini`, `[CPU] ProcessadoresLivres`:
   - `auto` (padrão): 16 ou mais processadores lógicos → reserva 4; 12 ou mais → 2; 6 ou mais → 1; menos → nenhum;
   - um número (ex. `2`) para escolher; `0` desliga.
2. **Prioridade normal com foco** em vez de "acima do normal" (`[Prioridade] LimitarEmNormal=1`; `0` = comportamento original).

Vale ao abrir o jogo. Log em `bin\ARA_CpuFix.log` (mostra quantos processadores ficaram para o jogo).

## Instalar
Feche o jogo e rode `INSTALAR.bat`. Substitui `bin\avutil-58.dll` (a original fica como `avutil-58_orig.dll`). Remover: `DESINSTALAR.bat`.

Se você usava o AraQoL 1.9c, atualize para o AraQoL 2.0 (sem a parte de CPU) para a correção não rodar duas vezes.

## English
Stops the whole-PC freeze at end of turn: reserves logical processors for Windows (game affinity, thread pinning calls, processor count and priority boost are filtered through the mod) and keeps the game at normal priority when focused. Setting: `bin\ARA_CpuFix.ini` → `[CPU] ProcessadoresLivres=auto|N|0`. Replaces `avutil-58.dll`.
