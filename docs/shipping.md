# Building and sharing

**Build** in the editor's toolbar, or from a terminal:

```bash
./build/src/redgem build my_game dist/my_game
```

This makes a folder with the engine, your game and a `run.sh`. Whoever receives it runs
`./run.sh`: they do not need RedGem, Ruby or Blender installed.

- The build runs on Linux, like the engine.
- `game.json` sets the window title and size:

```json
{ "title": "My Game", "window_width": 1280, "window_height": 720 }
```

- Everything in the game folder is copied, including the scripts as plain `.rb` files.
- Check the licenses of the models and sounds you include. A license that allows using an asset
  in a game does not always allow shipping its original file.
