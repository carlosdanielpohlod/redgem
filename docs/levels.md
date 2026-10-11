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
not drawn. What costs most is the number of separate objects on screen, more than their triangles:
join the parts of a model that share a material, and give heavy models lighter versions for far
away with `visible_from` and `visible_to`.

## Terrain

**GameObject > Terrain** adds a square of ground, 100 m across, that you shape with a brush.

1. Select the terrain. In the Inspector's **Terrain** card, tick **Sculpt**.
2. Drag the left mouse button over the ground in the Scene view. A ring shows the brush.
3. Pick what the brush does:

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

