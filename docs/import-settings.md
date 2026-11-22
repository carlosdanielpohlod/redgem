# Import settings

Each model, texture and sound of a game can say how the engine takes it in. The settings live
next to the file, in a sidecar named after it:

```text
models/oak.glb
models/oak.glb.import.json
```

A file without a sidecar uses the defaults. A sidecar may hold only the fields you change; the
others keep their defaults:

```json
{
  "scale": 0.01,
  "up_axis": "z",
  "colliders": "mesh"
}
```

Import settings are experimental: the fields may change in any release.

## Models

Models in `models/` (`.glb`, `.gltf`). The editor applies these when it imports the model into a
scene.

| Field | Default | What it does |
|-------|---------|--------------|
| `scale` | `1.0` | Scales the model, from `0.001` to `1000`. A model made in centimeters takes `0.01`. |
| `up_axis` | `"y"` | `"z"` turns a model made with Z up so that it stands up. |
| `colliders` | `"none"` | `"box"` or `"mesh"` gives each of the model's meshes a static collider of that shape. |
| `materials` | `"keep"` | `"extract"` also writes the model's pictures into `images/<model>/`, so you can edit them or put them on other materials. Files already there are kept. |
| `lod_levels` | `0` | How many levels of detail to make for each mesh, from `0` (none) to `4`. See below. |
| `lod_ratio` | `0.5` | The share of triangles each level keeps of the one before, from `0.05` to `0.95`. |
| `lod_screen_size` | `0.25` | The first level is drawn once the mesh covers less than this share of the view's height, from `0.01` to `1`. Each next level starts at half the size of the one before. |
| `lod_fade` | `0.1` | How wide the cross-fade between two levels is, from `0` (a hard switch) to `1`. It is a share of the distance at which the mesh fills the view's height. |

The scale and the up axis go on the model's own top-level objects. The object the model sits
under keeps the place, turn and size you give it.

The editor remembers which file each imported model came from (the `model_file` property of its
object). When you apply new settings in the editor, it imports the model again wherever the open
scene holds it. Each of those objects keeps its name, its placement, its properties and its
scripts. What it held is replaced by the new import.

### Levels of detail

With `lod_levels` above `0`, the editor makes lighter copies of each of the model's meshes when it
imports it. Each copy keeps about `lod_ratio` of the triangles of the one before and uses the
same vertices. It becomes a child of the mesh's object, named `<mesh> LOD 1`, `<mesh> LOD 2` and
so on. The editor log lists the triangles of each level, and so does the mesh card of the
Inspector.

```json
{
  "lod_levels": 3,
  "lod_ratio": 0.5,
  "lod_screen_size": 0.3
}
```

The game draws one level at a time, chosen by how big the mesh looks: the share of the view's
height its bounding sphere covers. A wider field of view or a bigger object moves the switches
farther away. Near a switch, the two levels cross-fade with a dither: each pixel comes from one
level or the other, so nothing is drawn twice.

Each level carries the properties that decide when it is drawn: `screen_size_from`,
`screen_size_to`, `lod_radius` and `lod_fade` (see the scene properties). Its own
`visible_from` and `visible_to` still apply on top of them.

Meshes moved by a skeleton or by shape keys get no levels. A level that would not be lighter
than the one before is not made. Colliders stay on the full mesh only.

## Textures

Pictures in `images/` (`.png`, `.jpg`, `.jpeg`, `.webp`, `.ktx2`).

| Field | Default | What it does |
|-------|---------|--------------|
| `color_space` | `"auto"` | `"srgb"` for colors, `"linear"` for data such as roughness. With `"auto"` the material slot decides: base color and emission read sRGB, the others linear. |
| `normal_map` | `false` | Marks a normal map. |
| `max_size` | `0` | The longest side, up to `16384`; `0` keeps the picture's own size. A bigger picture is scaled down, in proportion. |
| `mipmaps` | `false` | Smaller copies of a picture drawn on screen (`UI.image`, particles), so it stays smooth when drawn smaller than it is. Textures of a scene always have them. |
| `compression` | `"none"` | Only `"none"` today. A `.ktx2` file is used as it is. |

The editor warns when you put a picture in a material slot its settings do not suit: a normal
map outside the normal slot, or a picture whose color space is not the one the slot reads.

`max_size` applies when the picture is put on a material (the scene then carries the smaller
copy, as a PNG) and when the game loads it by name.

## Sounds

Files in `sounds/` (`.wav`, `.ogg`, `.mp3`, `.flac`).

| Field | Default | What it does |
|-------|---------|--------------|
| `stream` | `false` | Decodes the sound as it plays instead of loading it whole: for long sounds. |
| `loop` | `false` | Every play of the sound loops. A play that asks for a loop loops anyway. A loop always streams. |
| `volume` | `1.0` | The sound's own volume, from `0` to `1`, times the volume each play gives it. It applies to music too. |
| `mono_3d` | `true` | Mixes a positional sound down to mono before it is placed, so its own channels do not pull it aside. `false` keeps its stereo. |

## In the editor

Click a model, a picture or a sound in the Project panel. With no object selected, the Inspector
shows the file's import settings. Change them, then press **Apply** to write the sidecar, or
**Revert** to read it again. Applying a model's settings imports it again in the open scene, one
undo step for each object that holds it. The game must be stopped.

## From the command line

```console
$ redgem assets games/my_game --settings models/oak.glb
model models/oak.glb
  colliders = none
  materials = keep
  scale = 1.0
  up_axis = y
$ redgem assets games/my_game --settings models/oak.glb scale=0.01 colliders=mesh
model models/oak.glb
  colliders = mesh
  materials = keep
  scale = 0.01
  up_axis = y
wrote models/oak.glb.import.json
```

`--settings <asset>` lists the file's settings. With `field=value` words, it sets those fields and
writes the sidecar. A value out of range, an unknown field or a broken sidecar is refused, and the
sidecar is left as it was. The command line does not import the model again in a scene: apply the
settings in the editor for that.
