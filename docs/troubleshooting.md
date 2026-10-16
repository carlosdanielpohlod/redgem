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

**`on_trigger_enter` never fires.**
The object needs *Trigger* on, and the thing entering must be a character, a dynamic body or a
kinematic body. Static scenery does not set triggers off.

**The hero falls through a platform I move from a script.**
Give the platform a *kinematic* body. A static body is not meant to move.

**The game is slow on a laptop with two graphics cards.**
RedGem asks for the dedicated card and logs which one it got when it starts. To choose:
Edit > Graphics Card in the editor, or start with `--gpu dedicated`.

**I changed the level in Blender and the editor did not update.**
It reloads `scene.glb` by itself when you have no unsaved edits. With unsaved edits it asks.
