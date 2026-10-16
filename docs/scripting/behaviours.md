# Behaviours

A behaviour is a Ruby class that inherits from `Behaviour`. Attached to an object, it gets one
instance for that object, and the engine calls its methods as the game runs.

```ruby
class Door < Behaviour
  def start; end                      # once, before the first update
  def update(dt); end                 # every frame; dt is seconds since the last one
  def fixed_update(dt); end           # 60 times per second, with the physics
  def on_trigger_enter(other); end    # something entered this object's trigger (or it entered one)
  def on_trigger_exit(other); end
  def on_collision_enter(other); end  # this object's body touched another
  def on_collision_exit(other); end
  def on_destroy; end
end
```

Define only the ones you need. `other` is the other object (a `GameObject`).

## Files and class names

The file name decides the class name: `camera_follow.rb` must define `CameraFollow`. Scripts live
in the game's `scripts/` folder.

You never `require` another script. The first time you use a class name, RedGem loads the file it
should be in (`Patrol` loads `scripts/patrol.rb`). That works for plain helper classes too.

## Attaching a behaviour

There are three ways, and they mix freely:

- **In the editor:** Inspector > *Add Component*, or drag a script from the Project panel onto an
  object.
- **In Blender:** the custom property `script = "door.rb"` (several: `"door.rb, sound.rb"`).
- **From Ruby:** `object.add_component(Door, speed: 2.0)`.

## Settings

A behaviour reads its settings with `prop`:

```ruby
def start
  @speed = prop(:speed, 3.0)   # 3.0 when nothing sets it
end
```

`prop(:speed, default)` returns, in this order: what `add_component` was given, the object's
custom property `speed` (set in the Inspector or in Blender), or the default. So the same script
can be tuned per object without changing code.

## Reaching things

Inside a behaviour:

| You write | You get |
|-----------|---------|
| `game_object`, `transform`, `name` | this object, its transform, its name |
| `character`, `rigidbody`, `vehicle` | this object's physics body of that kind, or `nil` |
| `find("Hero")`, `find_all(:coin)` | an object by name, all objects with a tag |
| `get_component(Health)` | another behaviour on the same object |
| `other.get_component(Player)` | a behaviour on another object |
| `destroy` or `destroy(other)` | removes an object from the game |

### Objects that are gone

`destroy` takes an object out at the end of the step: its `on_destroy` still runs, and until then
it can be used as usual. After that, a reference you kept (in a variable, a global, a timer's
block) stands for that object only. `alive?` says `false`, `name` and `tag` still answer, and
anything else raises `game object "Coin" (id 12) was destroyed`, even once a newly spawned object
sits in the same place (its `id`) or the level was switched. A kept reference never starts acting
on another object.

```ruby
@target = nil unless @target&.alive?   # forget an enemy someone else destroyed
```

## Setting a level up in Ruby

Instead of attaching scripts object by object, a level can have a **scene script**:
`scripts/main_scene.rb` for `scene.glb`, `scripts/level2_scene.rb` for `scenes/level2.glb`. Its
`setup` runs when the level loads, before any behaviour starts.

```ruby
class MainScene < SceneScript
  ENEMY_SPEEDS = { "Slime" => 1.5, "Bat" => 4.0 }

  def setup
    find!("Player").add_component(Player, speed: 6.0)
    ENEMY_SPEEDS.each { |name, speed| find!(name).add_component(Patrol, speed: speed) }
    find_all("coin").each { |coin| coin.add_component(Coin) }
  end
end
```

`find!` is `find` that stops with a clear error when the object is missing. With a scene script
the level file says what is there and where, and Ruby says what it does.

## When a script fails

An error in a behaviour does not stop the game. That behaviour is switched off, and the Console
shows the error with the file and line. Fix it, press Play again.
