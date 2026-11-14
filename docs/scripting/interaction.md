---
description: "Interaction: things the player can use or talk to, the prompt, the :interact action, and interaction over the network."
---

# Interaction

Doors, levers, signs, chests and characters to talk to all work the same way: the player walks up
to one, a prompt shows at the bottom of the screen ("[E] Open"), and a key press (or a tap on the
prompt) uses it. Two behaviours do this:

- `Interactable`, on each thing that can be used;
- `Interactor`, on the player's hero.

```ruby
class MainScene < SceneScript
  def setup
    find!("Player").add_component(Interactor)
    find!("Door").add_component(Interactable, prompt: "door.open", range: 2.5)
    find!("Door").add_component(Door)
  end
end

class Door < Behaviour
  def on_interact(actor)              # actor: the object with the Interactor (the hero)
    transform.rotate(Vector3.new(0, 90, 0))
    get_component(Interactable).enabled = false   # used once
  end

  def on_focus(actor); end            # it became what the prompt is for
  def on_blur(actor); end             # it stopped being it
end
```

The prompt's text is a key of the game's translation files (see
[Texts in several languages](i18n.md)): `"door.open": "Open"` in `locales/en.json`. Without a
`prompt`, the engine's own "Interact" is shown.

## What the player can use

Every frame, the Interactor looks at the scene's Interactables and keeps those that are:

1. **enabled**;
2. **in range**: no further than `range` meters from the hero to the object;
3. **in front**, when the Interactable has an `angle`: no more than that many degrees off the way
   the hero faces, measured across the ground;
4. **in sight**: a ray from the hero's position to the object's hits nothing else first. Triggers
   never block it, what the hero carries (the objects under it) is seen through, and the object's
   own collider, or one of an object under it (a character's body), counts as reaching it.

Of those, the one with the highest `priority` wins, then the nearest. That one is the **focus**:
its behaviours hear `on_focus(actor)` when it becomes the focus, and `on_blur(actor)` when another
takes its place, it goes out of reach, or it is disabled.

| Setting | Default | What it does |
|---|---|---|
| `prompt` | `nil` ("Interact") | I18n key of the prompt's text |
| `range` | `2.0` | meters from the actor to the object |
| `priority` | `0` | of those in reach, the highest wins |
| `enabled` | `true` | `false`: never picked |
| `angle` | `nil` (any direction) | degrees off the actor's forward the object may be at |

Each setting can also be a Blender custom property of the object, and each can change while
playing: `interactable.enabled = false`, `interactable.range = 4.0`, `interactable.angle = nil`.
A range or an angle that is not a number from 0 up, or a priority that is not a number, raises an
`ArgumentError` where it is set (from a setting or a property: as the Interactable starts).

## The prompt and the :interact action

The prompt names what the `:interact` action is bound to: "[E] Open", or the gamepad's button
("[Y button] Open") while a gamepad is plugged in. `:interact` is one of the actions every game has
(E and a gamepad's Y), so it shows on the menu's controls page, and players can rebind it there; a
game binds it anew as any other action:

```json
{ "input": { "actions": { "interact": ["key:f", "pad:x"] } } }
```

With the on-screen touch controls showing (`Input.touch_controls = true`), the prompt shows the
text alone, and tapping it uses the focus. A click on it does the same with a mouse.

A game that draws its own prompt turns the engine's off and reads the focus:

```ruby
interactor = find!("Player").add_component(Interactor, prompt: false)
# in a HUD's update:
target = interactor.target
UI.text(interactor.prompt_label(target), anchor: :bottom, x: 0, y: -60) if target
```

`Interactor` settings: `pad` (whose `:interact` it reads, for a second local player: 0 unless
said) and `prompt` (`false` draws none). While a menu is open (`Time.scale = 0`) it neither looks
nor shows.

## Hearing of every use

Besides the target's own behaviours, the game can hear of every use, and of the focus moving, in
one place, for a quest log or a tutorial:

```ruby
Interaction.on(:interact) { |actor, target| Debug.log("#{actor.name} used #{target.name}") }
Interaction.on(:focus) { |actor, target| Audio.play(:hover) }
Interaction.on(:blur) { |actor, target| }
```

The target's behaviours hear first, as the engine calls its own callbacks (one that raises is
disabled and reported, and the others still hear), then the handlers. What `Interaction.on` registers lasts until
the next level is loaded, as `Network.on` does: register it in a scene script's `setup`.

A script can also use something itself, as the Interactor does when the key is pressed:
`Interaction.interact(actor, target)` (the target is an object or its Interactable) runs it when the
target is in the actor's reach, and returns whether it did. `Interaction.best_for(actor)` says what
an actor would pick now.

## Over the network

With the game over the network (see [Multiplayer](multiplayer.md)), every machine runs its own
hero's Interactor: the focus and the prompt are worked out where the player is. A use is not: a
client asks the host, which looks again (range, angle and sight, where it sees that client's hero)
and runs `on_interact` and the `:interact` handlers there. A modified client cannot use what its
hero is not near.

- The hero needs a `NetworkSync` that is its machine's (as every hero of a networked game has): the
  host knows it by that key, and believes only its owner.
- The target is found on the host by its `NetworkSync` key when it has one, by its name otherwise
  (of several Interactables with that name, the one in the hero's reach), and needs its
  Interactable on the host too: give every machine the same scene script.
- The host sees a client's hero a moment late, so it lets the hero be 1 meter past the range and
  30 degrees past the angle (`Interaction::NETWORK_SLACK`, `NETWORK_ANGLE_SLACK`). Its sight is
  checked from where the host sees the hero, with no slack.
- What the use does happens on the host. It tells the players what they need to see with
  `Network.emit` (the actor's owner is `actor.get_component(NetworkSync).owner`):

```ruby
def on_interact(actor)
  @open = true
  Network.emit(:door_opened, name)    # every other machine opens its copy
  open_here
end
```

Offline, and on the host itself, a use happens right away. `on_focus` and `on_blur` are only ever
heard on the machine whose hero it is.
