---
description: "What RedGem is, where to start, and how a game's folder is organized."
---

# RedGem 3D Engine

Build levels in the RedGem editor or import them from Blender. Write gameplay in **Ruby**, test
it in Play mode, and export a standalone game.

## Where to start

1. [Install](install.md) RedGem and open the project hub.
2. Follow [Your first game](first-game.md) to edit a template, write a Ruby behaviour, and
   export a playable build.
3. Use [Behaviours](scripting/behaviours.md) and the [API reference](scripting/api.md) while
   writing gameplay.
4. Use the [command line](command-line.md) to inspect scenes, run checks, or play a game
   without opening the editor.

## How a game is organized

A game is a folder:

| Path | What it is |
|------|------------|
| `scene.glb` | The main level: objects, meshes, lights, cameras |
| `scenes/*.glb` | More levels |
| `scripts/*.rb` | Ruby behaviours and scene scripts |
| `models/*.glb` | Models you can drop into a level |
| `prefabs/*.glb` | [Reusable objects](prefabs.md) you can place in the editor or spawn from Ruby |
| `sounds/` | `.wav`, `.ogg`, `.mp3` or `.flac` files |
| `images/` | Pictures for menus and the HUD (`.png`, `.jpg`) |
| `locales/` | The texts players read, one translation file per language: `en.json`, `pt.json`... (see [Texts in several languages](scripting/i18n.md)) |
| `game.json` | The game's id, studio, title and window size |

Keep the project folder together when moving the game or adding it to version control. The
editor saves scene edits directly to the project's `.glb` files.
