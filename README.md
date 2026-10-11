<p align="center"><img src="branding/redgem-logo.svg" alt="RedGem 3D Engine" width="460"></p>

A 3D game engine and editor where games are written in **Ruby**, the most human-friendly
language. Build levels in the editor or in **Blender**, write the gameplay in Ruby, press Play,
ship a standalone game.

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

## Features

- **Ruby scripting**: behaviours with `start`, `update`, triggers and collisions; scenes set up in
  Ruby; classes load on first use, no `require`.
- **Editor**: scene view with gizmos, hierarchy, inspector, Ruby code tabs, play mode, undo,
  sculptable terrain, and one-click builds of the game for Linux and Windows.
- **Blender as the modeling tool**: scenes are glTF files; custom properties carry gameplay data.
- **Rendering**: PBR materials, sun shadows, reflections, ambient occlusion, bloom, fog,
  skeletal animation.
- **Physics**: rigid bodies, character controllers, vehicles, triggers, raycasts.
- **Tests**: games are tested by scripts that play them frame by frame.

## Built with

RedGem stands on open source. Thank you to the people behind these projects.

| Project | Used for | License |
|---------|----------|---------|
| [Blender](https://www.blender.org) | Modeling, level building and the scene exporter | GPL |
| [Jolt Physics](https://github.com/jrouwe/JoltPhysics) 5.3.0 | Rigid bodies, characters, vehicles, raycasts | MIT |
| [mruby](https://mruby.org) 3.4.0 | The Ruby interpreter that runs game scripts | MIT |
| [raylib](https://www.raylib.com) 5.5 | Window, input, OpenGL rendering, audio | zlib |
| [cgltf](https://github.com/jkuhlmann/cgltf) 1.15 | Loading glTF scenes | MIT |
| [Dear ImGui](https://github.com/ocornut/imgui) 1.92.8 (docking) | The editor's interface | MIT |
| [rlImGui](https://github.com/raylib-extras/rlImGui) | Dear ImGui on raylib | zlib |
| [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) | Move, rotate and scale gizmos | MIT |
| [nlohmann/json](https://github.com/nlohmann/json) 3.12.0 | Reading and writing glTF JSON and settings | MIT |
| [doctest](https://github.com/doctest/doctest) 2.4.11 | C++ tests | MIT |
| [Inter](https://rsms.me/inter/), [JetBrains Mono](https://www.jetbrains.com/lp/mono/), [Font Awesome Free](https://fontawesome.com) | Editor fonts and icons | OFL |

The engine itself is **C++17**, built with CMake. Demo assets come from
[Kenney](https://kenney.nl), [Poly Haven](https://polyhaven.com) and [ambientCG](https://ambientcg.com)
(all CC0), and [BlenderKit](https://www.blenderkit.com) (see [showcase/CREDITS.md](showcase/CREDITS.md)).

## Quick start

With Docker, on Linux:

```bash
tools/docker/editor.sh                  # builds the image the first time, then opens the editor
tools/docker/editor.sh games/platformer # open a demo
```

From source: a C++17 compiler, CMake 3.24 or newer, Ninja, Ruby with rake and the X11 and OpenGL
development files.

```bash
cmake -S . -B build -G Ninja
cmake --build build
./build/src/redgem                      # project hub: open a demo or create a game
ctest --test-dir build                  # run the tests
```

## Making a game

```bash
./build/src/redgem new games/my_game --template platformer   # or --template empty
./build/src/redgem games/my_game                             # edit and play it
./build/src/redgem build games/my_game dist/my_game          # standalone game: dist/my_game/my_game
./build/src/redgem build games/my_game dist/win --target windows   # a Windows .exe (see docs/shipping.md)
```

A game is a folder: `scene.glb` (the scene, from Blender or the editor), `scripts/*.rb`,
`models/`, `sounds/` and `game.json`.

## Demos

Small games and scenes that ship with the engine, each with its scripts and tests:

- `games/platformer`: run, jump, push crates, ride platforms, collect the coins, reach the flag.
- `games/driving`: a time trial with wheel physics, checkpoints and lap times.
- `games/city`: a crossroad where cars obey traffic lights and brake for each other.
- `games/showcase`: a detailed street at dusk to drive through (built from downloaded assets,
  see [docs/contributing.md](docs/contributing.md)).

## Documentation

The manual is in [docs/](docs/index.md): [install](docs/install.md), [your first game](docs/first-game.md),
[the editor](docs/editor.md), [scripting](docs/scripting/behaviours.md) and the
[API reference](docs/scripting/api.md). To read it as a site:

```bash
pip install -r docs/requirements.txt
mkdocs serve
```
