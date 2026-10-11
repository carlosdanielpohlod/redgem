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
