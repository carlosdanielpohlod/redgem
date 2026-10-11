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
  directional / point / spot lights, cameras, and models from `models/`.
* **Project**: the game's scripts, models, sounds and scenes. Double click a script to edit it, a
  model to add it to the scene, a sound to hear it. *New script* creates a `Behaviour` from a
  template; *Open folder* shows the files.
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

Saving rewrites `scene.glb` in place (only the glTF JSON; meshes and textures are untouched), so the
file still opens in Blender. When the file changes on disk (e.g. you re-export from Blender) the
editor reloads it automatically, unless you have unsaved edits.
