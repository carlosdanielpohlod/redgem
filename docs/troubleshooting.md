---
description: "Known limits and what to do when something goes wrong."
---

# Limits and troubleshooting

## What RedGem does not do yet

Knowing this up front saves time:

- **Input** is keyboard, mouse and one gamepad. Keys and buttons cannot be rebound by the player.
- **In-game interface** is texts, rectangles, images, buttons and text fields you draw each frame.
  There are no sliders or layout containers.
- **Particles** are let out in bursts, as dots or pictures. They do not collide and are not lit.
- **Animation:** a character plays one clip with a second mixed over it. There are no blend trees,
  no animation events and no inverse kinematics.
- **Multiplayer** has no matchmaking and no lag compensation; playing over the internet needs a
  relay you run, or a forwarded port (see [Multiplayer](scripting/multiplayer.md)).
- **Platforms:** the editor is developed on Linux; the Windows and macOS downloads are
  experimental (macOS has only run on GitHub's virtual Macs, drawn by Apple's software renderer).
  Games are built for Linux, Windows and Android, and for the Mac from the Mac's editor (no iOS,
  no web).
- **macOS says the app cannot be opened** ("unidentified developer", or "damaged"): the app is
  not signed by a registered Apple developer. Right-click `RedGem.app` and choose *Open* (on
  macOS 15 and later: *System Settings > Privacy & Security > Open Anyway*), or run
  `xattr -dr com.apple.quarantine RedGem.app` in the unpacked folder.
- Scripts are loaded when you press Play; they are not reloaded while the game runs.
- **Scene size:** a scene (or a model) that would need more than 4 GB of memory, counting its file,
  its vertices as the engine keeps them and its pictures as they are once decoded, is refused with
  a message saying how much it needs. The biggest demo scene comes to about 1.8 GB. This is an
  estimate of the memory the game holds, not a limit on the graphics card's memory.

## Common problems

**The editor does not open from Docker.**
Your session must use X11 (`echo $XDG_SESSION_TYPE` prints `x11`). The launcher
`tools/docker/editor.sh` allows the container on your display and removes that permission when
the editor closes.

**The build fails looking for X11 headers.**
Install the development packages listed in [Install](install.md). If you cannot install
packages, `tools/setup/fetch_x11_headers.sh` extracts the headers into the project.

**My script does nothing.**
Look at the Console: a script with an error is switched off and reported there. Check that the
class name matches the file name (`moving_door.rb` defines `MovingDoor`) and that it inherits
from `Behaviour`.

**The game freezes for a moment and a behaviour stops, with `ScriptTimeout` in the Console.**
A call into that behaviour ran longer than the scripts' budget (1000 ms unless `game.json` says
otherwise): it is stuck in a loop, or does far too much in one go. See
[Behaviours](scripting/behaviours.md#a-script-that-never-returns).

**A built game closed by itself.**
A game that crashes writes a report and tells the player where it is (in a message box on
Windows; on Linux and the Mac on its error stream, which a terminal shows). The report is a text
file beside the game's save, in the player's home folder, in a folder named after the save:

```
.config/redgem/saves/by-id/<id>-crashes/crash-<date>-<time>.log                 Linux, Windows
Library/Application Support/RedGem/saves/by-id/<id>-crashes/crash-<date>-<time>.log   macOS
```

(A game with no `id` uses `saves/<folder>-<digits>-crashes/`.) It says the game, the engine's
version, the system, the time in UTC, what happened (the signal, such as `SIGSEGV`, or the
exception and its message) and, where the system can say, the functions the program was in.
Send it with a bug report. The save itself loses nothing: it is written to the disk on every
change. A built game for Android writes no report yet: look in `adb logcat`.

**`on_trigger_enter` never fires.**
The object needs *Trigger* on, and the thing entering must be a character, a dynamic body or a
kinematic body. Static scenery does not set triggers off.

**The hero falls through a platform I move from a script.**
Give the platform a *kinematic* body. A static body is not meant to move.

**The game is slow on a laptop with two graphics cards.**
RedGem asks for the dedicated card and logs which one it got when it starts. To choose:
Edit > Graphics Card in the editor, or start with `--gpu dedicated`.

**Joining fails with "this game runs protocol 1, the other side 2".**
The two machines (or the game and the relay or server it meets on) run versions of RedGem whose
network messages differ. Build every copy of the game, the relay and the server from the same
version.

**I want to see what the machines of a game say to each other.**
Start the game, the relay or the server with `REDGEM_LOG_MESSAGES=1` in its environment: every
message it sends and receives is written to its log, as JSON text.

**I changed the level in Blender and the editor did not update.**
It reloads `scene.glb` by itself when you have no unsaved edits. With unsaved edits it asks.
