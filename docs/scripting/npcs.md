---
description: "NPCs: a Brain (a state machine with transitions, guards, timers and events), Perception (a sight cone with a line of sight, and hearing), daily schedules on a game clock, and head look-at."
---

# NPCs

A character the player meets is made of a few behaviours on one object:

- a `Brain`, which says what it is doing (patrolling, chasing, talking) and what makes it do
  something else;
- a `Perception`, which tells the brain what it sees and hears;
- a `Schedule`, which says what it does at each hour of the game's day;
- a `HeadLook`, which turns its head toward who is near;
- an [`Interactable`](interaction.md), when the player can talk to it or use it.

```ruby
class MainScene < SceneScript
  def setup
    find!("Player").add_component(Interactor)
    guard = find!("Guard")
    guard.add_component(Perception, sight: 15.0, fov: 90.0, hearing: 12.0)
    guard.add_component(Interactable, prompt: "guard.talk")
    guard.add_component(Guard)
  end
end
```

## Brain

A brain is a state machine. A game's NPC is a subclass of `Brain` that declares its states and
the transitions between them:

```ruby
class Guard < Brain
  SEARCH_SECONDS = 8.0

  initial :patrol
  state :patrol                          # enter_patrol, update_patrol(dt), exit_patrol
  state :chase, update: :run_at_target   # or a hook by name (or a block)
  state :search
  state :talk

  transition from: :patrol, to: :chase, on: :see, if: ->(seen) { seen.tag?("Player") }
  transition from: :chase, to: :search, on: :lose_sight
  transition from: :search, to: :patrol, after: SEARCH_SECONDS
  transition from: %i[patrol search], to: :search, on: :hear
  transition from: :patrol, to: :talk, on: :interact
  transition from: :talk, to: :patrol, after: 3.0
  transition from: :any, to: :patrol, on: :reset

  def update_patrol(dt); end            # walk the route (see Navigation)
  def run_at_target(dt); end
  def enter_talk = Debug.log("Halt!")
end
```

A **state** has three hooks, each run on the brain: `enter` as it comes into the state, `update`
every frame it is in it (with dt), and `exit` as it leaves. Unless given (`enter: :name`, or a
block), they are the brain's methods `enter_<state>`, `update_<state>(dt)` and `exit_<state>`,
when it has them. The brain starts in its `initial` state, or the first one declared, on its
first frame.

A **transition** goes `from:` a state, a list of states, or `:any`, `to:` another, when:

- `on:` an **event** is fired (without `on:`, it is checked every frame);
- `after:` that many seconds have gone by in the state (a **timer**);
- its **guards** agree: `if:` is true and `unless:` is false.

Every frame, the transitions without `on:` are checked in the order they were declared, and the
first that may be taken is (at most one a frame); then the state's update runs. A transition
from `:any` never leads to the state the brain is in already.

A guard is a method's name, called with nothing, or a block run on the brain. A block that takes
an argument gets the event's **payload**: what was seen, the noise heard, who used it.

The events the engine fires on its own:

| Event | When | Payload |
|---|---|---|
| `:see` | the object's Perception sees something come into sight | the object |
| `:lose_sight` | ... and go out of it | the object |
| `:hear` | the Perception hears a noise | a `Perception::Noise` |
| `:interact` | the player uses the object's Interactable | the actor (the hero) |
| `:schedule` | the hour of another activity of its Schedule came | the activity (a Symbol) |

The game fires its own with `brain.fire(:hit, damage)`; `fire` is true when the brain changed
state. `brain.go(:stunned)` changes state at once, whatever the transitions say. `state`,
`previous_state` and `time_in_state` say where it is; `in?(:chase, :search)` asks.

Time in a state is game time: it stands still while the game is paused (`Time.scale = 0`). A
`Brain` subclass with its own `update` calls `super`.

Declaring a state, or a transition, with an option it does not take raises at once. A transition
to a state that was never declared raises as the brain starts.

A subclass of a brain keeps its parent's states and transitions and may add to them.

## Perception

A `Perception` gives an NPC its senses.

**Sight** is a cone in front of its eyes. It sees an object that is:

1. no further than `sight` meters;
2. inside the cone: no more than half of `fov` degrees off the way the NPC faces, measured across
   the ground (so a hero right in front, or on another floor, is in it);
3. in the line of sight: a ray from its eyes to the object hits nothing else first. What the NPC
   carries is seen through, and the object's own collider, or one under it, counts as reaching
   it. Triggers never block it.

It looks from `eyes` meters above its origin, at `aim` meters above the watched object's origin.
It looks for the players' heroes (the objects with an [Interactor](interaction.md)), or for the
objects of one tag with `watch: "Enemy"`. In a game over the network, put an Interactor on every
hero, the others' too: on a hero this machine does not play, it does nothing but be seen. As things
come into sight and go out of it (or are destroyed), the object's behaviours hear
`on_see(object)` and `on_lose_sight(object)`. `in_sight` lists what it
sees, `sees?(object)` asks about one, and `last_seen(object)` is where it last saw it: where to
search.

**Hearing** is a radius. A game makes a noise with `Perception.noise`:

```ruby
Perception.noise(at: transform.position)                               # a footstep
Perception.noise(at: muzzle.transform.position, loudness: 3.0, source: game_object)  # a shot
```

Every Perception no further than its `hearing` times the noise's `loudness` hears it, walls or
not; the noise's `source` does not hear itself. Its object's behaviours hear
`on_hear(noise)`, with the noise's `position`, `loudness`, `source` and `distance` from them.
`Perception.noise` returns the objects that heard it.

| Setting | Meaning | Default |
|---|---|---|
| `sight` | meters it sees | 15 |
| `fov` | degrees of the whole cone, up to 360 | 120 |
| `eyes` | meters above its origin it looks from | 1.6 |
| `aim` | meters above a watched object's origin it looks at | 1 |
| `hearing` | meters a noise of loudness 1 is heard from | 10 |
| `watch` | the tag of what it looks for | the heroes |

Each setting is also a Blender property of the object. One that is not a number from 0 up
raises as the behaviour starts. While the game is paused it notices nothing new.

## Daily schedules

`GameClock` is the time of day in the game's world. It starts at 06:00 of day 1 and runs on game
time, so it stands still while the game is paused. Unless told, a game minute goes by each
second, and a day takes 24 minutes. It goes on across levels.

```ruby
GameClock.to_s                        # "06:30"
GameClock.hour                        # 6.5
GameClock.day                         # 1
GameClock.set(hour: 21, minute: 30)   # night falls (day: too, when given)
GameClock.speed = 120.0               # game seconds each second: a day in 12 minutes (0 stops it)
Save.set(:clock, GameClock.state)     # saving it is the game's ...
GameClock.restore(Save.get(:clock))   # ... and so is restoring it
```

A `Schedule` is an NPC's day: what it does from what time. Each entry lasts until the next, and
the last one lasts through midnight until the first:

```ruby
baker.add_component(Schedule, plan: { "06:00" => :bake, "12:30" => :sell, "20:00" => :sleep })
baker.add_component(Schedule, plan: "06:00 bake, 12:30 sell, 20:00 sleep")  # as a Blender property
```

`schedule.current` is the activity it is time for. As the hour of another one comes, the object's
behaviours hear `on_schedule(activity)`, and its Brain the `:schedule` event. A transition
`to: :scheduled` goes to the state named after the activity it is time for:

```ruby
class Baker < Brain
  state :bake
  state :sell
  state :sleep
  state :flee
  transition from: %i[bake sell sleep], to: :scheduled, on: :schedule   # follows the day
  transition from: :any, to: :flee, on: :hear
  transition from: :flee, to: :scheduled, after: 10.0                   # back to what it is time for
end
```

A transition to `:scheduled` is not taken when the brain has no state of that activity's name,
and it never leads to the state the brain is in. A plan that is not `"HH:MM activity"` entries
raises as the behaviour starts.

## Head look-at

An NPC's head can turn toward something, on top of what its clips do. The engine turns the bone
after the clips and the scripts each frame, no further than a largest angle off the pose the clips
gave it, and eases in as it starts and out as it stops:

```ruby
animation.look_at(player)                                # the "Head" bone, at most 60 degrees
animation.look_at(Vector3.new(0, 2, 5), bone: "Neck", max_angle: 45.0)
animation.look_away                                      # back to the clips' pose
animation.looking?                                       # still turned, or on its way back
animation.bone("Head")                                   # the bone object, or nil
```

Call `look_at` every frame to follow something that moves. A bone is found under the character by
its name, or by that name after a rig's prefix (`"mixamorig:Head"` is a `"Head"`). Its forward is
the way the character faced when it started to look.

A `HeadLook` behaviour does this on its own: each frame it looks at the eyes of the nearest hero in
range and in front of it, or at its `target` (an object or a point) when the game sets one.

| Setting | Meaning | Default |
|---|---|---|
| `bone` | the name of the head's bone | `"Head"` |
| `range` | meters to a hero | 4 |
| `angle` | degrees off its forward a hero may be at, and the most the head turns | 70 |
| `aim` | meters above a hero's origin it looks at | 1.6 |

Look-at is what each machine sees: a HeadLook runs on every machine, each toward the heroes it
sees.

## Over the network

Only the host thinks and perceives: on a client, a `Brain` and a `Perception` do nothing, and a
noise made there is heard by nobody. The host moves the NPC, and the players see it move through
its [NetworkSync](multiplayer.md). Each machine keeps its own `GameClock`: the host tells the
others the time, with a message of the game's own. An NPC's `on_interact` runs on the host already (see
[Interaction over the network](interaction.md)).
