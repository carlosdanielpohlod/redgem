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

## Read a gamepad

A game written with axes and actions already works with a gamepad: the left stick moves
`:horizontal` and `:vertical` (by as much as it is pushed), and A, X and B are `:jump`, `:fire`
and `:run`. For the rest, ask the gamepad itself:

```ruby
Input.gamepad?                        # is one plugged in?
Input.gamepad_axis(:right_x)          # -1..1: left_x, left_y, right_x, right_y (left and up are negative)
Input.gamepad_axis(:right_trigger)    # 0..1: left_trigger, right_trigger
Input.gamepad_down?(:start)           # held; also gamepad_pressed? and gamepad_released?
```

Buttons are named as on an Xbox pad: `:a`, `:b`, `:x`, `:y`, `:left_bumper`, `:right_bumper`,
`:back`, `:start`, `:dpad_up`, `:dpad_down`, `:dpad_left`, `:dpad_right`, `:left_stick`,
`:right_stick`. To walk slowly with a half-pushed stick, use `clamped` instead of `normalized`:

```ruby
direction = Vector3.new(Input.axis(:horizontal), 0, -Input.axis(:vertical)).clamped
```

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

To show it in the player's language, keep the text in the game's translation files and show it
with `I18n.t("hud.coins", coins: @coins, total: @total)`: see
[Texts in several languages](i18n.md).

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

A text field hands back what is in it after this frame's typing, so keep the value and pass it in
again (`Input.text` is what was typed this frame, for anything else):

```ruby
@name = UI.text_field(@name, x: 40, y: 200, width: 320, height: 48, max_length: 12)
```

## Pause, and the flow between screens

`Time.scale = 0` freezes the game: physics stops and behaviours get an `update` with a delta of
0, so a menu keeps working while everything else stands still. `Time.scale = 1` resumes.

The usual flow of a game is made of three things:

- **A title screen:** a menu that starts open, with the game frozen behind it.
- **Pause:** the same menu opened by a key.
- **Other screens and levels:** `Game.load_scene("level2")` to move on, and
  `Game.load_scene(Game.scene_name)` to restart the current one.

`GameMenu` is all of this, ready made: a title screen, a pause menu on Esc, P or a gamepad's
Start, and an end screen. Put it on an object from the scene's script, and end the game from
anywhere:

```ruby
class MainScene < SceneScript
  def setup
    Camera.main.add_component(GameMenu)   # title: "...", start_open: false
  end
end

GameMenu.instance.finish("You win!")
```

Its screens are `:title`, `:pause` and `:over`. A game adds options and screens of its own:

```ruby
menu = GameMenu.instance
menu.screens[:credits] = GameMenu::Screen.new(heading: "Made by Ana", back: -> { menu.show(:pause) })
menu.screens[:pause].add_option("Credits", index: 1) { menu.show(:credits) }
```

`back` is what the pause keys do on that screen. A label or a heading may also be a lookup,
`-> { I18n.t("menu.credits") }`, made each time it is shown (see [Texts in several languages](i18n.md)). A screen that draws more than buttons is a
subclass of `GameMenu::Screen` (`draw`, `space_above`, `opened`, `closed`, `typing?`): the
platformer's multiplayer screen, `games/platformer/scripts/multiplayer_screen.rb`, is one.

## Play sounds and music

Files go in the game's `sounds/` folder.

```ruby
Audio.play(:jump)                      # sounds/jump.wav (or .ogg, .mp3, .flac)
Audio.play(:hit, volume: 0.5, pitch: 1.2)
Audio.music(:theme, loop: true)
Audio.stop_music
```

## A sense of speed

```ruby
rush = (speed / TOP_SPEED).clamp(0.0, 1.0)
Camera.fov = 52 + 10 * rush          # the view widens
Camera.motion_blur = 0.7 * rush      # the edges of the picture streak
Audio.music_pitch = 0.8 + 0.7 * rush # the engine's note climbs (Audio.music(:engine) is looping)
```

## Change level

```ruby
Game.load_scene("level2")   # scenes/level2.glb; "main" is scene.glb
```

The switch happens at the end of the frame. Ruby global variables survive it, which is the simple
way to carry a score or lives from one level to the next.

## Do something later, or over a while

Every behaviour has timers. They run on game time, so they wait while the game is paused, and they
end with their object.

```ruby
after(2.0) { destroy }                                   # once, two seconds from now
every(0.5) { Scene.spawn("Bullet", position: muzzle) }   # again and again
tween(0.4) { |t| transform.scale = Vector3.one * (1.0 + t) }   # every frame, t going from 0 to 1
```

`after` and `every` return a timer you can `cancel`; `cancel_timers` calls off all of a behaviour's.
`tween` starts and ends gently; `ease: :linear` keeps a steady pace.

## Sparks, dust and smoke

`Particles.burst` lets out small dots that fly and fade. They are looks only: nothing collides
with them.

```ruby
# A coin picked up: gold sparks in every direction.
Particles.burst(transform.position, count: 24, color: :gold, glow: true)

# Smoke from an exhaust: a few gray puffs a second, drifting up in a narrow cone.
every(0.05) do
  Particles.burst(exhaust, count: 1, color: :gray, direction: Vector3.up, spread: 20,
                  speed: 1.0, life: 1.5, size: 0.4, gravity: -0.05)
end
```

| Setting | Meaning | Default |
|---------|---------|---------|
| `count` | How many | 12 |
| `color`, `alpha` | A color name or `[r, g, b]`; 0 to 255 | `:white`, 255 |
| `glow` | Shines (sparks, magic) instead of covering what is behind (dust, smoke) | `false` |
| `direction`, `spread` | Where they fly, and how many degrees they may stray: 180 is everywhere | up, 180 |
| `speed`, `life`, `size` | Meters per second, seconds, meters across (each particle varies a little) | 3.0, 0.8, 0.12 |
| `gravity` | 1 falls like everything else, 0 floats, negative rises | 1.0 |
| `image` | A picture from the game's `images/` folder drawn on each particle, tinted by `color` | a soft dot |
| `frames`, `fps` | The image is a sprite sheet of `[columns, rows]` pictures, read like a page; `fps` of them a second, round and round, or all of them once over the particle's life without one | one picture |
| `spin` | Degrees per second each picture turns | 0 |
| `lit` | Takes the light of the sun, the sky and the lights near it, as smoke and dust do (glowing ones shine with their own light either way) | `false` |
| `soft` | Meters over which it fades where it meets the ground or a wall, instead of cutting a hard line into it | 0 |

A white picture with see-through edges works best, since the color tints it:
`Particles.burst(at, image: :smoke, color: :gray, alpha: 90, size: 1.2, life: 1.5, gravity: -0.05, spin: 40)`.

### Over their life

`size`, `color` and `alpha` may each be a list of up to 4 values, which every particle goes
through evenly over its life, from the first at its birth to the last at its end. A single size
or alpha does what particles always did: it holds for half the life, then the size shrinks to
0.3 of itself and the alpha fades out. A single color holds.

```ruby
# A flame that licks (an 8 x 4 sheet at 30 pictures a second), shrinking from yellow to a dark ember.
Particles.burst(torch, count: 1, image: :flame_sheet, frames: [8, 4], fps: 30, glow: true, soft: 0.3,
                size: [0.5, 0.6, 0.15], color: [[255, 230, 160], [255, 120, 30], [70, 25, 15]], life: 0.6)

# Smoke that grows as it rises, fades in and out, and darkens in the shade.
Particles.burst(chimney, count: 1, image: :smoke, lit: true, soft: 1.0, size: [0.5, 2.5],
                alpha: [0, 120, 90, 0], gravity: -0.1, life: 3.0, spin: 20)
```

`cast_shadow: true` includes a particle in the directional sun's shadow cascades; it defaults to
`false`. Its shadow is an alpha-cutout sprite facing the sun, using its current size, rotation,
alpha and flipbook cell (or the circular dot mask without an image). Fragments whose combined
texture and particle alpha is below 0.5 cast no shadow. Fading can therefore shrink or remove a
shadow abruptly. This is a sprite approximation, not volumetric smoke: overlapping particles do
not accumulate density, particles do not receive shadows, and `soft:` only fades the color pass
against the camera's scene depth. Point lights do not cast particle shadows.

Lit particles are lit at their corners by the sun, the sky and the nearest lights, as a puff lit
on about half of it; they take no shadows yet.

## Light that flickers and fades

An object with a light (a lamp, a fire's glow, made in Blender like any light) can change how
bright it shines and in what color while the game runs:

```ruby
glow = Scene.spawn("FireLight", position: torch)
every(0.05) { glow.light.intensity = 40 + rand * 25 }           # a flame's flicker
tween(0.6) { |t| glow.light.intensity = 300 * (1 - t) }        # a blast that fades
glow.light.color = [255, 120, 40]                               # or a name: :orange
```

`intensity` is in the units Blender gives the light (watts for a point or spot light, lux for the
sun) and is at least 0; `color` is three channels from 0 to 255, as a color picker shows them.

## Control animations

Every animation in a level plays in a loop from the start. `Animation` is for the ones that should
not, by the name the action has in Blender:

```ruby
Animation.stop(:DoorOpen)                              # in a scene script's setup: starts closed
Animation.play(:DoorOpen, loop: false)                 # opens once and stays open
Animation.play(:DoorOpen, loop: false, speed: -1.0)    # closes
Animation.pause(:Fan)                                  # freezes where it is; play carries on
Animation.playing?(:DoorOpen)                          # false once it reached its end
```

`Animation.names` lists the level's clips; `time`, `duration` and `seek(name, seconds)` are there
for syncing things to a clip. A stopped clip lets go of its objects, so scripts can move them.

## Animate one character

`Animation` is for the whole level. A character with several clips (idle, walk, run) picks its own
through `animation`, and so does each spawned copy of it:

```ruby
def start = animation.play(:Idle)

def update(_dt)
  animation.play(moving? ? :Run : :Idle, fade: 0.2)   # blends from one to the other over 0.2 s
end
```

Playing the clip a character is already on just carries on, so it is fine to call every frame.
`loop: false` plays once and holds the last pose; `speed:` and `restart: true` work as above.
A second clip can be laid over the first:

```ruby
animation.mix(:Wave)                 # a clip that only moves the arm: the legs keep walking
animation.mix(:Limp, weight: 0.5)    # half of it blended into everything
animation.unmix
```

`animation.follow_scene(fade: 0.3)` hands the character back to what the level plays, and
`animation.clip` is the name of its own clip (nil while it follows the level).

## Save progress

`Save` keeps numbers, texts and true/false values between runs, in one file per game:

```ruby
best = Save.get(:best_time, 999.0)    # the second value is what you get when nothing was saved
Save[:best_time] = time if time < best
Save[:name] = "Ana"
Save.delete(:name)
Save.clear                            # start from nothing
```

Whole numbers come back as integers (a saved `12.0` is read as `12`): call `.to_f` on a value you
divide.

The file is written on every change, to `.config/redgem/saves/by-id/<id>.json` in the player's
home folder, where `<id>` is the game's `id` from its `game.json`: the save stays with the game
when its folder is moved or renamed. A game with no `id` saves to
`.config/redgem/saves/<folder>-<digits>.json`, with digits made from where its folder is. Tests
get a save file of their own, empty at the start of each test.

Each change is flushed to the disk before it replaces the file, and the save before it is kept
beside it as `<name>.json.bak`. A save found damaged (cut short by a crash or a bad disk) is moved
aside to `<name>.json.damaged`, never written over, and the game starts from the backup; the log
says so, naming both files. When the backup is damaged too, the game starts from nothing.

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
