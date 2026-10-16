# Leader base (Brazil and Soviet Union)

Not a separate download: it ships inside the **Brazil** and **Soviet Union** zips. Both nation mods use the same base. It is installed once, and installing the second nation reuses it.

## What it contains
- **DLL `bin\swscale-7.dll`** (AraLeaderBuilderUnlock v1.19; the original is kept as `swscale-7_orig.dll`):
  - unlocks the modded leaders and nations in the game's leader builder;
  - assembles their 3D looks: face, outfit, hair, moustache and the 1945 marshal cap;
  - copies the texts for the game language (English or Brazilian Portuguese) into the mod;
  - plays the custom leader introduction narration.
- **3D parts and emblems** written into the game packages:
  - `Characters.OxAssetInfoPackage` and `Characters.OxModelPackage` (in both `PackagedData` and `PackagedDataHD`);
  - `Characters.OxTexturePackage`;
  - `UI.OxTexturePackage` (flag emblems);
  - `UI\Art\Icons\RGBEmblems.xml`;
  - plus the new flags in `GameCore\Flags`.

## How installation works
- **Binary patches from the original files.** The packages are rebuilt from the user's own original Steam files. The installer checks the original's SHA-1 first, builds the new file in a temporary file, checks the result's SHA-1, and only then swaps it in.
- **No game files in the zip.** The zip carries only the differences: the mods' parts and textures, plus index tables.
- **Uninstall restores the originals.** The original bytes that change are saved locally in `Documents\My Games\Ara History Untold\Mods\_RebornLeaders`. When the last nation mod is uninstalled, every file goes back to the original.
- **Free space.** About 4.5 GB is needed temporarily during installation; the character texture package alone is 3.4 GB.
- **Game updates.** If a Steam update changes these packages, the patches no longer match. The installer stops without changing anything.

Sources: `src/ara_unlock.c`. The patches are built with `tools/make_rpatch.py` and applied by `tools/rpatch.ps1`.
