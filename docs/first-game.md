---
description: "Create a platformer, edit its level, add a Ruby behaviour, and export a playable build."
---

# Your first game

You will create a game from a template, change it, write a behaviour in Ruby and export it.

## 1. Create the game

Open RedGem. In the project hub, choose **New game**, select **Platformer**, and name the project.
If you prefer the terminal, use the executable for your [installation](install.md). These
commands assume `redgem` is on your PATH.

```bash
redgem new my_game --template platformer
redgem my_game
```

The other templates start other kinds of game: `third-person` (a hero seen from behind),
`top-down` (a hero seen from above, that walks where you click), `menu-only` (title, settings and
credits screens, for a game of menus), `multiplayer-levels` (two levels played together, with an
NPC and a quest: see [A game of levels played together](multiplayer-levels.md)) and `empty`. All
but `empty` and `platformer` come with tests of their own, and every new game comes with a
workflow that runs its tests on GitHub (see [Testing your game](testing.md)).

The editor opens:

- **Scene** (center): the level. Hold the right mouse button and use WASD to fly, scroll to move
  closer, click an object to select it.
- **Hierarchy** (left): every object of the level.
- **Inspector** (right): what the selected object is made of.
- **Project** and **Console** (bottom): the game's files, and messages from the engine and from
  your scripts.

## 2. Play it

Press **Play** in the toolbar (or Ctrl+P). The game runs inside the Scene view: WASD moves, Space
jumps. Press Esc or **Stop** to go back to editing. The level returns exactly to how it was.

## 3. Change the level

1. Click a coin in the Scene view.
2. Drag the arrows of the gizmo to move it somewhere harder to reach.
3. Press Ctrl+D to duplicate it and move the copy too.
4. Press Ctrl+S to save.

Ctrl+Z undoes any of this.

## 4. Write a behaviour

Add a platform that moves up and down. The script sets its position every frame, and a
kinematic body lets the player stand on it.

1. In the menu, **GameObject > 3D Object > Cube**. Scale and place it as a platform.
2. In the Inspector, **Add Component > Script**, type `bob` and press **New**.
3. The script opens in a tab next to the Scene. Replace its contents with:

```ruby
# Moves the object up and down.
class Bob < Behaviour
  HEIGHT = 2.0   # meters
  SECONDS = 4.0  # per cycle

  def start
    @origin = transform.position
  end

  def update(_dt)
    wave = Math.sin(Time.elapsed / SECONDS * 2 * Math::PI)
    transform.position = @origin + Vector3.up * (wave * HEIGHT)
  end
end
```

4. Press Ctrl+S. The tab shows *Syntax OK*, or the line of the mistake.
5. Select the cube and choose **Add Component > Physics body**. Pick the kinematic body option.
   If the cube already has a **Physics** card, change its body type there.
6. Save the scene and press **Play** to try the platform.

If a script fails while the game runs, the game keeps going: that behaviour is switched off and
the error, with its line, appears in the Console.

## 5. Share it

Choose **Build** in the toolbar and select a target. For a desktop build, zip the whole output
folder or the macOS app bundle. Android builds produce an APK by default. See
[Building and sharing](shipping.md) for target requirements and terminal commands.

## Next

- [Behaviours](scripting/behaviours.md): how scripts are attached and what they can do.
- [Gameplay recipes](scripting/gameplay.md): input, movement, pickups, HUD, sound, levels.
