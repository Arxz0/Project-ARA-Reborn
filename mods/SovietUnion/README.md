# Soviet Union — Joseph Stalin 1.0

<img src="data/URSS/images/Stalin_Left_256x350.png" height="200" align="right" alt="Joseph Stalin">

A new playable nation for ARA: History Untold 2.1.0.542: the **Soviet Union** (`ntn_SovietUnion`), led by **Joseph Stalin**. Part of **Project ARA: Reborn**; works on its own or together with Brazil.

## Content
- **Leader feature — Great Patriotic War:**
  - +25% strength for land armies within your territory;
  - +50% craft rate in Military improvements;
  - −50% war weariness.
- **Unique unit — T-34:** Red Army medium tank.
- AI personality (Aggressive, Efficient, Disciplined, Vindictive), own improvements, recipes and technologies. Starts in Moscow.
- **Two 3D leader looks** in the leader screen:
  - **1924:** brown coat;
  - **1945:** marshal, with white uniform and peaked cap.
- Portraits, emblems and a flag.
- **Custom introduction narration** in English and Brazilian Portuguese. It was generated with Coqui XTTS-v2 using a stock studio voice; no real person's voice was cloned.
- **Localization:** English and **Brazilian Portuguese (PT-BR)**, following the game language.

## Compatibility
- Works with every other Project ARA: Reborn mod, including the Brazil mod: both use the same leader base and ship the same starting positions file.
- **DLL:** the leader base replaces `bin\swscale-7.dll`. Another mod that replaces the same DLL can't be used at the same time.
- **Game data:** the mod replaces these game entries:
  - technology `tch_InternalCombustionEngine`, which unlocks the T-34;
  - improvement `imp_VehicleFactory`, where the T-34 is built;
  - the Earth map starting positions (`UniqueStartInfo`), which place the Soviet Union in Moscow.

  When another enabled mod replaces the same entries, the game uses only one version, and parts of the mod can stop working: for example the T-34 is never unlocked, or the Soviet Union gets no fixed starting position.
- **Known conflict:** *Naxt's Grand ReFlavor Mod* (mod.io) replaces `tch_InternalCombustionEngine` and `imp_VehicleFactory`. Don't enable it together with this mod.

## Install
1. Close the game.
2. Run `INSTALL.bat`.
   - It installs the **leader base**, which patches the game's character packages from the original Steam files. Details are in `base\BASE_README.md`. About 4.5 GB of free space is needed during installation.
   - It copies `Mods\URSS`.
3. Enable the **URSS** mod (the mod's folder name) when starting a **new game**.

To remove the mod, run `UNINSTALL.bat`. If the Brazil mod is still installed, the leader base stays. Otherwise the original game files are restored.

## Credits
Marshal cap adapted from [*"Russian Imperial WW1 Army Cap"*](https://sketchfab.com/3d-models/russian-imperial-ww1-army-cap-d5d8d1c2636946a1adbaaeb2fe5dcd92) by **dawiderodawid** (Sketchfab), [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Details are in `CREDITS.txt`.
