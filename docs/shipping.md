# Building and sharing

**Build** in the editor's toolbar, or from a terminal:

```bash
./build/src/redgem build my_game dist/my_game
```

This makes a folder with an executable named after your game (`dist/my_game/my_game`) and a
`game/` folder with its files. Whoever receives the folder runs the executable: they do not need
RedGem, Ruby or Blender installed.

## For Windows

RedGem runs on Linux, and builds games for Linux and for Windows. In the editor, **Build** asks
which one; from a terminal:

```bash
tools/windows/build_runtime.sh                                   # once: builds the engine for Windows
./build/src/redgem build my_game dist/my_game_win --target windows
```

The first command downloads a Windows compiler into the project (no administrator rights
needed) and builds `redgem.exe`; it takes a few minutes and is only needed once, or again after
updating RedGem. The build then makes `dist/my_game_win/my_game.exe` next to its `game/` folder.
It needs 64-bit Windows 10 or later and a graphics card with OpenGL 3.3; nothing has to be
installed with it.

- The executable needs the `game/` folder next to it.
- `game.json` sets the window title and size:

```json
{ "title": "My Game", "window_width": 1280, "window_height": 720 }
```

- Everything in the game folder is copied, including the scripts as plain `.rb` files.
- Check the licenses of the models and sounds you include. A license that allows using an asset
  in a game does not always allow shipping its original file.
