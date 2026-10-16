# Base dos líderes (Brasil e URSS)

Não é um mod para baixar sozinho: vai dentro dos zips **Brasil** e **URSS**. Os dois líderes usam a mesma base, que é instalada uma vez só (instalar o segundo líder reaproveita a base).

## O que a base faz
- **DLL `bin\swscale-7.dll`** (AraLeaderBuilderUnlock v1.19; a original fica como `swscale-7_orig.dll`):
  - libera os líderes e nações dos mods no criador de líderes do jogo e monta o visual 3D (rosto, roupa, cabelo, bigode, quepe da skin 1945);
  - copia os textos do idioma do jogo (português do Brasil ou inglês) para o mod;
  - toca a narração de apresentação dos líderes dos mods.
- **Peças 3D e emblemas** gravados nos pacotes do jogo:
  - `Characters.OxAssetInfoPackage`, `Characters.OxModelPackage` (pastas `PackagedData` e `PackagedDataHD`);
  - `Characters.OxTexturePackage`;
  - `UI.OxTexturePackage` (emblemas das bandeiras);
  - `UI\Art\Icons\RGBEmblems.xml`;
  - mais as bandeiras novas em `GameCore\Flags`.

## Como funciona a instalação
- Os pacotes são montados por **patch** a partir dos arquivos **originais da Steam**: o instalador confere o SHA-1 do arquivo antes, monta o novo num temporário, confere o SHA-1 do resultado e só então troca.
- O zip não traz arquivos do jogo, só as diferenças: as peças e texturas dos mods, mais as tabelas de índice.
- Os trechos originais que mudam ficam guardados em `Documentos\My Games\Ara History Untold\Mods\_RebornLideres`. Quando o último líder é desinstalado, tudo volta ao original.
- Precisa de ~4,5 GB livres no disco do jogo durante a instalação (o pacote de texturas dos personagens tem 3,4 GB).
- Se a Steam atualizar o jogo, os patches não batem mais e o instalador avisa sem mudar nada.

Fontes: `src/ara_unlock.c`. Patches gerados por `tools/make_rpatch.py`; aplicador `tools/rpatch.ps1`.
