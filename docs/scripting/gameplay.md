# Gameplay recipes

Short answers to "how do I...". Each snippet goes inside a behaviour.

## Read the keyboard

```ruby
Input.axis(:horizontal)        # -1..1: A/D or left/right arrows
Input.axis(:vertical)          # -1..1: S/W or down/up arrows
Input.action_pressed?(:jump)   # true on the frame Space goes down
Input.action?(:run)            # true while Left Shift is held
Input.down?(:e)                # any key: held
Input.pressed?(:e)             # any key: went down this frame
```

Built-in actions: `:jump` (Space), `:fire` (Left Ctrl or Enter), `:run` (Left Shift).

## Read the mouse

```ruby
Input.mouse_x, Input.mouse_y        # pixels from the top-left corner
Input.mouse_dx, Input.mouse_dy      # how far it moved since the last frame
Input.mouse_down?(:left)            # :left, :right or :middle, held
Input.mouse_pressed?(:left)         # went down this frame
Input.mouse_released?(:left)        # went up this frame
Input.mouse_wheel                   # notches turned this frame
```

To turn a first-person camera with the mouse, hide the pointer and keep it in the window, then
use how far it moved:

```ruby
def start = Input.mouse_locked = true

def update(_dt)
  transform.rotate(Vector3.new(0, -Input.mouse_dx * 0.1, 0))
end
```

To click on things in the world, cast a ray from the camera through the pointer:

```ruby
if Input.mouse_pressed?(:left)
  origin, direction = Camera.screen_ray(Input.mouse_x, Input.mouse_y)
  hit = Physics.raycast(origin, direction, 100.0)
  hit[:object].destroy if hit
end
```

## Move a character

Give the object a **character** body: set *Body* in the Inspector's **Physics** card (objects made
in the editor already have one), or use *Add Component > Physics body* on an object without
physics. A character walks on floors, climbs slopes and is stopped by walls.

```ruby
def update(_dt)
  direction = Vector3.new(Input.axis(:horizontal), 0, -Input.axis(:vertical)).normalized
  character.move(direction * 6.0)                                    # meters per second
  character.jump(9.0) if Input.action_pressed?(:jump) && character.grounded?
end
```

## Push things around

A **dynamic** body falls, rolls and gets pushed. From a script:

```ruby
rigidbody.add_impulse(Vector3.up * 5.0)
rigidbody.velocity = Vector3.new(0, 0, -10)
```

A **kinematic** body is moved by your script (set `transform.position`) and carries what stands
on it: platforms, lifts, doors.

## Drive a car

Give the chassis a **vehicle** body and mark its wheel children with `wheel = true`.

```ruby
def fixed_update(_dt)
  vehicle.drive(throttle: Input.axis(:vertical), steering: Input.axis(:horizontal),
                handbrake: Input.down?(:space) ? 1.0 : 0.0)
end
```

`vehicle.speed` is in meters per second, negative in reverse.

## Pickups and zones

Tick **Trigger** in the object's Physics card (it stops being solid) and react when something
enters:

```ruby
class Coin < Behaviour
  def on_trigger_enter(other)
    return unless other.get_component(Player)

    Audio.play(:coin)
    destroy
  end
end
```

Only moving things (characters, dynamic and kinematic bodies) set triggers off.

## Spawn objects

Mark an object as a **Template** in the Inspector (`prefab = true`). It disappears from the game
and becomes something to copy:

```ruby
bullet = Scene.spawn("Bullet", position: transform.position, rotation: transform.rotation)
bullet.add_component(Bullet, speed: 30.0)
```

The copy comes with the template's physics and scripts.

## Look ahead with a ray

```ruby
hit = Physics.raycast(transform.position, transform.forward, 20.0, game_object)
Debug.log("I see #{hit[:object].name} #{hit[:distance]} m away") if hit
```

The last argument is an object the ray ignores (usually the one casting it).

## Show text on screen

Call it every frame from `update`:

```ruby
UI.text("Coins #{@coins}/#{@total}", x: 20, y: 40, size: 28, color: :gold)
```

Colors: `:white`, `:black`, `:red`, `:green`, `:blue`, `:yellow`, `:gold`, `:orange`, `:gray`, or
`[r, g, b]`.

## Menus, buttons and images

Everything in `UI` is drawn the frame you call it, so a menu is just a behaviour that draws
itself every frame. `UI.button` returns true on the frame it is clicked:

```ruby
def update(_dt)
  UI.rect(x: 0, y: 0, width: UI.width, height: UI.height, color: :black, alpha: 170)
  UI.text("My Game", x: UI.width / 2, y: 120, size: 72, align: :center)
  UI.image(:logo, x: 40, y: 40, width: 128)            # images/logo.png
  start_game if UI.button("Play", x: UI.width / 2 - 140, y: 300, width: 280, height: 58)
  Game.quit if UI.button("Quit", x: UI.width / 2 - 140, y: 374, width: 280, height: 58)
end
```

`UI.width` and `UI.height` are the size of the screen, so layouts can be centered. Pictures go in
the game's `images/` folder (`.png` or `.jpg`).

## Pause, and the flow between screens

`Time.scale = 0` freezes the game: physics stops and behaviours get an `update` with a delta of
0, so a menu keeps working while everything else stands still. `Time.scale = 1` resumes.

The usual flow of a game is made of three things:

- **A title screen:** a menu that starts open, with the game frozen behind it.
- **Pause:** the same menu opened by a key.
- **Other screens and levels:** `Game.load_scene("level2")` to move on, and
  `Game.load_scene(Game.scene_name)` to restart the current one.

The Platformer template ships all of this in `scripts/game_menu.rb`: a title screen, a pause
menu on Esc or P, and an end screen. Copy it into your game and call
`GameMenu.instance.finish("You win!")` when the game ends.

## Play sounds and music

Files go in the game's `sounds/` folder.

```ruby
Audio.play(:jump)                      # sounds/jump.wav (or .ogg, .mp3, .flac)
Audio.play(:hit, volume: 0.5, pitch: 1.2)
Audio.music(:theme, loop: true)
Audio.stop_music
```

## Change level

```ruby
Game.load_scene("level2")   # scenes/level2.glb; "main" is scene.glb
```

The switch happens at the end of the frame. Ruby global variables survive it, which is the simple
way to carry a score or lives from one level to the next.

## Keep score across objects

Put a manager behaviour on one object and let the others find it:

```ruby
class GameManager < Behaviour
  attr_reader :coins

  def self.instance = Scene.find("GameManager")&.get_component(GameManager)

  def start = @coins = 0
  def collect_coin = @coins += 1
end

# anywhere else
GameManager.instance.collect_coin
```

## Time

`Time.elapsed` is seconds since the game started, `Time.delta` the length of the last frame.
