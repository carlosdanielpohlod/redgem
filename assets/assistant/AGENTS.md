# Working on this game

This is a game made with RedGem, a 3D engine whose games are written in Ruby. These notes are for
whoever works on it, coding assistants included. The engine's manual is at
https://carlosdanielpohlod.github.io/redgem-site/manual/ (`llms.txt` there indexes it).

## Layout

| Path | What it is |
|------|------------|
| `scene.glb` | the level the game starts in (binary glTF: read and change it with `redgem scene`) |
| `scenes/<name>.glb` | more levels, loaded with `Game.load_scene("<name>")` |
| `scripts/*.rb` | behaviours and scene scripts, one class per file |
| `tests/*_test.rb` | the game's tests |
| `sounds/`, `images/`, `models/` | sounds, pictures for the HUD and the sky, models to place |
| `locales/<language>.json` | the texts players read (`I18n.t`) |
| `game.json` | the game's id, title and window size |

## Commands

```bash
redgem scene list .                        # every object: "#5 Player pos=0,1.1,0 scripts=Player tag=player"
redgem scene get . Player                  # everything about one object (or #5, or Level/Player)
redgem scene set . Coin1 position=1,2,3 prop.speed=2.5 script+=Spinner
redgem scene add . Coin6 --from Coin1 position=4,1,0
redgem scene remove . Coin6
redgem check .                             # scripts compile, scenes read, attached scripts exist
redgem check . --run-tests                 # ... and the tests in tests/ pass
redgem run . --seconds 5                   # play with no window; prints each script error with file:line
redgem shot . shot.png --at 2              # a 512x288 picture of the game 2 seconds in
redgem api                                 # the scripting API: a line per class
redgem api Transform                       # one class's methods
redgem api --search raycast                # methods about a word
redgem . --play                            # play it in a window
```

Every command takes `--help`. `--scene <name>` works on `scenes/<name>.glb`. Ids (`#12`) change
when objects are added or removed: list again before using one. A scene changed with
`redgem scene` is saved exactly as the editor saves it; do not edit `.glb` files any other way.

Run `redgem check .` after every change, and `redgem run .` after changing a behaviour.

## Tests

Tests live in `tests/*_test.rb`: some play the game, with keys held and assertions; unit
tests check Ruby classes alone (the manual's "Testing your game" shows both). Write one for each
feature that must keep working. `redgem check . --run-tests` runs them all with no window and
says each failure with its first error; `redgem-test --help` runs them one by one.

## Conventions

- One behaviour per file: `scripts/camera_follow.rb` defines `class CameraFollow < Behaviour`.
  A class is loaded the first time it is named: never `require` a script.
- Attach behaviours with `redgem scene set . <object> script+=ClassName`, or from a scene script
  (`scripts/main_scene.rb`, `class MainScene < SceneScript`, `def setup`).
- Settings come from `prop(:name, default)`: the object's custom property, or what
  `add_component` was given. Prefer properties over constants for what a level designer tunes.
- Find objects by name (`find("Player")`) or tag (`find_all(:coin)`); `find!` fails loudly.
- Meters, seconds and degrees everywhere; Y is up; objects face -Z.
- An error in a behaviour switches that behaviour off and is logged with file and line: read
  `redgem run` output after a change.
- Prefabs are objects with `prefab = true`: hidden templates for `Scene.spawn`.

## The calls used most

{{most_used_calls}}

Everything else: `redgem api`, `redgem api <Class>`, `redgem api --search <word>`.
