# AraQoL 2.0

Correções de câmera e interface para ARA: History Untold 2.1.0.542. Parte do **Project ARA: Reborn**; funciona sozinho.

## O que faz
- **Câmera:** o teto de altura deixa de depender da área explorada (multiplicador da região 2,0 → 100). A Visão Estratégica fica alcançável desde o começo. `[Camera] ZoomMaximo=0` volta ao original.
- **Bandeiras do mapa** (recursos, rendimentos, melhorias, forças): opção desligada = escondidas em qualquer zoom; ligada = sempre visíveis.
- **Clique nas unidades:** folga extra no clique dos bonecos 3D (`[Clique] RaioExtraUnidades`, padrão 40, relido a cada 2 s).
- **Aprimorar Tudo:** botão no painel de aprimoramento do exército (escolhe o melhor aprimoramento de cada unidade; você confirma o custo) e botão no painel Militar (manda aprimorar todos os exércitos que podem). Texto no idioma do jogo.

A correção de CPU/prioridade que existia no AraQoL 1.9 virou o mod separado **ARA CPU Fix**.

## Instalar
Feche o jogo e rode `INSTALAR.bat`. Substitui `bin\swresample-4.dll` (a original fica como `swresample-4_orig.dll`). Log: `bin\AraQoL.log`; opções: `bin\AraQoL.ini`.
Remover: `DESINSTALAR.bat`.

## English
Camera ceiling no longer limited by explored area, map banner visibility follows the toggle, larger click radius on units, and "Upgrade All" buttons (army upgrade panel and Military panel). Replaces `swresample-4.dll`; settings in `bin\AraQoL.ini`.
