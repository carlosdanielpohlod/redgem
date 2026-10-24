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
| `antialias` | `"temporal"` smooths edges with the frames before (see "Large outdoor scenes"); anything else, the default, smooths them after the picture is drawn |
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

`antialias = "temporal"` goes further: each frame the view moves by less than a pixel, a different
amount every frame, and the frames before, followed to where each pixel was then, are mixed into
this one. Thin grass, leaves and the glints of shiny surfaces stop shimmering, and copies fading
out by distance turn from a fine pattern into an even shade. What moves is followed, characters'
bones and swaying foliage included; what is no longer there (a lightning flash, a character
running past, the camera cutting to another place) is not smeared over the picture; particles
take less of the frames before. A new scene, a new window size and a camera that jumps start over.
It works with `msaa` on or off and costs one more pass over the scene's opaque surfaces.
History fades out locally between 16 and 32 pixels of motion per frame, and at large motion
splits around opening edges. Fast scenery therefore favors current detail, while a tracked
subject with little screen motion still accumulates samples. Reprojected previous-frame depth
rejects hidden or newly revealed surfaces, including moving and skinned objects. A still view
holds still and its edges, against the sky or other surfaces, settle smooth: depth is compared
around where each pixel was, so a silhouette keeps its history. Thin cut-outs and very fast details can still
shimmer as fewer temporal samples are retained.

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

### Coated materials

Blender's Principled BSDF coat is read through `KHR_materials_clearcoat`. A polished coating
can sit above rough paint, wood or a textured base. Its weight and roughness have independent
linear maps (red and green channels respectively), and its normal map does not change the base
normal. The coating reflects the sun, point lights and the environment while attenuating the
base layer and emission by Fresnel, rather than adding unlimited brightness.

Export the coat with the material in glTF. A zero coat weight preserves the original shading.
The editor keeps its factors, texture references and normal scale through import and saving.
The layering follows the [glTF clearcoat extension](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_clearcoat).

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

### Scatter distance LODs

Use lighter templates to keep vegetation visible far away without drawing the full mesh:

```ruby
forest = Scatter.place("Tree", count: 2_000, area: [corner_a, corner_b], seed: 7,
                       fade: 8.0, lods: [["TreeMid", 40.0], ["TreeFar", 120.0]])
```

All levels share the base template's placement, scale, tint, wind and shadow settings. Each
replacement contributes only its mesh and materials; model it with the same origin, orientation
and dimensions. The base template's `visible_from` and `visible_to` bound the whole chain.

At 40 meters the middle mesh begins appearing; over the next 8 meters it replaces the near
mesh through complementary dithered pixels. The far mesh takes over from 120 to 128 meters.
A zero `fade` switches immediately. Up to three replacements are supported. Distances must
increase, leaving more than `fade` meters between transitions and before `visible_to`.

`count` reports placed copies and `remove` removes the whole chain. Each representation uses
one drawing record per copy: this example uses 6,000 of the scene's 262,144-record budget.
An invalid chain or an exhausted budget adds nothing. Omitting `lods` keeps the original rendering.

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


## Water surfaces

A horizontal, upward-facing mesh becomes water when its material has the Blender custom
property `water = true`. Enable custom properties in the glTF export. The surface is drawn
after opaque geometry: it refracts that picture, absorbs light through the depth behind it,
and reflects on-screen objects with a sky and surroundings fallback. The material's base color is its deep-water
color and roughness controls its reflections. Its alpha is ignored; transparency comes from
water depth and absorption instead.

| Material custom property | Default | Meaning |
| --- | --- | --- |
| `water` | `false` | Use water shading. |
| `wave_height` | `0.08` | Maximum vertical displacement in world meters, from 0 to 10. |
| `wavelength` | `3` | Dominant wavelength in meters, from 0.05 to 1000. |
| `wave_speed` | `1` | Wave clock multiplier, from 0 (still) to 10. |
| `absorption_red` | `0.35` | Red light absorption per meter, from 0 to 10. |
| `absorption_green` | `0.08` | Green light absorption per meter, from 0 to 10. |
| `absorption_blue` | `0.035` | Blue light absorption per meter, from 0 to 10. |
| `foam_width` | `0.12` | Nominal local horizontal width of a shore foam band, in meters; 0 disables it. World-space patches vary its reach from 0.85 to 1.15 times this value. |
| `refraction` | `0.035` | Refraction strength, from 0 to 0.1; the default uses water's IOR of 1.333 and 0 keeps the bottom undistorted. |

The Blender helper `scene_kit.water_material("Pond", wave_height=0.04, wavelength=2.0)`
creates the material and its custom properties. Optional keyword arguments override the table
above. The material preview has water's IOR and transmission; runtime waves and depth-based
color are applied by the engine after export.

Use a subdivided mesh: vertex spacing at most one tenth of the smallest visible geometric
wavelength avoids angular silhouettes. Three traveling waves displace geometry; finer ripples
change normals. Smooth spatial phase variation curves the crests without changing the maximum height.
The pattern is in world coordinates, so adjacent patches share phase and mesh
scale does not change the wavelength. Wave displacement follows the renderer's clock, including
fixed-step recordings, and the temporal coverage pass uses the same displaced geometry.
Water does not cast an opaque shadow. Objects keep their authored collision shapes.

Dynamic and kinematic physics bodies automatically disturb horizontal water meshes when they
cross the surface or travel through it. Each contact sends a short, outward-moving wave packet;
its strength follows motion and its initial width follows the body's footprint. No script call
is needed. The visual effect displaces vertices and bends the lighting normal with the same
radial wave, so use a subdivided surface when the silhouette needs to move.

Impulses belong to one object and one original mesh primitive. Copies that share a mesh or
material, and other water primitives in that mesh, remain independent. The simulation keeps at
most 16 impulses for three seconds; drawing never advances their ages. Pausing simulation freezes
them, and recording uses the same simulation steps. They reset when the world changes. Temporal
motion uses the last snapshot actually drawn; a newly observed impulse starts with its current
age in both poses to avoid an invented first-frame velocity. These ripples are a visual response:
they do not apply buoyancy or forces to physics bodies and are not network-synchronized.

Contact detection uses the actual water triangles with a cached spatial tree. It supports rigid,
horizontal planar mesh primitives; skinned or tilted surfaces and Scatter copies do not generate
contacts. Body footprints use conservative world-aligned collider bounds, including spheres and
capsules. A small surface margin, emission cooldown and travel spacing suppress resting jitter.
Scripted position changes of dynamic bodies, characters and vehicles are treated as teleports;
scripted kinematic movement can create entry waves and wakes.

Work is bounded to the first 128 eligible water objects, 128 valid water primitives and
262,144 triangles per world, 256 moving bodies and 1,024 active body/surface pairs. Tilted or
oversized candidates still occupy a water-object slot. A primitive that exceeds the remaining
triangle budget is ignored as a whole. These bounds keep interaction work predictable for large scenes.

This first water material is for views above a lake or sea surface. Refraction samples only
opaque geometry visible on screen; transparent objects and other water layers are absent from
that snapshot. Screen-space reflections miss off-screen and hidden geometry and use the environment capture
when a ray misses. There is no planar reflection pass. Shore foam approximates the horizontal distance
to the intersection of the local water and visible-bottom tangent planes, without breaking-wave simulation.
Its world-space patches and softer pockets form an irregular band
rather than a uniform intersection line. Low-density pockets leave water uncovered; even dense
patches preserve some transmission instead of replacing the whole shallow surface with white.
Coverage fades in over a short contact distance and
fades out across the authored horizontal width; larger `foam_width` values broaden the band. The
pattern stays fixed in world space and does not drift with the render clock. Fine pores are filtered
by their projected footprint and converge to expected mean coverage when unresolved. Flat shallow
plateaus with no local shore intersection receive no foam. The local plane estimate uses the primary
opaque depth, not the refracted optical path. Occluded or off-screen bottom geometry, depth discontinuities,
and degenerate or almost parallel tangent planes produce no foam; they do not invent a hidden shore.
Curved terrain is only locally approximated, and rejected curvature can leave gaps. Nearly vertical
bottom walls are supported without dividing by the bottom normal's vertical component.

Traveling waves combine seven non-harmonic scales with distinct phases around one prevailing
direction. Slow spatial envelopes vary crest height; displacement and shading use the same
analytic gradient. Positive amplitude weights sum to one, and the envelopes never exceed one,
so `wave_height` remains the maximum displacement used by visibility bounds. Fragment normals
fade each traveling component when its phase becomes too fine for a pixel; discarded slope
variance adds bounded reflection roughness. Geometry and motion retain the full wave shape. This is a
procedural surface approximation, without wave interactions or a fluid spectrum simulation.
Refraction traces the visible depth field. When the refracted surface is hidden or outside the screen,
it keeps the undistorted opaque sample instead; this fallback cannot reveal geometry absent from the snapshot.
Depth discontinuities use a guarded, texel-matched color sample. When a ray misses, the visible
bottom's local tangent plane may estimate its absorption distance; this remains an approximation
for hidden or curved geometry.
Absorption filters the transmitted picture, but does not yet attenuate lighting on its way to submerged
objects; a lit bottom can therefore be brighter than in a full participating-medium render.
There is no underwater camera mode, buoyancy, fluid simulation, or wave collision. Use ordinary
scene meshes for water surfaces rather than Scatter copies.

The [optical comparison](water-validation.md) records a fixed-camera Cycles reference and
the remaining differences around refraction and incident lighting.
