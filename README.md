<p align="center"><img src="branding/redgem-logo.svg" alt="RedGem 3D Engine" width="460"></p>

<p align="center">
  <a href="https://github.com/carlosdanielpohlod/redgem/actions/workflows/linux.yml"><img src="https://github.com/carlosdanielpohlod/redgem/actions/workflows/linux.yml/badge.svg?branch=main" alt="Linux"></a>
  <a href="https://github.com/carlosdanielpohlod/redgem/actions/workflows/windows.yml"><img src="https://github.com/carlosdanielpohlod/redgem/actions/workflows/windows.yml/badge.svg?branch=main" alt="Windows"></a>
  <a href="https://github.com/carlosdanielpohlod/redgem/actions/workflows/macos.yml"><img src="https://github.com/carlosdanielpohlod/redgem/actions/workflows/macos.yml/badge.svg?branch=main" alt="macOS"></a>
</p>

A 3D game engine and editor where games are written in **Ruby**, the most human-friendly
language. Build levels in the editor or in **Blender**, write the gameplay in Ruby, press Play,
ship a standalone game.

## New in 0.5.0: an AI agent inside the editor

The **Agent panel** brings a coding agent into the editor: Claude Code and Codex come as presets
(over the Agent Client Protocol), or add your own. Ask it to change the game, and watch the
answer stream in with its tool calls, diffs, plan and permission questions. The agent works in
the game's folder and gets the editor's live MCP tools: it lists and moves objects, presses Play,
reads errors and looks at the viewport. Each game keeps its past conversations. See
[the Agent panel](docs/assistants.md#the-agent-panel).

The same MCP tools work from any MCP client, with or without an open editor:

```sh
redgem /path/to/game --editor-bridge
# Uses the open editor when there is one, the files otherwise:
redgem mcp /path/to/game --editor auto
# File inspection without an editor:
redgem mcp /path/to/game --read-only
```

Also new: Ruby colors in the code tabs, builds in the background with their progress, and camera
helpers (`Camera.follow`, `orbit`, `shake`, `world_to_screen`). See [MCP tools and
permissions](docs/mcp.md) and the [changelog](CHANGELOG.md).

> **Experimental.** RedGem is a young engine: expect rough edges, and see the
> [known limitations](KNOWN_LIMITATIONS.md). It is not written from zero: it puts together
> open source projects such as Blender, Jolt Physics, raylib and mruby ([Built with](#built-with)).

<p align="center">
  <a href="https://carlosdanielpohlod.github.io/redgem-site/#demo"><img src="https://carlosdanielpohlod.github.io/redgem-site/assets/poster-0.3.jpg" alt="Watch the RedGem demo video" width="720"></a><br>
  <a href="https://carlosdanielpohlod.github.io/redgem-site/#demo">Watch the demo</a> ·
  <a href="https://carlosdanielpohlod.github.io/redgem-site/">Site</a> ·
  <a href="https://carlosdanielpohlod.github.io/redgem-site/manual/">Manual</a>
</p>

<details>
<summary>Assets in the demo video</summary>

Models from Sketchfab under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/):
[Samurai](https://sketchfab.com/3d-models/150603038ee6476a9a4b173c682ff8d5) by fletcherkinnear ·
[Viking](https://sketchfab.com/3d-models/79321179bafb4d558f0cd6a4a014ba9a) by Andy Woodhead ·
[Leopard 2 A6](https://sketchfab.com/3d-models/7cb23d5322df4b409a880de635826067) by buh ·
[Katana](https://sketchfab.com/3d-models/bee6931856084cd4886b2901226bca6a) by Joan LP ·
[Viking battle axe](https://sketchfab.com/3d-models/b7123192adbf40f68043c57dd8ddcd15) by Mikhail Antonov ·
[Coniferous Forest Assets Pack](https://sketchfab.com/3d-models/240ce9cf0ad3474f809a59f9446e63cd) by Dari ·
[Grass claster](https://sketchfab.com/3d-models/832eb6c9c5b24790b1ca24ad7dfdcdba) by VRA ·
[Residences of Rothenburg ob der Tauber](https://sketchfab.com/3d-models/af81933914b14dcbbb39bd8a0bbafaaf) by shotkey ·
[BMW E30](https://sketchfab.com/3d-models/9dea494b447e442fafbddfc7eccbf158) and
[Cadillac 75](https://sketchfab.com/3d-models/2935f8faa5f64ce4a3bd65af0f925c21) by ROH3D ·
[Linden tree](https://sketchfab.com/3d-models/c0c3aa73cc9945d2a36c01a0d8d6d4bb) by Georgeous ·
[Vintage lamp post](https://sketchfab.com/3d-models/56f6dcb3865144cd84e049ca6a736fae) by dasmodal ·
[Street Bench](https://sketchfab.com/3d-models/1ddafec2fda5430697aaf403b3deba96) by donnichols.

Characters' animations from [Mixamo](https://www.mixamo.com); sky, plants and textures from
[Poly Haven](https://polyhaven.com) (CC0); particles from
[Kenney](https://kenney.nl/assets/particle-pack) (CC0). City: [Poly Haven](https://polyhaven.com) and
[ambientCG](https://ambientcg.com) (CC0), car and people from [BlenderKit](https://www.blenderkit.com).

</details>

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

- **Ruby scripting**: behaviours with `start`, `update`, triggers, collisions, timers and tweens;
  scenes set up in Ruby; classes load on first use, no `require`.
- **Gameplay kit**: keyboard, mouse and gamepad input, menus and HUD, sounds and music, particles,
  per-character animation with blending, saved progress.
- **Multiplayer**: games found by themselves on the local network, rooms by code over the
  internet through a relay that ships with the engine, named messages, objects kept in sync.
- **Editor**: scene view with gizmos, hierarchy, inspector, Ruby code tabs with syntax colors,
  play mode, undo, sculptable terrain, and one-click builds of the game for Linux, Windows and
  Android (and, on a Mac, for the Mac), made in the background.
- **AI agents**: the Agent panel chats with Claude Code or Codex inside the editor, and
  `redgem mcp` gives any MCP client the scene, headless runs, errors and the live editor.
- **Blender as the modeling tool**: scenes are glTF files; custom properties carry gameplay data.
- **Rendering**: PBR materials, sun shadows, reflections, ambient occlusion, bloom, fog,
  skeletal animation.
- **Physics**: rigid bodies, character controllers, vehicles, triggers, raycasts.
- **Tests**: games are tested by scripts that play them frame by frame, run by a tool of their
  own (`redgem-test`): the game your players get has no test code in it, and no editor either.

## Built with

RedGem stands on open source. Thank you to the people behind these projects.

| Project | Used for | License |
|---------|----------|---------|
| [Blender](https://www.blender.org) | Modeling, level building and the scene exporter | GPL |
| [Jolt Physics](https://github.com/jrouwe/JoltPhysics) 5.3.0 | Rigid bodies, characters, vehicles, raycasts | MIT |
| [mruby](https://mruby.org) 3.4.0 | The Ruby interpreter that runs game scripts | MIT |
| [raylib](https://www.raylib.com) 5.5 | Window, input, OpenGL rendering, audio | zlib |
| [Android NDK](https://developer.android.com/ndk) | Compiling the engine for Android games | Apache 2.0 |
| [cgltf](https://github.com/jkuhlmann/cgltf) 1.15 | Loading glTF scenes | MIT |
| [Dear ImGui](https://github.com/ocornut/imgui) 1.92.8 (docking) | The editor's interface | MIT |
| [rlImGui](https://github.com/raylib-extras/rlImGui) | Dear ImGui on raylib | zlib |
| [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) | Move, rotate and scale gizmos | MIT |
| [nlohmann/json](https://github.com/nlohmann/json) 3.12.0 | Reading and writing glTF JSON and settings; multiplayer messages (MessagePack) | MIT |
| [Mbed TLS](https://www.trustedfirmware.org/projects/mbed-tls/) 3.6.7 | HTTPS for the games' web requests | Apache 2.0 |
| [SQLite](https://www.sqlite.org) 3.50.4 | Where a dedicated server keeps its game's saves | Public domain |
| [doctest](https://github.com/doctest/doctest) 2.4.11 | C++ tests | MIT |
| [Inter](https://rsms.me/inter/), [JetBrains Mono](https://www.jetbrains.com/lp/mono/), [Font Awesome Free](https://fontawesome.com) | Editor fonts and icons | OFL |

The engine itself is **C++17**, built with CMake. Demo assets come from
[Kenney](https://kenney.nl), [Poly Haven](https://polyhaven.com) and [ambientCG](https://ambientcg.com)
(all CC0), and [BlenderKit](https://www.blenderkit.com) (see [showcase/CREDITS.md](showcase/CREDITS.md)).

## Quick start

**Download and run**, nothing to install:

| System | Download | Then |
|--------|----------|------|
| Linux (x86_64) | [redgem-linux-x86_64.tar.gz](https://github.com/carlosdanielpohlod/redgem/releases/latest/download/redgem-linux-x86_64.tar.gz) | unpack, run `bin/redgem` |
| Windows (x86_64) | [redgem-windows-x86_64.zip](https://github.com/carlosdanielpohlod/redgem/releases/latest/download/redgem-windows-x86_64.zip) | unpack, run `redgem.exe` |
| macOS (Apple Silicon) | [redgem-macos-arm64.zip](https://github.com/carlosdanielpohlod/redgem/releases/latest/download/redgem-macos-arm64.zip) | unpack, open `RedGem.app` (unsigned: see the README inside) |
| macOS (Intel) | [redgem-macos-x86_64.zip](https://github.com/carlosdanielpohlod/redgem/releases/latest/download/redgem-macos-x86_64.zip) | unpack, open `RedGem.app` (unsigned: see the README inside) |

The project hub opens: pick *Platformer* under *New game* and press Play. The Linux download is
the tested one and builds games for Linux, Windows and Android; the Windows one has only been
tried under Wine so far, and the macOS ones only on GitHub's virtual Macs (they build games for the
Mac). Details in [docs/install.md](docs/install.md).

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
./build/src/redgem --version            # which version this is
./build/src/redgem-test --help          # the tool that runs a game's tests
./build/src/redgem-relay                # the meeting point for multiplayer games over the internet
./build/src/redgem-server games/platformer   # a game on a dedicated server, for players to join
ctest --test-dir build                  # run the tests
```

## Making a game

```bash
./build/src/redgem new games/my_game --template platformer   # or --template empty
./build/src/redgem games/my_game                             # edit and play it
./build/src/redgem build games/my_game dist/my_game          # standalone game: dist/my_game/my_game
./build/src/redgem build games/my_game dist/win --target windows   # a Windows .exe (see docs/shipping.md)
./build/src/redgem build games/my_game dist/apk --target android   # an Android .apk
```

A built game is a copy of `redgem-player`, the small program that plays a game and nothing else,
next to the game's files: the editor and the other tools stay with you.

A game is a folder: `scene.glb` (the scene, from Blender or the editor), `scripts/*.rb`,
`models/`, `sounds/` and `game.json`.

## Demos

Small games and scenes that ship with the engine, each with its scripts and tests:

- `games/platformer`: run, jump, push crates, ride platforms, collect the coins, reach the flag.
- `games/driving`: a time trial with wheel physics, checkpoints and lap times.
- `games/city`: a crossroad where cars obey traffic lights and brake for each other.
- `games/showcase`: a detailed street at dusk to drive through (built from downloaded assets,
  see [docs/contributing.md](docs/contributing.md)).

## Contributing

Small pull requests are welcome: [CONTRIBUTING.md](CONTRIBUTING.md) says how, and
[KNOWN_LIMITATIONS.md](KNOWN_LIMITATIONS.md) lists what is missing or wrong today.
[CHANGELOG.md](CHANGELOG.md) says what changed in each release.

## License

RedGem is [MIT licensed](LICENSE). The projects it is built on keep their own licenses, listed
in [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md); every built game carries them too.

## Documentation

The manual is in [docs/](docs/index.md): [install](docs/install.md), [your first game](docs/first-game.md),
[the editor](docs/editor.md), [scripting](docs/scripting/behaviours.md), the
[API reference](docs/scripting/api.md), [the command line](docs/command-line.md) and
[making games with an AI assistant](docs/assistants.md). To read it as a site:

```bash
pip install -r docs/requirements.txt
mkdocs serve
```
