---
description: "Gameplay recipes: movement, cameras, triggers, spawning, UI, sound, menus and more."
---

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

Built-in actions: `:jump` (Space), `:fire` (Left Ctrl or Enter), `:run` (Left Shift). A game
declares its own and lets players change them: see [Controls](#controls).

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

Up to four gamepads are read at once, numbered from 0. Each is a player: `pad:` picks whose
actions, axes or buttons to read, and leaving it out reads the first.

```ruby
Input.pads                              # [0, 1]: the gamepads plugged in
Input.action_pressed?(:jump, pad: 1)    # the second player jumped
Input.axis(:horizontal, pad: 1)         # the second player's stick
Input.gamepad_down?(:start, pad: 1)
```

The keyboard belongs to the first player (pad 0). The other players start with the gamepad's
bindings only, unless a player binds keys of their own (two players sharing one keyboard).

Gamepads cannot rumble: the window library the engine uses on desktops has no vibration, and
neither does Android's.

## Controls

A game names what its players do (actions, which are on or off, and axes, from -1 to 1) and says
what triggers them. Scripts then read the names, never the keys, so players can change the keys.
The built-in `:jump`, `:fire`, `:run`, `:horizontal` and `:vertical` are always there; a game adds
its own, or binds one of those anew, in the `input` section of its `game.json`:

```json
{
  "input": {
    "actions": {
      "dash": ["key:e", "pad:b"],
      "jump": ["key:space", "key:w", "pad:a"]
    },
    "axes": {
      "steer": {"negative": ["key:a", "pad:dpad_left"], "positive": ["key:d", "pad:dpad_right"], "pad_axis": "left_x"},
      "climb": {"pad_axis": "right_y", "invert": true}
    }
  }
}
```

A binding is `key:` with a key's name (`key:space`, `key:left_shift`, `key:7`) or `pad:` with a
gamepad button's (`pad:a`, `pad:dpad_up`). An axis' `pad_axis` is a stick that moves it too, by as
much as it is pushed; `invert` runs that stick the other way (up is negative on a stick). A
binding the engine does not know is left out with a warning, and the game plays on. Scripts can
declare names too, before using them. There, a binding may also be a Symbol (a key) or
`{keys: [...], buttons: [...]}`, mixed as one likes:

```ruby
Input.define_action(:dash, keys: [:e], buttons: [:b])     # or define_action(:dash, :e, "pad:b")
Input.define_axis(:steer, negative: [:a, "pad:dpad_left"], positive: {keys: [:d], buttons: [:dpad_right]},
                  pad_axis: :left_x)
Input.actions                   # [:dash, :fire, :jump, :run]
Input.action_released?(:dash)   # true on the frame the last of its keys and buttons lets go
```

Declaring a name again binds it anew; declaring it as the other kind (an axis that was an action)
also takes away what players had bound to the old one.

### Letting players rebind

Each player (a gamepad's number, `pad:`, 0 when left out or `nil`) has bindings of their own:

```ruby
Input.bindings(:jump)                         # ["key:space", "pad:a"]
Input.bindings(:horizontal)                   # {negative: ["key:a", "key:left"], positive: [...]}
Input.bind(:jump, key: :k)                    # Space and K both jump now
Input.bind(:jump, key: :j, button: :y)        # both given: both bound (unbind takes both away too)
Input.unbind(:jump, key: :space)              # only K
Input.unbind(:jump)                           # nothing: every key and button taken away
Input.bind(:fire, button: :y, pad: 1)         # the second player's Y fires
Input.bind(:horizontal, key: :j, side: :negative)   # an axis says which way the key pushes it
Input.reset_bindings(:jump)                   # back to the game's, for every player
Input.reset_bindings(pad: 1)                  # all of the second player's, back to the game's
```

What players bind is kept between runs in the user's RedGem folder, under `bindings/`, named as
the game's save is (`by-id/<game id>.json`, or after the game's folder for a game with no `id`).
It is written at the end of the frame in which it changed, once however many calls changed it,
and not at all when it ends up as the file already says. A player binds an axis' keys and
buttons only: its stick, and whether the stick is inverted, are always the game's, so a game's
"invert Y" option (declaring the axis again with `invert:`) reaches every player.

One key may be bound to several actions, and every action it is bound to sees it: Enter can both
`:fire` in the game and `:confirm` in a menu. A settings screen that wants each key to do one
thing checks `Input.bindings` of the other actions before binding it.

A "press a key" screen asks for the next key or button pressed. The block gets the binding as
text, and the gamepad's number (`nil` for a key); a key already held when it asks does not count.
Escape, or a gamepad's B or Back, calls the capture off instead: the block gets `nil` for the
binding, so the player can back out (`cancel:` lists other bindings for that, `cancel: []` none).
Whatever went down on the frame a capture ends is the capture's: no action fires and no menu opens
with it.

```ruby
Input.capture_next do |binding, pad|
  next if binding.nil?   # called off: jump stays as it was

  Input.unbind(:jump, pad: pad)
  Input.bind(:jump, binding, pad: pad)
end
Input.capture_next(cancel: ["key:backspace"]) { |binding, pad| ... }   # Escape can be bound
Input.capturing?       # still waiting
Input.capturing_for?(block)   # still waiting to call that block (not once another capture took its place)
Input.cancel_capture   # stop waiting (the block is never called)
Input.bindings_changes # a count that goes up whenever actions, axes or bindings may have changed
```

Sticks and triggers are not caught, only keys and buttons. `Input.axis_stick(:horizontal)` says
which stick moves an axis (`:left_x`, or `nil`): it is the game's, the same for every player.

### The controls page

`GameMenu`'s settings screen has a Controls button that opens a ready-made controls page (the
`:controls` screen). At its top, a choice of the player whose bindings it shows (1 to 4: the first
also has the keyboard). Under it, a row for each action and one for each side of each axis, every
one the game declared, in two cells: the player's keys, and their gamepad buttons (an axis' stick
shows among them, though no player rebinds it). Up and down move between the rows, left and
right between a row's two cells.

Enter, a gamepad's A or a click on a cell (which also moves the focus there) waits for the next
key (the keys' cell) or button (the buttons' cell): that one takes the place of everything the cell
had, and the row's other cell keeps its own. A button pressed for the keys' cell (or a key for the
buttons'), or a button of another player's gamepad, is not for it, and it waits on; any keyboard
key is taken for any player. Escape or a gamepad's Back calls the wait off (and B too, for a keys' cell), so those
are never bound there.
A key bound to two rows is allowed, as above, and both cells show it in orange with a `!`. "Reset to
defaults" puts all of the shown player's bindings back to the game's; Back (or Esc) goes back to the
settings, on their Controls button (the settings were kept on the way in). What players bind is
kept for the next runs as it changes. A game adds options to both screens as to any other
(`add_option`): they come after the engine's buttons unless an `index:` says otherwise.

A row is named by the game's translation files, as `game.controls.action_<name>` (an axis' sides
as `game.controls.action_<name>_negative` and `_positive`); without one, its name is made readable (`:air_dash`
is "Air dash", the sides of `:steer` "Steer -" and "Steer +"). The engine names its own (`:jump` is
"Jump", the negative side of `:horizontal` "Move left") and the keys and buttons ("Space", "Left
Shift", "A button", "Left stick X"), as `game.controls.*` texts a game may change too.

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

Forces push for one physics step, so a steady push goes in `fixed_update`:

```ruby
def fixed_update(_dt)
  rigidbody.add_force(Vector3.up * 30.0)                                  # newtons: a hovering crate
  rigidbody.add_force(transform.forward * 5.0, at: transform.position + Vector3.up)   # also tips it
  rigidbody.add_torque(Vector3.new(0, 2, 0))                              # spins it around y
end
```

`rigidbody.mass`, `gravity_scale` (0 floats), `angular_velocity` (degrees per second) can be read
and changed while the game runs. `rigidbody.kinematic = true` hands a falling crate to your script
(a crane picks it up); `rigidbody.dynamic = true` drops it again, at rest. `rigidbody.sleep` keeps
a body still until something bumps it or `wake` is called.

## Doors, ropes and chains: joints

A joint holds two bodies together, or one to the world (give `nil` as the other). Points and axes
are in world space.

A door on a hinge that pushes open up to 110 degrees, and that its motor pulls shut (back to the
0 degrees it started at, its other limit):

```ruby
class Door < Behaviour
  def start
    pin = transform.position + Vector3.left * 0.5            # its left edge
    @hinge = Physics.joint(:hinge, game_object, nil, anchor: pin, axis: Vector3.up, limits: [0, 110])
  end

  def close = @hinge.motor(-60, 50)                           # degrees per second, newton meters
  def let_go = @hinge.motor(0, 0)
end
```

A rope of distance joints: each link may come closer to the one above it, never farther.

```ruby
hook = Vector3.new(0, 8, 0)
above = nil
links.each do |link|                                          # dynamic spheres, top to bottom
  Physics.joint(:distance, link, above, other_anchor: above ? nil : hook)
  above = link
end
```

`spring: 2, damping: 0.3` makes it a bungee. Other kinds: `:fixed` glues two bodies as they are,
`:slider` lets one slide along an axis (`limits:` in meters), `:point` lets it turn every way (a
ball and socket). `break_force: 500` breaks a joint pulled harder than 500 newtons, or twisted
harder than 500 newton meters (`joint.broken?` then says so); `joint.remove` lets go; destroying either object removes it.

## Collision layers

Bullets that fly through pickups, ghosts that walk through walls: give each body a **layer** and
say in `game.json` which layers meet. Up to 16 layers; `default` is always there.

```json
"physics": {
  "layers": ["default", "player", "enemy", "bullet", "pickup"],
  "collides": {"bullet": ["enemy", "default"], "pickup": ["player"]}
}
```

A layer with no `collides` entry meets every layer; two layers meet only when each lets the other
in, so above bullets hit enemies and walls but pass through players, pickups and other bullets,
and pickups only notice players. Put an object on a layer with the `layer` custom property (in
Blender or the Inspector), or from a script:

```ruby
bullet = Scene.spawn("Bullet", position: muzzle, rotation: transform.rotation)
bullet.rigidbody.layer = :bullet
bullet.rigidbody.velocity = transform.forward * 40.0
```

Triggers only notice bodies whose layer meets theirs, and queries look at the layers they are given.

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

The last argument is an object the ray ignores (usually the one casting it). Every query also takes
`layers:` (a layer or a list) and `ignore:` (an object or a list):

```ruby
# Everything a laser goes through, nearest first.
Physics.raycast_all(eye, forward, 50.0, layers: [:enemy, :default], ignore: [game_object, gun])

# Would a ball of radius 0.4 get from here to there? (nil: the way is clear)
blocked = Physics.sphere_cast(transform.position, target, 0.4, ignore: game_object)

# An explosion: everything on the enemy layer within 5 m.
Physics.overlap_sphere(transform.position, 5.0, layers: :enemy).each do |hit|
  hit[:object].rigidbody&.add_impulse((hit[:point] - transform.position).normalized * 20.0)
end
```

`box_cast`, `capsule_cast`, `overlap_box` and `overlap_capsule` work the same way. Hits come
nearest first, each object once, at most 64.

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
Start, an end screen, and a settings screen (see [A settings screen](#a-settings-screen)). Up and
down move the focus between its buttons; Enter or a gamepad's A picks the focused one. Put it on an object from the scene's script, and end the game from
anywhere:

```ruby
class MainScene < SceneScript
  def setup
    Camera.main.add_component(GameMenu)   # title: "...", start_open: false
  end
end

GameMenu.instance.finish("You win!")
```

Its screens are `:title`, `:pause`, `:over`, `:settings` and `:controls`. A game adds options and screens of its own:

```ruby
menu = GameMenu.instance
menu.screens[:credits] = GameMenu::Screen.new(heading: "Made by Ana", back: -> { menu.show(:pause) })
menu.screens[:pause].add_option("Credits", index: 1) { menu.show(:credits) }
```

`back` is what the pause keys do on that screen. A label or a heading may also be a lookup,
`-> { I18n.t("menu.credits") }`, made each time it is shown (see [Texts in several languages](i18n.md)). A screen that draws more than buttons is a
subclass of `GameMenu::Screen` (`draw`, `space_above`, `opened`, `closed`, `typing?`, and `rows`
for widgets of its own that take the focus before its buttons, with `focus`): the
platformer's multiplayer screen, `games/platformer/scripts/multiplayer_screen.rb`, is one.

### A loading screen

```ruby
Game.on_loading do |delta|
  UI.text("Loading #{Game.loading_scene}...", x: 24, y: 24, size: 28)
  Game.cancel_scene_load if UI.button("Cancel", x: 24, y: 80, width: 160, height: 48)
end
Game.load_scene_async("level2")
```

Normal simulation pauses while the file is read; the loading screen, audio and network continue.
Failure or cancellation preserves the current scene. Check `Game.loading_error` after the load
ends to report a failure or retry. Successful activation removes the old loading callback.
Physics, texture decoding and GPU upload can still cause an activation hitch; see
[the API's loading limits](api.md#game). Use `Game.load_scene` for the existing synchronous switch.

## A settings screen

`GameMenu` has one ready, reached from its title and pause screens: window mode, resolution,
render scale, vsync, frame-rate cap, quality and master volume, with a button to its controls page
(see [The controls page](#the-controls-page)). A game builds its own with
`Settings` and the widgets of `UI`, which the keyboard and a gamepad work through a focus:

```ruby
def update(_dt)
  @focus ||= UI::Focus.new
  @focus.update(3)   # up and down (arrows, D-pad) move it among three widgets
  box = { x: 40, width: 480, height: 44 }
  on = UI.toggle("VSync", Settings.vsync, y: 100, focused: @focus.at?(0), **box)
  Settings.vsync = on unless on == Settings.vsync
  names = %w[Low Medium High Ultra]
  index = Settings::QUALITIES.index(Settings.quality) || 3
  picked = UI.choice("Quality", names, index, y: 152, focused: @focus.at?(1), **box)
  Settings.quality = Settings::QUALITIES[picked] unless picked == index
  volume = UI.slider("Volume", Settings.volume, y: 204, focused: @focus.at?(2), **box)
  Settings.volume = volume unless volume == Settings.volume
end
```

Each change shows at once; `Settings.save` keeps them for the game's next runs. A quality caps
what the level's environment asks for (samples a pixel, temporal anti-aliasing, shadow cascades
and distance, scattered copies and how far they show, the sky picture's size, ambient occlusion,
bloom): `:ultra` draws the level as it was made, and nothing ever gets more than it asked for.
What the graphics card cannot do is left out whatever the quality.

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

# Smoke follows this object, from a local exhaust offset. Old puffs stay in the world.
@smoke = Particles.emitter(game_object, rate: 20, offset: [0, 0.5, 1], color: :gray,
                           direction: Vector3.up, spread: 20, speed: 1.0, life: 1.5,
                           size: 0.4, gravity: -0.05)
```

`Particles.emitter(object, rate: 24, offset: Vector3.zero, **settings)` returns a handle with
`active?` and `stop`. It takes the burst settings below except `count`: `rate` is particles per
game second, greater than zero and at most 4096. Offset uses the object's full world transform;
direction follows its world rotation. Birth positions are spread along the object's movement
between frames. Movement over 10 meters in one update is treated as a teleport: new particles
start at the destination. Each birth is aged and advanced by the time since its birth within
the frame. Emitted particles move independently in world space, without inheriting the
object's velocity. `Time.scale = 0` pauses emission.

Destroying the object or changing scenes ends its emitters; stopping one leaves its existing
particles to finish their lives. `Particles.clear` removes particles and emitters together. Up to
256 emitters share the existing limit of 4096 live particles. When that limit is full, missed
births are dropped rather than saved for a later burst. Priority passes to the next emitter
that is waiting for space, so a full pool cannot permanently favor the first one. Emitter
handles are never reused.

These are local visual effects, not replicated by `NetworkSync`. Start matching effects on
each client through gameplay events. A game running without a presenter, including a dedicated
server, returns inactive emitter handles and does not allocate or simulate emitters.

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
not accumulate density, and `soft:` only fades the color pass
against the camera's scene depth. Point lights do not cast particle shadows.

Lit particles are lit at their corners by the sun, the sky and the nearest lights, as a puff lit
on about half of it. `lit: true` also receives the scene's filtered sun shadows, using the same
shadow cascades as surfaces. Sun shadowing leaves sky and point-light contributions intact.
Unlit particles, including unlit glowing ones, keep their own color in shadows.

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
beside it as `<name>.json.bak`, written the same way. A save found damaged (cut short by a crash
or a bad disk) is moved aside to `<name>.json.damaged`, never written over, and the backup takes its
place; the log says so, naming both files. When the backup is damaged too, it is moved aside as
well and the game starts from nothing. A save that cannot be read at all (another program holds
it, say) is left alone: the game runs without saving, and the log says why.

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

## Measure frame times

```ruby
Debug.stats_visible = true           # also starts profiling; shows the developer panel
Debug.frame_stats[:mean][:update_ms] # average foreground update time, in milliseconds
Debug.frame_stats[:peak][:scene_ms]  # largest foreground scene operation in the window
Debug.stats_visible = false          # hides the panel while keeping collection active
Debug.profiling = false              # stops collection; keeps the last summary
Debug.reset_frame_stats              # clears the window and any unfinished measurement
```

The panel works in the player, editor Play and recordings. Collection is off by default and
keeps at most 120 completed frames. Timings use the steady clock, so pausing or slowing the game
does not change what is measured. Physics is included in update, and presentation includes any
upload, frame cap or synchronization wait: these are inclusive foreground wall times, not GPU
timers or a per-function sampling profiler. Leave the panel off for reproducible pictures.


## Draw world diagnostics

```ruby
def update(_delta)
  origin = transform.position
  Debug.line(origin, origin + Vector3.up, color: :green)
  Debug.box(origin, Vector3.new(1, 2, 1), color: :yellow, depth_test: false)
end
```

Lines and axis-aligned wire boxes use world coordinates and are submitted every frame. Box size
is its full nonnegative width, height and depth. They create no objects and are local to the
presenter; a dedicated server validates arguments and returns `false` without collecting commands.
Calls return `true` when accepted and `false` after the shared limit of 4096 commands per frame.
The next frame starts with an empty buffer. Invalid vectors, box bounds or color channels are rejected before reaching the renderer. Use `UI.text` for a screen-space label.

`depth_test: true` hides shapes behind opaque geometry; `false` draws through it. Transparent
surfaces use the renderer's normal depth, so this is an opaque-scene diagnostic. Frames with shapes
use an unjittered camera and restart temporal history; normal temporal sampling resumes when no
shapes are submitted. Editor, scaled and recorded views reuse their color/depth target. Native
windows allocate one cached target on demand; if allocation fails, the game still draws, reports
a warning and omits the shapes. Changing the window size allows another allocation attempt.


## Keep separate progress in save slots

```ruby
Save.slot = 'profile-1'
Save[:coins] = 12
Save.slot = 'profile-2'
Save[:coins]                         # nil: another slot
Save.slot = 'profile-1'
Save[:coins]                         # 12
Save.slots                          # sorted String names, including 'default'
Save.slot = 'default'                # the original save, with its existing data
```

Every engine starts on `default`. Selection lasts across scene changes and is independent for
server rooms, even when their database is shared. `get`, `set`, `update`, `keys`, `delete` and
`clear` use the selected slot; clearing one does not erase the others. An `update` block writes
back to the store selected at entry, even if the block selects another slot. The block's other
side effects are not rolled back when it raises.

Names contain 1–48 lowercase ASCII letters/digits, hyphens or underscores and start with a letter
or digit. Symbols are accepted on assignment; the getter and list return Strings. Invalid names
raise `ArgumentError`; a failed selection leaves the previous slot and its data selected. At
most 64 named slots can be opened per engine/run (and per shared local-file backend); listing is
bounded to 4096 names. Unsupported custom backends or exceeded limits raise `RuntimeError`.

The default file is neither renamed nor migrated. Named files live beside it in
`<save-file>.slots/slot-<name>.json` and use the same atomic writes, backups and damaged-file
recovery. Existing symbolic links at slot folders/files/backups are refused. Files are cached
by the backend, so returning to an opened slot does not create stale copies of its values.
Local-file stores retain their existing single-threaded, one-process contract; shared server
rooms use SQLite. Named SQLite data has separate tables and the original default table remains
unchanged. Storage operations still run synchronously.

`Save.slots` includes persisted names plus slots opened during this run. A file slot is only
persisted after a write; the database remembers named slots when they are opened. A supplied
custom `SaveStore` keeps its original behavior and must implement the optional slot factory to
support named selection. Game tests isolate slot files too: fresh runs clear live data/backups,
restart runs copy them, and damaged evidence is retained rather than copied into another test.
This API stores values; it does not automatically serialize scene objects or migrate game data.

### Keeping references to scene objects

A loaded object's `stable_id` is its identity in the scene file, independent of its name or
runtime `id`. Use `Scene.find_by_id(identity)` to look it up in the current scene; it accepts a
String or Symbol and returns `nil` when the original is gone or the identity is unknown.

```ruby
Save[:selected_object] = {
  "scene" => Game.scene_name,
  "id" => Scene.find!("Door").stable_id
}
selection = Save[:selected_object]
if selection && selection["scene"] == Game.scene_name
  door = Scene.find_by_id(selection["id"])
end
```

Identities are scoped to one scene: save the scene name with the identity. The raw String has
no `#` prefix (the command-line selector adds that prefix). Old files receive deterministic
identities on read; a scene must be saved by the editor or command-line tools to persist them
before an external tool reorders its nodes. See [scene editing](../command-line.md).

Runtime copies have `nil` identities, including their descendants and copies of copies. A copy
never resolves as its source, even after an object is destroyed or a runtime slot is reused.
An old object reference remains stale after a scene switch; look up the saved identity again.
An object whose in-memory source supplies no identity also returns `nil`. This lookup does not
serialize transforms, behaviours or spawned instances; object checkpoints are a separate API.

### Object checkpoints

`GameObject.capture_state` returns a versioned Hash of the object's local position, quaternion
rotation, scale, tint and visibility. Keep that Hash unchanged in `Save`, beside the object's
identity. On a fresh run or after loading the matching scene, resolve the identity and restore it:

```ruby
hero = Scene.find!("Hero")
Save[:hero] = { "id" => hero.stable_id, "state" => hero.capture_state }
# Later, in the same scene (for example its SceneScript#setup):
saved = Save[:hero]
hero = saved && Scene.find_by_id(saved["id"])
hero.restore_state(saved["state"]) if hero
```

Restoration validates the complete payload and resulting hierarchy before changing anything.
The scene name, source node, original/copy kind, parent source and current physics body must
match. Malformed, incompatible or oversized data raises `ArgumentError`; a destroyed or pending
object raises `RuntimeError`. A stale reference never restores a new object that reused its slot.
A hierarchy pending destruction is refused. In-memory sources need explicit scene-file identities.

The pose is local to the current parent. Physical bodies in the moved hierarchy are teleported
immediately and stop moving; velocities are not saved. Existing colliders cannot change scale:
a checkpoint that would rescale a physical hierarchy is refused. Render-only hierarchies can
restore scale. Local components and resulting world components are bounded to one million in
magnitude, and at most 4096 objects can move in one restoration.

A checkpoint also works on an explicitly chosen runtime copy of the same source and parent;
it cannot silently select a copy or overwrite its original. The game creates any needed copies
first. This API neither spawns nor resurrects objects, changes parents, nor restores descendants'
individual local poses or appearance. Save each object's checkpoint if those values matter.
Animations, behaviours, joints, forces and game time remain active. Health, inventory and other
script-owned values must be saved explicitly beside the checkpoint; this is not a whole-game
snapshot. Checkpoints follow scene-file identity rules described above.
