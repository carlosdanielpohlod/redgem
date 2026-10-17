# Levels, lighting and Blender

## How a scene is made

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
| `contrast`, `saturation` | the finished picture's grade: 1 leaves it as it is |
| `grain`, `aberration` | film grain, and color fringes toward the corners as through a lens (0 turns each off) |
| `glow_limit` | the brightest glowing particles together make a pixel, in linear light (2 to 4 suits fire): many flames or sparks over each other come near it and keep their color instead of washing out to white (0, the default, sets no limit) |

Without one, scenes get a clear day. Other things worth knowing: the 16 point lights nearest to the
camera are used each frame (so a street can have dozens of lamps); the sun casts soft shadows
around the camera; materials use their glTF normal, roughness/metallic, occlusion and emission
textures; and textured materials with transparency are cut out by their alpha and sway with the
wind (leaves, fences).

Glossy surfaces reflect their surroundings: the engine keeps capturing the scene around the camera
(one cube face per frame) and uses it for reflections and ambient light.

**Animated characters.** glTF skeletons and animations are supported: skinned meshes are deformed on
the GPU (up to 1024 bones per skeleton, 4 per vertex: a character with fingers has about 65) and every animation in the scene plays in a
loop until a script says otherwise (see [Control animations](scripting/gameplay.md#control-animations)). The editor keeps skeletons and animations when saving, reordering or importing models.
`tools/blender/walk_rig.py` gives a scanned standing person leg bones and a walk cycle.

**Per-object color.** The property `tint = "#rrggbb"` multiplies the colors of an object's mesh, so
copies sharing one mesh (cars, crates) can each have their own color.

**Graphics card.** On laptops with two cards the engine asks for the dedicated one and logs which
card it got. To choose: Edit > Graphics Card in the editor (saved in
`~/.config/redgem/settings.json`, used from the next start), the `REDGEM_GPU` environment
variable (read by `redgem` and `redgem-test`), or `--gpu auto|dedicated|integrated` for one run. The command line wins over the
environment, which wins over the saved setting.

**Performance.** Objects outside the view, and parts too small to see from where the camera is, are
not drawn. Objects that share a mesh (linked duplicates in Blender, Alt+D, or copies of one model)
and come one after the other in the level are drawn together, every copy of a part in one call,
however many there are: a street of the same building costs little more than one. Blender keeps
duplicates together by their names (Tree, Tree.001, Tree.002...); copies whose turn comes between
other objects are drawn in that order, so that surfaces lying on each other look as they did. What still costs is the number of different meshes on
screen, and of separate parts in each: join the parts of a model that share a material, and give
heavy models lighter versions for far away with `visible_from` and `visible_to`.

## Grass, forests and cities

Tens of thousands of copies (a meadow's grass, a forest) are too many to be objects: a script
scatters them instead (`Scatter.place`, see the [API reference](scripting/api.md)). Model one tuft
or one tree, give it `prefab = true` so the template itself is hidden in the game, and scatter it
over the ground from a scene script's `setup`:

```ruby
Scatter.place("GrassTuft", count: 50_000, area: [Vector3[-80, 0, -80], Vector3[80, 0, 80]],
              ground: find("Ground"), seed: 7, scale: 0.7..1.4, align: true)
Scatter.place("Tree", count: 2_000, area: [Vector3[-140, 0, -140], Vector3[140, 0, -90]],
              ground: find("Ground"), seed: 8, scale: 0.8..1.5)
```

The copies take the template's own mesh (a model whose parts are children: join them), its tint
and its `visible_from` / `visible_to`. They are only drawn: nothing collides with them (give the
trees a collider of their own, or invisible objects, where players must not walk through) and no
script runs on them. The same seed places them the same on every machine of a game.

They are drawn by patches of ground 16 m across: a patch out of view, or too far for its biggest
copy to show, is skipped whole. Like any object, they cast shadows, and small things (grass) only
near the camera: a tuft half a meter high stops casting one about 50 m away, a tree hundreds of
meters away. Textured cut-out materials sway in the wind (`wind` on the environment), each copy
with its own phase, so a field does not move in step.

## Terrain

**GameObject > Terrain** adds a square of ground, 100 m across, that you shape with a brush. It
comes selected with the brush on: mountains, holes and plateaus are a drag of the mouse away.

1. Drag the left mouse button over the ground in the Scene view. A ring shows the brush.
2. In the Inspector's **Terrain** card, pick what the brush does (**Sculpt** turns the brush off
   and on again, to move the terrain itself with the gizmo):

| Brush | Effect |
|-------|--------|
| Raise | Piles ground up under the brush (hold Shift to dig instead) |
| Lower | Digs (hold Shift to pile up) |
| Smooth | Softens bumps and edges |
| Flatten | Levels the ground to the height at the brush's center: plateaus, roads |

*Radius* is the size of the brush in meters, *Strength* how fast it works. Each stroke is one
undo step. Untick Sculpt to select and move objects again.

The terrain's collider follows its shape, so characters walk and things roll on the hills you
make. Its color and roughness are in the Mesh card, like any object's. A terrain is made and
shaped in the editor; one re-exported from Blender is an ordinary mesh.

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
| `visible_from`, `visible_to` | meters                          | The object is drawn only while that far from the camera: give a model a detailed version up close and a light one far away |
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

