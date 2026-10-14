# Your first game

You will create a game from a template, change it, write a behaviour in Ruby and export it.

## 1. Create the game

Start RedGem with no arguments. In the project hub, pick the **Platformer** template, type a name
and create it. The same from a terminal:

```bash
./build/src/redgem new ~/RedGemGames/my_game --template platformer
./build/src/redgem ~/RedGemGames/my_game
```

With Docker, write `tools/docker/editor.sh` wherever this manual says `./build/src/redgem`.

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

Make the hero's coins worth collecting: a platform that moves up and down.

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
5. With the cube selected, find the **Physics** card in the Inspector and set *Body* to
   *kinematic*, so the hero can stand on it while it moves.
6. Press Play.

If a script fails while the game runs, the game keeps going: that behaviour is switched off and
the error, with its line, appears in the Console.

## 5. Share it

**Build** in the toolbar (or `redgem build <game> <folder>`) makes a folder with your game's
executable and its files, for Linux, Windows or (from a Mac) macOS. Zip it and send it. See [Building and sharing](shipping.md).

## Next

- [Behaviours](scripting/behaviours.md): how scripts are attached and what they can do.
- [Gameplay recipes](scripting/gameplay.md): input, movement, pickups, HUD, sound, levels.
