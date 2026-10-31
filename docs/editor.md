---
description: "The editor's panels, tools and shortcuts: placing objects, the Inspector, playing."
---

# The editor

```bash
./build/src/redgem games/platformer          # opens the editor
./build/src/redgem games/platformer --play   # just plays the game
```

The editor is built with Dear ImGui (dark theme, Inter and JetBrains Mono fonts, Font Awesome
icons):

* **Toolbar**: move / rotate / scale gizmos, local or world axes, the pivot several selected objects
  turn and scale around (their middle, or the first one selected), grid snapping (or hold Ctrl
  while dragging), Play / Pause / Stop, and Build.
* **Hierarchy**: the object tree with search. Click to select, Ctrl+click to add an object to the
  selection or take it away, Shift+click to select every row from the last one clicked; double
  click to frame, drag onto another object to parent it (or onto empty space to unparent; a row of
  the selection takes the whole selection along), right click for actions (on the whole selection) and to create objects. Scripts and models from the
  Project panel can be dropped on it.
* **Scene**: hold the right mouse button and use WASD/QE to fly, wheel to dolly, F to frame the
  selection, click to pick objects (Ctrl or Shift + click adds or takes away), drag a rectangle to
  select what shows inside it (with Ctrl or Shift held, added to the selection), W / E / R switch
  the gizmo. Drop a model on the ground to place it there, or a script on an object to attach it.
* **Inspector**: name, tag, visible, *Template* (hidden in the game, copied with `Scene.spawn`), then
  one card per component: Transform, Mesh with its materials (color, metallic, roughness, emission
  and their pictures, see [Material pictures](#material-pictures)),
  Light (type, color, intensity, range), Camera (field of view, clipping), one card per Ruby script,
  Physics (body, trigger, collider, mass), vehicle wheels and custom properties. **Add Component**
  adds scripts (existing or new), a physics body, a trigger or a property. With several objects
  selected it shows what they have in common: tag, visible, *Template* and the Transform, a dash
  where their values differ. An edit applies to all of them.
* **GameObject menu**: terrain you sculpt with a brush (see [Levels](levels.md#terrain)), empty
  objects, cube / sphere / plane / cylinder / capsule (with colliders),
  directional / point / spot lights, cameras, and the game's models.
* **Your own models, sounds and pictures**: drop `.glb` files (exported from Blender), `.wav` /
  `.ogg` / `.mp3` / `.flac` files and `.png` / `.jpg` / `.jpeg` / `.webp` pictures from your file
  manager onto the editor window. They are copied into the game's `models/`, `sounds/` and
  `images/` folders and show up in the Project panel.
* **Project**: the game's scripts, models, sounds, scenes and images. Double click a script to edit
  it, a model to add it to the scene, a sound to hear it, a scene to open it (see [Scenes](#scenes)),
  a picture to see it in your system's viewer.
  *New script* creates a `Behaviour` from a template, *New scene* a scene; *Open folder* shows the
  files.
* **Console**: engine and Ruby output (filter by level), including script errors with backtraces.
* **Scripts**: each open Ruby file gets its own tab next to the Scene. Drag tabs to split the view
  anywhere (or press *Split right*), like a code editor's editor groups. The code is colored as
  you type: keywords, strings, symbols, comments, numbers, constants and `@variables` each have a
  color of the editor's theme. Ctrl+S saves and checks
  the syntax (errors show the line); *Open externally* uses your default editor. Play saves all
  open scripts and reloads them.
* **Agent**: a chat with a coding agent (Claude Code, Codex or one of your own), in a tab beside
  the Inspector. It works in the game's folder and sees and changes the open scene through the
  editor's tools; see [The Agent panel](assistants.md#the-agent-panel).
* **View menu**: hide the Hierarchy (Ctrl+B), the Inspector (Ctrl+Alt+B) or the bottom panels
  (Ctrl+J) to give the scene or the code more room; the toolbar's outer buttons toggle the sides.
* **Play / Pause / Stop** (Ctrl+P, Esc): runs the game inside the Scene view with the current edits
  and fresh scripts; Stop restores the scene exactly as it was.
* **Ctrl+Z / Ctrl+Y** undo and redo, **Ctrl+D** duplicate, **Del** delete, **Ctrl+X / Ctrl+C /
  Ctrl+V** cut, copy and paste, **Ctrl+S** save. Help > Keyboard Shortcuts lists them all. File >
  New / Open Game goes back to the hub.

## Material pictures

Under a material's color, metallic, roughness and emission, the Inspector lists its five texture
slots, each with the picture it shows or *None*:

* **Color map**: the surface's colors, tinted by the material's color.
* **Surface map**: roughness in the green channel and metalness in the blue one (glTF's
  metallic-roughness map), scaled by the sliders.
* **Normal map**: small bumps and grooves, as a tangent-space normal map.
* **Occlusion map**: how much ambient light reaches each spot, in the red channel.
* **Glow map**: where the surface glows, tinted by the emission color.

Click a slot to pick one of the pictures of the game's `images/` folder (or *None*), or drag a
picture from the Project panel's Images onto it. The picture goes inside the scene's `.glb` (the
same picture used twice is stored once), so the scene still shows it in Blender and when copied to
another scene; a picture no material shows any more is left out when the scene is saved. Each
change is one undo step. PNG, JPEG and still WebP pictures can be used.

## Several objects at once

Everything selected moves, turns and scales together with the gizmo (around the middle of the
selection, or around the first object selected: the toolbar's pivot button; a scale goes along the
pivot's axes, and an object turned another way and stretched unevenly takes the nearest placement
without shear), and Duplicate, Delete,
Cut, Copy and Paste act on all of it. A child goes along with its selected parent. Each of these is
one undo step, however many objects it touches.

**Copy** takes the selected objects with everything under them: their meshes, materials, pictures,
skeletons and animations, scripts and properties, lights and cameras. **Paste** (Ctrl+V) puts them
next to the selected object, or at the top level when nothing is selected; **Paste as Child**
(Ctrl+Shift+V, or the Hierarchy's menu) puts them under the selected object. They land where they
were in the world, and a name that is already taken gets a number (`Crate.001`). Paste works in the
scene they came from, in another scene of the game (open it and paste), and in another editor
window: the copy goes on the system's clipboard as text. A copy bigger than 64 MB stays in the
editor's own clipboard and pastes only in the window it was copied in.

A pasted object has its own copy of its meshes and pictures, even in the scene it was copied from
(Duplicate shares them instead).

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
