# AraQoL 2.0

Camera and interface fixes for ARA: History Untold 2.1.0.542. Part of **Project ARA: Reborn**; works on its own.

## Features
- **Higher camera ceiling.** The maximum camera height no longer depends on how much of the map you have explored (the region multiplier goes from 2.0 to 100). Strategic View is reachable from the start.
- **Map banners that obey their toggle.** Resources, yields, improvements and forces: when a toggle is off, those banners stay hidden at every zoom level; when it is on, they are always shown.
- **Easier unit clicking.** An extra click radius around the 3D unit models.
- **Upgrade All.** Two new buttons:
  - in the army upgrade panel: picks the best unlocked upgrade for every unit, and you still confirm the cost as usual;
  - in the Military panel: orders every army that can upgrade right now to do it.

  Button labels follow the game language.

The CPU/priority fix that used to live in AraQoL 1.9 is now the separate **ARA CPU Fix** mod.

## Settings (`bin\AraQoL.ini`)
| Key | Default | Meaning |
|---|---|---|
| `[Camera] ZoomMaximo` | `1` | `1` = higher camera ceiling, `0` = original ceiling |
| `[Clique] RaioExtraUnidades` | `40` | extra click radius around units (re-read every 2 s) |
| `[UpgradeAll] TextoPainel` / `TextoMilitar` | `auto` | button labels (`auto` = game language) |
| `[UpgradeAll] PularExercitosEmMarcha` | `1` | skip armies that are currently marching |

## Install
Close the game and run `INSTALL.bat`. It replaces `bin\swresample-4.dll`; the original is kept as `swresample-4_orig.dll`. The log is written to `bin\AraQoL.log`.
To remove the mod, run `UNINSTALL.bat`.
