# Ferrovias (Railroads) 1.9

Ferrovias construídas pelo jogador em ARA: History Untold 2.1.0.542. Parte do **Project ARA: Reborn**; funciona sozinho.

## O que faz
- **Botão FERROVIA** na tela de gerenciamento de estradas, liberado pela tecnologia **Steam Engine**. Você escolhe início e fim e o jogo traça região por região, como uma estrada.
- **Custo por região:** Madeira 3, Pedra 3, Ferro 2, Aço 2; 30 de produção (turnos de obra). Onde já existe estrada o trilho toma o lugar dela; estrada da IA ou do jogador nunca apaga trilho.
- **Visual de trilho:** a DLL desenha as ligações com ferrovia com o material de trilhos que já existe no jogo (sem uso no jogo original). As ruas internas das regiões no caminho da linha também viram trilho.
- **Train Station:** nova melhoria (Steam Engine) que serve de ponto de conexão de estradas/ferrovias em regiões sem cidade.
- **Train Station 3D (opcional):** modelo próprio (prédio de galpões com trilho pintado na fundação). Instalado com `INSTALAR.bat`, que chama `estacao\estacao.py`. Precisa de **Python 3** (o instalador baixa o `numpy` com o pip se faltar). Os pacotes originais do jogo ficam guardados em `Mods\Ferrovias\estacao_backup` e o desinstalador os devolve.
- **Estação alinhada à linha (experimental):** a DLL gira e desloca só o visual da estação para a faixa dos portões ficar sobre um trecho reto da linha.

## Importante
- Marque o mod **Ferrovias** ao criar um **jogo novo**: o jogo grava as regras (custos, melhoria, tecnologia) dentro do save.
- O mod substitui `ConnectionPoints` e `Technologies` (Steam Engine). Não use junto com outro mod que substitua essas listas (ex.: Workshop 5293348).
- Ainda não feito: ponte própria de ferrovia (rios mostram a ponte de estrada), cor/ícone da ferrovia no mapa de estradas, trens andando. O alinhamento da estação ainda não foi testado em jogo.

## Instalar
Feche o jogo e rode `INSTALAR.bat`. Ele:
1. substitui `bin\avformat-60.dll` (a original fica como `avformat-60_orig.dll`);
2. copia `Mods\Ferrovias` para `Documentos\My Games\Ara History Untold\Mods`;
3. instala a Train Station 3D se houver Python (opcional; sem Python a estação usa o modelo da fábrica).

Log: `bin\Ferrovias.log`; opções: `bin\Ferrovias.ini` (`[Estacao] DesvioDaLinha=0` desliga o alinhamento; `[Desenho] TrilhoDentroDaRegiao`). Remover: `DESINSTALAR.bat`.

## English
Player-built railroads (Steam Engine tech, RAILROAD button in the road screen; per-region cost Wood 3, Stone 3, Iron 2, Steel 2; 30 production), drawn with the game's unused railroad track material, plus a Train Station improvement that acts as a connection point. Optional 3D station model (needs Python 3). Enable the mod when starting a **new game**. Replaces `avformat-60.dll`.
