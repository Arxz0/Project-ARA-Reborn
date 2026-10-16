# Railroads 1.9

Player-built railroads for ARA: History Untold 2.1.0.542. Part of **Project ARA: Reborn**; works on its own.

## Features
- **RAILROAD button** in the road management screen, unlocked by the **Steam Engine** technology. Pick a start and an end, and the game routes the line region by region, just like a road.
- **Cost per region:** Wood 3, Stone 3, Iron 2, Steel 2, with 30 production per region (turns of construction).
- **Rails replace roads.** Where a road already exists, the railroad takes its place. AI and player roads never overwrite a railroad.
- **Real track visuals.** Railroad links are drawn with the railroad track material that ships with the game but is otherwise unused. Inner region streets along the line become track too, so there is no stray asphalt in the middle of a railroad.
- **Train Station:** a new improvement, unlocked by Steam Engine, that acts as a road/railroad connection point in regions without a city.
- **Train Station 3D model (optional):** a dedicated warehouse-style station with tracks painted on its foundation.
  - Installed by `INSTALL.bat` through `station\station.py`. Needs **Python 3**; the installer adds `numpy` with pip if it is missing.
  - The original game packages are backed up in `Mods\Ferrovias\station_backup` and restored by the uninstaller.
- **Station aligned to the line (experimental).** The DLL turns and shifts only the station's visual so its track side sits on a straight stretch of the line.

## Important
- Enable the **Ferrovias** mod (the mod's folder name) when starting a **new game**. Costs, the improvement and the technology change are stored in each save.
- The mod replaces the `ConnectionPoints` and `Technologies` (Steam Engine) lists. Don't combine it with another mod that replaces the same lists, for example Workshop item 5293348.
- **Not done yet:**
  - dedicated railroad bridges (rivers still show road bridges);
  - railroad color and icon on the road map overlay;
  - moving trains.
- The station alignment has not been tested in game yet.

## Settings (`bin\Ferrovias.ini`)
| Key | Default | Meaning |
|---|---|---|
| `[Estacao] DesvioDaLinha` | `1` | `1` = align Train Stations to the line, `0` = off |
| `[Desenho] TrilhoDentroDaRegiao` | `0` | `1` = every inner street of a railroad region becomes track (older behaviour) |

## Install
Close the game and run `INSTALL.bat`. It:
1. replaces `bin\avformat-60.dll` (the original is kept as `avformat-60_orig.dll`);
2. copies `Mods\Ferrovias` to `Documents\My Games\Ara History Untold\Mods`;
3. installs the Train Station 3D model if Python is available. Without Python the station uses the factory model.

The log is written to `bin\Ferrovias.log`. To remove the mod, run `UNINSTALL.bat`.
