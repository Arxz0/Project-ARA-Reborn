# ARA CPU Fix 1.0

Stops the whole-PC freeze that can happen during end-of-turn processing in ARA: History Untold. Part of **Project ARA: Reborn**; works on its own.

## The problem
The game:
- creates one worker thread per logical processor (minus one);
- pins each worker to a processor;
- lets the workers spin when they have no work, at high priority;
- raises its own process to "above normal" priority while the window has focus.

During turn processing every processor ends up busy above Windows itself, and the whole system can lock up.

## What the mod does
Everything happens in memory; no game file is changed.

1. **Reserves logical processors for Windows.** The game only sees and uses the others. This covers the process affinity, the game's thread-pinning calls, the processor count (so the game creates fewer workers) and the dynamic priority boost.
2. **Keeps the game at normal priority** while it has focus, instead of "above normal".

Both apply when the game starts. `bin\ARA_CpuFix.log` shows how many processors were left to the game.

## Settings (`bin\ARA_CpuFix.ini`)
| Key | Default | Meaning |
|---|---|---|
| `[CPU] ReservedProcessors` | `auto` | `auto`: 4 with 16+ logical processors, 2 with 12+, 1 with 6+, none below that. Or a number; `0` disables |
| `[Priority] CapAtNormal` | `1` | `1` = normal priority while focused, `0` = game's original behaviour |

## Install
Close the game and run `INSTALL.bat`. It replaces `bin\avutil-58.dll`; the original is kept as `avutil-58_orig.dll`. To remove the mod, run `UNINSTALL.bat`.

If you used AraQoL 1.9c, update to AraQoL 2.0, which no longer contains the CPU part. Otherwise the fix runs twice.
