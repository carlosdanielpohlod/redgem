---
description: "redgem scene, run, shot and check: read and change scenes as text, play with no window, take pictures, check a game."
---

# The command line

Besides opening the editor (`redgem my_game`), playing (`redgem my_game --play`), making a game
(`redgem new`) and building one (`redgem build`), `redgem` has tools that answer in a few lines
of text and end: they read and change a scene, play the game with no window, take a picture of
it, look it over, and say what the scripting API has. They are made for scripts, terminals and coding assistants: what they say
is short, and every problem they report says where it is and what to do instead.

Each one says more with `--help`. They end with exit code 0 when everything went well, 1 when
something is wrong with the game, and 2 when the command line was not understood.

## `redgem scene`: read and change a scene

A `scene.glb` is a binary file. `redgem scene` shows it as text, one line per object, and changes
it through the same code the editor saves with, so a scene changed here is saved exactly as the
editor would save it.

```console
$ redgem scene list games/platformer
scene main: 27 objects (#id name, then what differs from the defaults)
#0 Ground pos=0,-0.5,-4 scale=14,1,14 collider=box
#4 MovingPlatform pos=0,1.5,-25 scale=3,0.5,3 scripts=Mover axis=x body=kinematic distance=2.5 speed=1.2
#5 Player pos=0,1.1,0 scripts=Player body=character jump_speed=9 speed=6 tag=player
  #6 Visor pos=0,0.45,-0.35 scale=0.5,0.2,0.12
#7 Coin1 pos=0,1,-4 scripts=Coin tag=coin trigger=true
#24 Camera pos=0,6,9 rot=-28.6,0,0 camera scripts=CameraFollow distance=9 height=5 target=Player
#25 Sun pos=5,12,5 rot=-44.8,24.4,0 light=sun
...
```

A line says only what differs from an object's defaults: its position (`pos`, meters), rotation
(`rot`, degrees about X, Y and Z), scale when it is not 1, the mesh it shows when it is named
otherwise than the object, its light or camera, the classes of its scripts, and its custom
properties. Children are indented under their parent. `--depth 0` lists the top objects alone;
`--limit N` stops after N lines (200 unless said; `--limit 0` lists all).

**Naming an object.** `#12` is a persistent object identity, independent of its place in the
scene file. A path (`Player/Visor`) or a name works too when only one object has it; a name several
objects share is refused, with each of them listed by id and path. Identities survive saves,
renames and reparenting. New objects get hexadecimal identities: take them from `scene list` or
`scene add` and pass them back unchanged.

| Command | What it does |
|---------|--------------|
| `redgem scene list <game>` | every object, a line each |
| `redgem scene get <game> <object>` | everything about one object: its path, parent, placement, mesh and materials, light, camera, scripts, properties and children |
| `redgem scene set <game> <object> key=value...` | changes the object and saves the scene |
| `redgem scene add <game> <name> [--from <object>] [--parent <object>] [key=value...]` | adds an empty object, or a copy of another (with its children), and saves |
| `redgem scene remove <game> <object>` | removes the object and its children, and saves |

Every command takes `--scene <name>` for another scene than `scene.glb`: `--scene level2` is
`scenes/level2.glb`.

What `set` (and `add`) take:

| Word | Effect |
|------|--------|
| `position=x,y,z` (or `pos=`) | where it is, in its parent's space |
| `rotation=x,y,z` (or `rot=`) | its rotation, in degrees |
| `scale=2` or `scale=1,2,1` | its size |
| `name=Gate` | a new name |
| `parent=Level` | moves it under another object, keeping where it is in the world; `parent=` moves it to the top |
| `prop.speed=2.5` | a custom property: `true`/`false`, a number, or a text (`prop.label="Big coin"`); `prop.speed=` removes it. A text that reads as a number is given in quotes that reach redgem: `prop.code='"12"'` (a list shows it as `code="12"`) |
| `script+=Door`, `script-=Door`, `script=Door,Sound` | attaches a behaviour, takes one off, or sets the list (`Door` is `scripts/door.rb`) |

```console
$ redgem scene set games/my_game Coin1 position=1,2,3 script+=Spinner
#7 Coin1 pos=1,2,3 scripts=Coin,Spinner tag=coin trigger=true
note: scripts/spinner.rb does not exist yet; it must define class Spinner
$ redgem scene add games/my_game Coin6 --from Coin5 position=3,3,-31
#27 Coin6 pos=3,3,-31 mesh=Coin5 scripts=Coin tag=coin trigger=true
```

A copy made with `--from` of a `prefab = true` template is an object of the scene, not another
template. Names are made unique as the editor makes them (`Wall`, then `Wall.001`), by `add` and
by `name=` alike; a name that starts with `#` or holds a `/` is refused, since it would read as an
id or a path.

The words of a `list` line are what `set` takes back: `pos=`, `rot=`, `scripts=Coin,Spin` (the
list of scripts), and a property as `speed=6` as well as `prop.speed=6`. A key that is a typo of
one of `set`'s (`positon=`) is refused rather than made a property; `prop.` makes any key a
property. `mesh=`, `light=` and `camera` say what an object is: they are taken back as they are,
and not changed.

## `redgem run`: play with no window

```console
$ redgem run games/my_game --seconds 3
error: scripts/coin.rb:12: Coin#update on 'Coin1' (behaviour disabled): undefined method 'spin' (NoMethodError)
ran main for 180 frames (3 s): 1 errors, 0 warnings
```

It plays the game with no window and no sound for a while (5 seconds unless `--seconds N` or
`--frames N` says otherwise), each frame one fixed step of 1/60 s with nothing pressed, so the same
run gives the same answer every time. It says each warning and error once (with how many times it
came), led by the place in the game's scripts it points at, then one line of summary, and ends
with an error when a script failed. `--scene level2` starts on another scene. What the game saves
goes to a folder of the run's own, deleted afterwards: a run never touches your progress.

## `redgem shot`: a picture of a moment

```console
$ redgem shot games/my_game shot.png --at 2
saved shot.png (512x288, scene main at 2 s)
```

It plays the game to that moment (1 second unless `--at` says otherwise; `--frames N` counts fixed
steps instead; at most an hour of the game, as for `run`) the way `run` does, draws
every frame off screen, and saves the picture of the one asked for. Small unless asked otherwise:
`--size 1280x720` for more. `--camera Name` shows the game through the camera of that object
instead of the scene's first camera; `--scene` starts on another scene. It needs a graphics card:
it opens a hidden window to draw with. The same moment gives the same picture.

## `redgem check`: look a game over

```console
$ redgem check games/my_game
error scripts/coin.rb:5: syntax error, unexpected end of file, expecting "'end'" (SyntaxError)
error scene main #20 Goal: scripts/goal_post.rb is not there; did you mean goal.rb?
error scripts/door.rb does not define class Door (attached in scene main #8 Door)
check: 3 errors, 0 warnings (12 scripts, 2 scenes)
```

Without playing the game it checks that every script compiles, every scene reads (with what the
loader warns of), every script a scene attaches is there and defines the class its name says, and
every `sky_image` is in `images/`. It lists a screenful of problems (`--all` lists every one).

`--run-tests` then runs the game's own tests (`tests/*_test.rb`, see
[Testing your game](testing.md)) with `redgem-test`, system tests with no window, and adds a line
per failure with the first error it said:

```console
$ redgem check games/my_game --run-tests
FAIL tests/score_test.rb: FAIL ScoreTest#test_a_coin_is_worth_ten: RedGemTest::Failure: expected 10, got 1
tests: 3 passed, 1 failed
check: 0 errors, 0 warnings (12 scripts, 2 scenes)
```

## `redgem api`: the scripting API

```console
$ redgem api Transform
Transform: Where an object is, how it is turned and how big it is.
  position                        its position in the world (Vector3)
  position = vector               moves it; a physics object is teleported
  rotate(degrees)                 turns it by those Euler degrees
  ...
```

`redgem api` alone lists the classes, a line each; `redgem api <Class>` the methods of one;
`redgem api --search <word>` the methods that have to do with a word (at most 20). `--json` says
the same as data, for tools. It is the [API reference](scripting/api.md), which `--markdown`
writes, and which is made from `docs/scripting/api.json`.

Errors that `run` and `shot` report come with a hint when they are common ones: a mistyped method
of one of the engine's classes gets the names near it, a class no script defines the file it is
looked for in, a scene the game lacks the scenes it has.
