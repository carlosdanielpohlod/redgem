# RedGem 3D Engine

RedGem is a 3D game engine with its own editor. You build levels in the editor (or in Blender),
write the gameplay in **Ruby**, press Play, and export a standalone game.

This manual is for people making games with it.

## Where to start

1. [Install](install.md) the editor.
2. Follow [Your first game](first-game.md): about fifteen minutes from an empty folder to a
   playable game.
3. Keep [Behaviours](scripting/behaviours.md) and the [API reference](scripting/api.md) open while
   you write scripts.
4. [The command line](command-line.md) reads and changes scenes as text, plays a game with no
   window, takes pictures of it and looks it over: handy in scripts and with coding assistants.

## How a game is organized

A game is a folder:

| Path | What it is |
|------|------------|
| `scene.glb` | The main level: objects, meshes, lights, cameras |
| `scenes/*.glb` | More levels |
| `scripts/*.rb` | Ruby behaviours and scene scripts |
| `models/*.glb` | Models you can drop into a level |
| `sounds/` | `.wav`, `.ogg`, `.mp3` or `.flac` files |
| `images/` | Pictures for menus and the HUD (`.png`, `.jpg`) |
| `locales/` | The texts players read, one translation file per language: `en.json`, `pt.json`... (see [Texts in several languages](scripting/i18n.md)) |
| `game.json` | The game's id, title and window size |

The editor reads and writes these files directly. There is no project database and no import
step: what you see in the folder is the game.
