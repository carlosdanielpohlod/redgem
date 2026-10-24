---
description: "Behaviours: Ruby classes attached to objects, their callbacks, settings and scene scripts."
---

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

### A script that never returns

Every call the engine makes into the scripts (a behaviour's `start` and `update`, a contact, a
scene script, a network handler) has a budget of time: 1000 ms unless the game's `game.json` says
otherwise. A call still running past it (a `loop {}` with no `break`, a `while true` waiting for
something that never comes) is stopped with a `ScriptTimeout` error. The behaviour is switched
off like one that raised, the Console says which behaviour and method it was and how long it ran,
and the game goes on. The game freezes for the length of the budget, once.

```json
{ "id": "com.mystudio.mygame", "title": "My Game", "script_budget_ms": 2000 }
```

`script_budget_ms` is a whole number of milliseconds from 1 to 60000. It applies the same way to
a game you play, to Play in the editor and to every room of a dedicated server, where each room
has its own scripts and its own budget. Raise it for a scene script that builds a large level in
one go; a frame's update should never come near it.

`ScriptTimeout` is not a `StandardError`, so `rescue => e` does not catch it, and a script that
rescues `Exception` is stopped all the same: once a call is over its budget, it stops again at
every step until it has returned. Time spent inside the engine's own methods counts, but they are
never cut off halfway: the call stops at its next line of Ruby.

A recursion that never ends stops on its own, with `SystemStackError`, long before the program's
stack runs out, and is handled the same way: its behaviour is switched off. `redgem-test` runs a
file of Ruby tests with no budget, as one call.
