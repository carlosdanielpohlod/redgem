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
| `sky_image`, `sky_rotation`, `sky_intensity` | a picture of the sky in the game's `images/` folder, drawn behind everything and lighting the scene (see "A sky picture"), the degrees it is turned around the vertical, and its brightness (1 = as the picture is) |
| `msaa` | samples per pixel of the scene's picture (2, 4 or 8; 0, the default, is one): smoother edges, and grass and leaves whose cut-out edges are smoothed too |
| `shadow_cascades`, `shadow_distance` | sun shadows over a large area: 1 to 4 maps, each farther from the camera, reaching `shadow_distance` meters (150 unless given) |
| `shadow_softness` | how wide the sun is, in degrees: shadows grow softer the farther they fall from what casts them (the real sun is about 0.5; 0, the default, keeps every edge equally soft) |
| `glow_limit` | the brightest glowing particles together make a pixel, in linear light (2 to 4 suits fire): many flames or sparks over each other come near it and keep their color instead of washing out to white (0, the default, sets no limit) |

Without one, scenes get a clear day. Other things worth knowing: the 16 point lights nearest to the
camera are used each frame (so a street can have dozens of lamps); the sun casts soft shadows
around the camera; and materials use their glTF normal, roughness/metallic, occlusion and emission
textures. A light's intensity and color can change while the game runs (see
[Gameplay recipes](scripting/gameplay.md#light-that-flickers-and-fades)).

### See-through materials and wind

Blender's material settings decide how a material shows what is behind it (the glTF exporter
writes them as the material's alpha mode):

| Alpha mode | Drawn as | For |
|------------|----------|-----|
| Opaque | whole: its alpha is not looked at | almost everything |
| Mask (alpha clip) | cut out where its alpha is below the material's cutoff | leaves, grass, fences |
| Blend | see-through: mixed with what is behind it by its alpha | glass, hair cards, fur, smoke on a card |

A blended material is drawn in two steps. What of it is nearly opaque (alpha 0.9 and above) is
drawn with the opaque surfaces, so that a dozen hair cards over each other still sort themselves
out; the rest is drawn after every opaque surface, mixed with what is behind it, one object at a
time from the farthest to the nearest. Two see-through parts of the same object are not sorted
between themselves, nor are copies scattered with `Scatter.place`. For many layers of fine cards
(fur, a mane), give the material the custom property `transparency = "dithered"` (Material
Properties → Custom Properties, exported when the exporter's *Custom Properties* is on): its texels
are then left out in a fine pattern that thins with their alpha, which needs no sorting at all,
at the price of a slight grain.

**Wind.** Textured cut-out and blended materials are foliage: drawn on both sides, and swaying
with the environment's `wind`. An object whose foliage must stay still gives itself `sway =
false`; objects moved by a skeleton (characters, and so their hair) do not sway unless they say
`sway = true`. Copies scattered from an object sway as it does.

Glossy surfaces reflect their surroundings: the engine keeps capturing the scene around the camera
(one cube face per frame) and uses it for reflections and ambient light.

**A sky picture.** `sky_image = "meadow_sky"` puts a picture of the whole sky around the scene
(`images/meadow_sky.hdr`, `.png` or `.jpg`): an *equirectangular* picture, twice as wide as it is
high, the kind sites of free skies offer and Blender takes as a world texture. It is laid as
Blender lays it (the middle of the picture toward +X; `sky_rotation` turns it), and it lights the
scene in place of the gradient: matte surfaces take the light of the sky around them, and glossy
ones reflect it, sharper the smoother they are. Give the picture's sun a sun light of its own
pointing the same way (the picture's own sun is held down in the light it gives, so that it does
not light the scene twice) and set `ambient` to 1 for the sky's light as the picture has it.
A Radiance picture (`.hdr`) holds light brighter than white, as a real sky has; a PNG or JPEG
stops at white, and `sky_intensity` makes up for it. Pictures are read up to 8192 pixels a side;
the sky drawn behind everything is kept at 4096 pixels across, its light is worked out from a
smaller copy when the scene is loaded (a fraction of a second).

**Large outdoor scenes.** With `shadow_cascades = 4` the sun's shadows reach `shadow_distance`
meters, sharp up close and coarser far away; `shadow_softness = 1.5` widens their edges with the
distance to what casts them (a tree's shadow is sharp at its trunk and soft at its crown). Each
cascade draws the scene's casters once more: a forest of tens of thousands of copies costs more
with four than with one. `msaa = 4` smooths edges with four samples a pixel; cut-out materials
(grass, leaves) then let those samples smooth their edges too, which keeps a meadow from
shimmering as the camera moves. Without it, cut-out edges are smoothed afterwards with the rest of
the picture.

**Animated characters.** glTF skeletons and animations are supported: skinned meshes are deformed on
the GPU (up to 1024 bones per skeleton, 4 per vertex: a character with fingers has about 65) and every animation in the scene plays in a
loop until a script says otherwise (see [Control animations](scripting/gameplay.md#control-animations)). The editor keeps skeletons and animations when saving, reordering or importing models.
`tools/blender/walk_rig.py` gives a scanned standing person leg bones and a walk cycle.

**Per-object color.** The property `tint = "#rrggbb"` multiplies the colors of an object's mesh, so
copies sharing one mesh (cars, crates) can each have their own color.

**No shadow.** `cast_shadow = false` keeps an object out of the sun's shadows (a sky dome, a glow,
a light's shade): it is still drawn and still receives shadows.

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

The copies take the template's own mesh (a model whose parts are children: join them), its scale,
tint, `visible_from` / `visible_to` and `cast_shadow`. They are only drawn: nothing collides with them (give the
trees a collider of their own, or invisible objects, where players must not walk through) and no
script runs on them. The same seed places them the same on every machine of a game.

They are drawn by patches of ground 16 m across: a patch out of view, or too far for its biggest
copy to show, is skipped whole. Like any object, they cast shadows, and small things (grass) only
near the camera: a tuft half a meter high stops casting one about 50 m away, a tree hundreds of
meters away. Their foliage sways in the wind (`wind` on the environment), each copy with its own
phase, so a field does not move in step.

How the copies look can vary too, the same for the same seed on every machine:

```ruby
Scatter.place("GrassTuft", count: 50_000, area: field, ground: find("Ground"), seed: 7, align: true,
              tints: [[0.85, 1.0, 0.7], [1.05, 1.0, 0.8]],  # each a color between these
              tint_variation: 0.12,                         # and a little brighter or darker
              fade: 10.0,                                   # fading out over the last 10 m
              normals_up: 0.7)                              # lit as the meadow, not as flat cards
```

`fade` makes copies near the template's `visible_to` thin out pixel by pixel over that many meters
instead of popping away (and fade in after its `visible_from`, when it has one: two levels of
detail whose distances overlap by the fade cross over without a gap). `normals_up` bends the
copies' normals toward their own up: grass cards are then lit as the ground they stand on, so a
field does not show the cards' faces turning light and dark with the sun.

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
| `cast_shadow` | `false`                                     | Keeps the object out of the sun's shadows (a sky dome) |
| `sway`     | `true` / `false`                               | Whether the wind sways its foliage: unless said, yes, except on objects a skeleton moves (see "See-through materials and wind") |
| `environment` | `true`                                      | This object's properties set the sky, fog and effects (see "Look of a scene") |
| anything   | number / string / bool                         | Available in Ruby as `props[:name]` / `prop(:name, default)` |

Cameras and lights (sun, point) are taken from the Blender scene. Spot lights are loaded but lit
like point lights (in every direction). Parenting is preserved.

**Vehicles.** Give the chassis `body = "vehicle"` and parent the wheel meshes to it with `wheel = true`.
Wheels in front of the center (Blender `+Y`) steer, the rear ones get the hand brake, and every axle
is driven. Wheel radius and width come from the wheel mesh. See `tools/blender/build_driving.py`.
Blender is Z-up; the engine is Y-up like glTF: Blender `(x, y, z)` is engine `(x, z, -y)`
(`Vector3.blender(x, y, z)` does the conversion). Forward is `-Z` (Blender `+Y`).

