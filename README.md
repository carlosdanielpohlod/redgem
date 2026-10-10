# RedGem Engine

A 3D game engine whose games are written in **Ruby**. It comes with an **editor** with play mode,
uses **Blender** for modeling, and runs on a small C++17 runtime built on embedded
[mruby](https://mruby.org), [raylib](https://www.raylib.com) (window, input, rendering),
[Jolt Physics](https://github.com/jrouwe/JoltPhysics) and [cgltf](https://github.com/jkuhlmann/cgltf).
Objects carry behaviours, plain Ruby classes with `start`, `update`, `fixed_update`,
`on_trigger_enter`...

```ruby
class Coin < Behaviour
  SPIN_DEGREES_PER_SECOND = 180.0

  def update(dt)
    transform.rotate(Vector3.new(0, SPIN_DEGREES_PER_SECOND * dt, 0))
  end

  def on_trigger_enter(other)
    return unless other.get_component(Player)

    GameManager.instance.collect_coin
    destroy
  end
end
```

## How it works

1. Model the level in Blender. Gameplay is configured with **custom properties**
   (*Object Properties → Custom Properties*), e.g. `script = "coin.rb"`, `trigger = true`.
2. Export to `scene.glb` (glTF binary). The exporter writes custom properties to each node's `extras`.
3. The runtime loads the `.glb` with cgltf, reads the extras, builds physics bodies and instantiates
   the Ruby class named after each script (`camera_follow.rb` → `CameraFollow`).

A game is just a folder:

```
games/platformer/
  scene.glb          # exported from Blender
  platformer.blend   # the editable source
  scripts/*.rb       # behaviours
```

## Building

Requirements: a C++17 compiler, CMake ≥ 3.24, Ninja (optional), Ruby + rake (to build mruby),
X11/OpenGL development files and Blender 4.5 (only to regenerate scenes).

```bash
tools/setup/fetch_x11_headers.sh   # only if Xrandr/Xinerama/Xcursor headers are missing (no root needed)
cmake -S . -B build -G Ninja
cmake --build build
```

All dependencies are fetched by CMake at pinned versions (raylib 5.5, mruby 3.4.0, cgltf 1.15,
Jolt 5.3.0, Dear ImGui 1.92.8-docking with rlImGui and ImGuizmo, nlohmann/json 3.12.0, doctest 2.4.11).

## Making a game

Run `./build/src/redgem` with no arguments to open the **project hub**: pick a recent game, or
create one from a template (*Empty* or *Platformer*, saved in `~/RedGemGames`). The same from a
terminal:

```bash
./build/src/redgem new games/my_game --template platformer   # or --template empty
./build/src/redgem games/my_game                              # edit it
./build/src/redgem build games/my_game dist/my_game           # standalone copy: dist/my_game/run.sh
```

A game folder holds `scene.glb` (the main scene), `scenes/*.glb` (more levels), `scripts/*.rb`,
`sounds/*.wav|ogg|mp3`, `models/*.glb` and `game.json` (window title and size).

A typical loop: add objects (**GameObject** menu, or drop `.glb` models from Blender into the scene),
give them physics and a Ruby script with **Add Component**, write the behaviour in the Script panel,
press **Play**, tweak, **Save**, and **Build** when it is ready to share.

## The editor

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
* **GameObject menu**: empty objects, cube / sphere / plane / cylinder / capsule (with colliders),
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

Demos: `games/city` (a crossroad with Ruby-driven traffic: lights, braking, spinning wheels; Kenney
CC0 models), `games/platformer` (WASD/arrows, Space jumps: grab 5 coins, reach the flag) and
`games/driving` (W/S throttle, A/D steer, Space hand brake, R reset: drive laps).

**Showcase** (`games/showcase`, not committed: it is built from downloaded assets). A street at
dusk with a drivable car, traffic that keeps its distance and overtakes, parked cars, shop fronts
and cafe tables, a crowd on the sidewalks, potholes and falling leaves, made to push the renderer. Blender only builds the scene; everything
that happens in it is set up in `games/showcase/scripts/main_scene.rb`:

```bash
python3 -I tools/assets/fetch_showcase.py      # street: Poly Haven and ambientCG (CC0)
python3 -I tools/assets/fetch_ad_assets.py     # car and people: BlenderKit (see showcase/CREDITS.md)
SHOWCASE_RENDER=0 BLENDER_MEMORY_MAX=6500M tools/blender/run.sh build_showcase.py showcase
./build/src/redgem games/showcase           # then Play: WASD drives, Space brakes
```

The export needs about 4.5 GB of memory (close the editor first); without `SHOWCASE_RENDER=0` it
also renders `showcase/renders/ad.png` with Cycles, which needs an NVIDIA RTX card.
`games/city` is rebuilt
with `tools/assets/fetch_kenney.sh` and `tools/blender/run.sh build_city.py games/city`.

## Look of a scene: environment, lights and effects

Scenes are drawn in HDR and finished with bloom (bright things glow), filmic tone mapping, a
vignette and anti-aliasing. One object with the property `environment = true` (GameObject >
Environment in the editor, or a Blender empty) sets the mood through its properties:

| Property | Meaning |
|----------|---------|
| `sky_top`, `sky_horizon`, `ground_color` | colors of the sky gradient, as `#rrggbb`; they also light the scene |
| `fog_color`, `fog_density` | distance fog (it follows the horizon color unless given its own) |
| `ambient`, `exposure` | strength of the sky's light, overall brightness |
| `bloom`, `vignette`, `sun_glow` | glow around bright things, dark corners, the sun's disc and halo (0 turns each off) |
| `wind` | how much foliage sways |
| `shadows` | darkness of the sun's shadows (0 = none, 1 = full) |
| `ambient_occlusion` | darkening of creases and contact areas (0 = none, 1 = full) |

Without one, scenes get a clear day. Other things worth knowing: the 16 point lights nearest to the
camera are used each frame (so a street can have dozens of lamps); the sun casts soft shadows
around the camera; materials use their glTF normal, roughness/metallic, occlusion and emission
textures; and textured materials with transparency are cut out by their alpha and sway with the
wind (leaves, fences).

Glossy surfaces reflect their surroundings: the engine keeps capturing the scene around the camera
(one cube face per frame) and uses it for reflections and ambient light.

**Animated characters.** glTF skeletons and animations are supported: skinned meshes are deformed on
the GPU (up to 64 bones per skeleton, 4 per vertex) and every animation in the scene plays in a
loop. The editor keeps skeletons and animations when saving, reordering or importing models.
`tools/blender/walk_rig.py` gives a scanned standing person leg bones and a walk cycle.

**Per-object color.** The property `tint = "#rrggbb"` multiplies the colors of an object's mesh, so
copies sharing one mesh (cars, crates) can each have their own color.

**Graphics card.** On laptops with two cards the engine asks for the dedicated one and logs which
card it got. To choose: Edit > Graphics Card in the editor (saved in
`~/.config/redgem/settings.json`, used from the next start), the `REDGEM_GPU` environment
variable, or `--gpu auto|dedicated|integrated` for one run. The command line wins over the
environment, which wins over the saved setting.

**Performance.** Objects outside the view, and parts too small to see from where the camera is, are
not drawn.

## Editing the level in Blender

```bash
blender games/platformer/platformer.blend           # edit, save
blender -b games/platformer/platformer.blend \
  --python tools/blender/export_scene.py -- games/platformer/scene.glb
```

`tools/blender/run.sh build_platformer.py games/platformer` (or `build_driving.py games/driving`)
regenerates a whole level from code; `tools/blender/scene_kit.py` has helpers to build levels with `bpy`.

### Custom properties

| Property   | Values                                         | Meaning |
|------------|------------------------------------------------|---------|
| `script`   | `"player.rb"` or `"a.rb, b.rb"`                | Behaviours to attach, loaded from `scripts/` |
| `body`     | `static`, `dynamic`, `kinematic`, `character`, `vehicle` | Physics body (characters use Jolt's `CharacterVirtual`, vehicles its `VehicleConstraint`) |
| `wheel`    | `true`                                         | On a child of a `vehicle`: it becomes a wheel (model it with its axle along X) |
| `collider` | `box`, `sphere`, `capsule`, `mesh`             | Shape, sized from the mesh bounds × object scale. `collider` alone = static body |
| `trigger`  | `true`                                         | Trigger volume: fires `on_trigger_enter/exit`, does not collide |
| `mass`     | number                                         | Mass of dynamic bodies |
| `tag`      | string                                         | Used by `Scene.find_all(tag)` and `game_object.tag?` |
| `visible`  | `false`                                        | Hide the mesh (the object still exists) |
| `prefab`   | `true`                                         | A template: hidden and inert in the game, copied by `Scene.spawn` |
| `tint`     | `"#rrggbb"`                                    | Multiplies the colors of this object's mesh (copies sharing a mesh can differ) |
| `environment` | `true`                                      | This object's properties set the sky, fog and effects (see "Look of a scene") |
| anything   | number / string / bool                         | Available in Ruby as `props[:name]` / `prop(:name, default)` |

Cameras and lights (sun, point) are taken from the Blender scene. Spot lights are loaded but lit
like point lights (in every direction). Parenting is preserved.

**Vehicles.** Give the chassis `body = "vehicle"` and parent the wheel meshes to it with `wheel = true`.
Wheels in front of the center (Blender `+Y`) steer, the rear ones get the hand brake, and every axle
is driven. Wheel radius and width come from the wheel mesh. See `tools/blender/build_driving.py`.
Blender is Z-up; the engine is Y-up like glTF: Blender `(x, y, z)` is engine `(x, z, -y)`
(`Vector3.blender(x, y, z)` does the conversion). Forward is `-Z` (Blender `+Y`).

## Ruby API

| Object | Highlights |
|--------|-----------|
| `Behaviour` | `start`, `update(dt)`, `fixed_update(dt)`, `on_trigger_enter/exit(other)`, `on_collision_enter/exit(other)`, `on_destroy`; helpers `game_object`, `name`, `transform`, `character`, `rigidbody`, `vehicle`, `props`, `prop`, `find`, `find_all`, `destroy`, `get_component` |
| `GameObject` | `name`, `tag`, `tag?`, `props`, `alive?`, `destroy`, `visible` / `visible=`, `behaviours`, `parent`, `children`, `transform`, `character`, `rigidbody`, `vehicle`, `get_component(Klass)`, `add_component(Klass, speed: 2.0)`, `animation_offset(=)` (seconds a spawned copy's animations run ahead of the original's) |
| `Transform` | `position(=)`, `local_position(=)`, `rotation(=)` (Euler degrees), `rotate`, `look_at`, `translate`, `scale(=)`, `forward`, `right`, `up` |
| `CharacterController` | `move(velocity)`, `jump(speed)`, `grounded?`, `velocity` |
| `Rigidbody` | `velocity(=)`, `add_impulse(vector)` |
| `Vehicle` | `drive(throttle:, steering:, brake:, handbrake:)`, `stop`, `speed` (m/s, signed), `velocity` |
| `Physics` | `raycast(origin, direction, max_distance = 1000, ignore = nil)` → `{object:, point:, normal:, distance:}` or `nil` |
| `Input` | `axis(:horizontal / :vertical)`, `down?(:space)`, `pressed?`, `released?`, `action?(:jump)`, `action_pressed?` |
| `Time` | `delta`, `fixed_delta`, `elapsed`, `frame` |
| `Scene` / `Camera` | `Scene.find(name)`, `Scene.find_all(tag)`, `Scene.objects`, `Camera.main` |
| `SceneScript` | `setup`, `find`, `find_all`, `find!` (see *Setting a scene up in Ruby*) |
| `Scene.spawn` | `Scene.spawn("Bullet", position: v, rotation: euler)` copies a `prefab = true` template (hidden and inert in the game) with its physics and scripts; copies of animated characters move like the original |
| `Audio` | `Audio.play(:jump, volume:, pitch:)`, `Audio.music(:theme, loop: true)`, `Audio.stop_music`, `Audio.volume = 0.8` (files in `sounds/`) |
| `UI` / `Debug` / `Game` | `UI.text(str, x:, y:, size:, color:)`, `Debug.log/warn/error`, `Game.quit`, `Game.screenshot(name)`, `Game.load_scene("level2")` (`scenes/level2.glb`; `"main"` is `scene.glb`; Ruby globals survive), `Game.scene_name` |
| `Vector3` | arithmetic, `dot`, `cross`, `length`, `normalized`, `lerp`, `flat`, `with(y: 0)`, `distance_to` |

### Setting a scene up in Ruby

Behaviours can be attached in Blender (`script = "coin.rb"`) or from Ruby. A scene's setup script
is `scripts/<scene>_scene.rb` (`main_scene.rb` for `scene.glb`, `level2_scene.rb` for
`scenes/level2.glb`); its `setup` runs when the scene loads, before any behaviour starts:

```ruby
class MainScene < SceneScript
  ENEMY_SPEEDS = { "Slime" => 1.5, "Bat" => 4.0 }

  def setup
    find!("Player").add_component(Player, speed: 6.0)
    ENEMY_SPEEDS.each { |name, speed| find!(name).add_component(Patrol, speed: speed) }
    find_all("coin").each { |coin| coin.add_component(Coin) }
  end
end
```

A behaviour reads its settings with `prop(:speed, default)`: what `add_component` was given, else
the object's Blender property of that name, else the default. So the scene file can stay about
what is there and where, and Ruby about what it does. Scripts never `require` each other: the
first use of a class (`Patrol`) loads `scripts/patrol.rb`, behaviour or not.

Script errors are logged with a backtrace and only disable the failing behaviour; the game keeps running.
Assigning `transform.position` to a physics object teleports it (moving bodies also stop); changing
its rotation keeps its velocity. Vehicle wheel meshes must have their rotation applied (axle along X).

## Tests

```bash
cd build && ctest --output-on-failure        # everything
ctest -L system                              # only the system tests
```

A system test is a Ruby file run against a game:
`redgem <game_dir> --test <file.rb> [--screenshots <dir>] [--headless]` (`--headless` runs
without a window; `redgem --help` lists every option).

* **C++ unit + integration** (doctest): math, extras parser, glTF loading from Blender fixtures,
  scene hierarchy, physics world, Ruby VM, and a headless engine running the `sandbox` fixture game.
* **Ruby tests** run inside the engine's mruby: `redgem --ruby-test tests/ruby/vector3_test.rb`.
* **Editor system test** (`tests/editor_system/`) opens the editor window on a copy of the platformer,
  picks, edits, plays, stops, undoes and saves.
* **System tests** (`tests/system/<game>/*_test.rb`, run against `games/<game>`) open the real game
  window and drive it frame by frame with simulated input:

```ruby
SystemTest.define do |t|
  t.hold(:w, frames: 60)
  t.wait_until(frames: 30) { Scene.find("Coin1").nil? }
  t.assert("score counts the coin") { GameManager.instance.coins == 1 }
  t.screenshot("collect_coin_after")        # saved to build/screenshots/<game>/
end
```

Test fixtures are regenerated with
`tools/blender/run.sh build_test_fixtures.py tests/fixtures`.

## Layout

```
src/core      constants (config.hpp), math, logging, fixed timestep clock
src/scene     glTF loader (meshes, skeletons, animations), Blender extras parser, scene graph,
              animation playback
src/physics   PhysicsWorld interface + Jolt implementation
src/input     keys, axes/actions, keyboard and scripted input sources
src/audio     sound effects and music (raylib, or a recorder for tests)
src/script    mruby VM, behaviour host, native bindings
src/render    renderer (HDR, shadows, reflections, post-processing), shaders, environment,
              culling, graphics card choice, mesh conversion
src/editor    editor: scene document (undo, save, import), session (play mode), camera, picking,
              project hub, script tabs, ImGui panels and theme (src/editor/ui)
src/app       engine loop, game projects (templates, build, recent games), user settings and
              command line
ruby/engine   Ruby prelude embedded in the binary (Behaviour, Vector3, test DSLs)
templates     scripts of the new-game templates (embedded in the binary)
assets/editor fonts of the editor (Inter, JetBrains Mono; SIL Open Font License, see the license files)
games         demo games: scene.glb, scripts and the .blend each scene was built from
showcase      renders of the showcase scene and the credits of its assets
tools/blender scene builders (run with tools/blender/run.sh), walk_rig.py
tools/assets  downloaders of the free assets the demos use
tests         unit, integration, Ruby and system tests, with their fixtures
cmake         dependencies (pinned versions) and file embedding
```

## Not there yet

* Choosing which animation plays: every animation in a scene loops at once (fine for one clip per
  character). Characters copied with Duplicate in the editor keep the pose they were copied in.
* Spot light cones, and camera clip planes from the scene (the engine uses fixed ones).
* Reusing data when the same model is imported twice, and dropping the data of deleted imports
  from `scene.glb` (each import appends its meshes and textures).
* Mirrored objects (negative scale) are drawn inside out.
* More vehicle options from Blender properties (drive layout, engine torque); hot reload of
  scripts while playing.
