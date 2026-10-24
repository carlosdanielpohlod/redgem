---
description: The editor's panels, tools and shortcuts: placing objects, the Inspector, playing.
---

# The editor

```bash
./build/src/redgem games/platformer          # opens the editor
./build/src/redgem games/platformer --play   # just plays the game
```

The editor is built with Dear ImGui (dark theme, Inter and JetBrains Mono fonts, Font Awesome
icons):

* **Toolbar**: move / rotate / scale gizmos, local or world axes, grid snapping (or hold Ctrl while
  dragging), Play / Pause / Stop, and Build.
* **Hierarchy**: the object tree with search. Click to select, double click to frame, drag onto
  another object to parent it (or onto empty space to unparent), right click for actions and to
  create objects. Scripts and models from the Project panel can be dropped on it.
* **Scene**: hold the right mouse button and use WASD/QE to fly, wheel to dolly, F to frame the
  selection, click to pick objects, W / E / R switch the gizmo. Drop a model on the ground to place
  it there, or a script on an object to attach it.
* **Inspector**: name, tag, visible, *Template* (hidden in the game, copied with `Scene.spawn`), then
  one card per component: Transform, Mesh with its materials (color, metallic, roughness, emission),
  Light (type, color, intensity, range), Camera (field of view, clipping), one card per Ruby script,
  Physics (body, trigger, collider, mass), vehicle wheels and custom properties. **Add Component**
  adds scripts (existing or new), a physics body, a trigger or a property.
* **GameObject menu**: terrain you sculpt with a brush (see [Levels](levels.md#terrain)), empty
  objects, cube / sphere / plane / cylinder / capsule (with colliders),
  directional / point / spot lights, cameras, and the game's models.
* **Your own models and sounds**: drop `.glb` files (exported from Blender) and `.wav` / `.ogg` /
  `.mp3` / `.flac` files from your file manager onto the editor window. They are copied into the
  game's `models/` and `sounds/` folders and show up in the Project panel.
* **Project**: the game's scripts, models, sounds and scenes. Double click a script to edit it, a
  model to add it to the scene, a sound to hear it, a scene to open it (see [Scenes](#scenes)).
  *New script* creates a `Behaviour` from a template, *New scene* a scene; *Open folder* shows the
  files.
* **Console**: engine and Ruby output (filter by level), including script errors with backtraces.
* **Scripts**: each open Ruby file gets its own tab next to the Scene. Drag tabs to split the view
  anywhere (or press *Split right*), like a code editor's editor groups. Ctrl+S saves and checks
  the syntax (errors show the line); *Open externally* uses your default editor. Play saves all
  open scripts and reloads them.
* **View menu**: hide the Hierarchy (Ctrl+B), the Inspector (Ctrl+Alt+B) or the bottom panels
  (Ctrl+J) to give the scene or the code more room; the toolbar's outer buttons toggle the sides.
* **Play / Pause / Stop** (Ctrl+P, Esc): runs the game inside the Scene view with the current edits
  and fresh scripts; Stop restores the scene exactly as it was.
* **Ctrl+Z / Ctrl+Y** undo and redo, **Ctrl+D** duplicate, **Del** delete, **Ctrl+S** save. Help >
  Keyboard Shortcuts lists them all. File > New / Open Game goes back to the hub.

Undo goes back up to 200 steps, and keeps at most 256 MB for them (its copies of the objects and
terrains, and what deleted objects and undone imports were made of: vertices, pictures and their
bytes in the scene's file; an estimate, counted as each step begins): past either, the oldest steps
go, and what no step can bring back any more is let go of, so a long session with many imports does
not keep growing. The latest step always stays, however big.

Saving rewrites the open scene's file in place (only the glTF JSON; meshes and textures are
untouched), so the file still opens in Blender. When the file changes on disk (e.g. you re-export
from Blender) the editor reloads it automatically, unless you have unsaved edits.

## Scenes

A game can have several scenes: levels, missions, menus. Each is a `.glb` file, known by the name
a script loads it with:

| Scene | File | Loaded with |
|-------|------|-------------|
| `main` | `scene.glb` | `Game.load_scene("main")` |
| `level2` | `scenes/level2.glb` | `Game.load_scene("level2")` |

The game always starts on `main` (`scene.glb`); scripts move to the others with
`Game.load_scene`. The editor opens on `main` too, and edits one scene at a time:

* **Open** another scene with a double click in the Project panel's *Scenes* folder, or with
  File > Open Scene. When the open scene has unsaved edits, the editor asks first (Save, Discard
  or Cancel), like when closing. Each scene has its own undo history: it starts empty when the
  scene opens.
* **New scene** (File > New Scene, or *New scene* in the *Scenes* folder) makes
  `scenes/<name>.glb` with ground, a sun and a camera, and opens it. Names are lowercase letters,
  digits, `_` and `-`, starting with a letter (`level2`, `boss-fight`); `main` is taken.
* **Save** (Ctrl+S) writes the open scene back to its own file. File > Save Scene As writes it as a
  new scene of the game, which is then the one open. A scene that uses files next to it (textures
  exported outside the `.glb`) is not saved into another folder that way, where it would not find
  them: export it from Blender as a `.glb` with its textures in it first.
* **Play** starts the game on the open scene as it is edited, under its own name
  (`Game.scene_name`, and its `scripts/<name>_scene.rb` runs). Stop comes back to it.

The window title and the status bar name the open scene, and the editor follows that scene's file
for changes made outside. Built games carry every scene: Build copies the whole game folder,
`scenes/` included.
