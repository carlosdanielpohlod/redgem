# Limits and troubleshooting

## What RedGem does not do yet

Knowing this up front saves time:

- **Input** is keyboard only. There is no mouse or gamepad input for games.
- **In-game interface** is text (`UI.text`). There are no buttons, images or menus.
- **Saving progress** has no API.
- **Particles** do not exist; small effects are objects moved by a script.
- **Animation:** every animation in a level loops all the time. You cannot pick which one plays.
- **Platforms:** Linux.
- Scripts are loaded when you press Play; they are not reloaded while the game runs.

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
