# Animation graphs

`Animation::Graph` describes named states and transitions for one character. Install it with
`game_object.animation.use(graph)`. A graph can be reused: each character owns its clocks and
parameter values. Installation validates the whole definition before changing the character.

```ruby
graph = Animation::Graph.new(initial: :grounded, parameters: {speed: 0.0, grounded: true})
graph.blend_1d(:grounded, parameter: :speed,
               clips: [[0.0, :Idle], [1.5, :Walk], [5.0, :Run]])
graph.clip(:airborne, :Jump, loop: false)
graph.transition(:grounded, :airborne, parameter: :grounded, equals: false, fade: 0.15)
graph.transition(:airborne, :grounded, parameter: :grounded, equals: true, fade: 0.2)
animation.use(graph)

# In the character's update:
animation.set(:speed, velocity.length)
animation.set(:grounded, grounded?)
```

Declare every parameter and its default in `parameters:`. Values are booleans or finite numbers;
a parameter keeps its declared type. Blend parameters must be numeric. Clips must exist in the
loaded scene, state names must be unique, and every state must be reachable from `initial`.
For numeric conditions use exactly one of `equals:`, `above:` or `below:`; comparisons are strict
for `above` and `below`. For booleans use `equals:`. The first matching transition in declaration
order wins, and at most one transition is taken per frame.

One-dimensional blend points must have strictly increasing finite thresholds. Outside their range, the nearest
endpoint plays alone. Inside, the two neighboring clips mix linearly. Clips share normalized
phase; their weighted duration determines the cycle length. A missing channel contributes the
character's rest pose. Rotations mix normalized quaternions on the same hemisphere.
Installation and parameter updates reject synchronized clip speeds outside the representable
float range, preserving the active controller and its previous parameter values.

For movement on two axes, use a complete rectangular grid. Points can be in any order; each
combination of the distinct x and y coordinates must appear exactly once, with at least two
coordinates per axis. Both parameters must be distinct, declared numbers. Missing corners,
duplicate points, unknown clips and nonfinite coordinates are rejected at installation.

```ruby
graph = Animation::Graph.new(initial: :move, parameters: {strafe: 0.0, forward: 0.0})
graph.blend_2d(:move, parameters: [:strafe, :forward], clips: [
  [[-1.0, -1.0], :BackLeft], [[1.0, -1.0], :BackRight],
  [[-1.0, 1.0], :ForwardLeft], [[1.0, 1.0], :ForwardRight]
])
animation.use(graph)
animation.set(:strafe, sideways_speed)
animation.set(:forward, forward_speed)
```

The four corners of the enclosing cell mix bilinearly. Outside the grid, each axis clamps to
its nearest endpoint. Zero-weight corners are omitted; shared phase and weighted cycle duration
work as for one-dimensional blends. The evaluated tracks are replicated by `NetworkSync`.
The larger snapshots use protocol version 2; machines with different protocols cannot join.

`clip`, `blend_1d` and `blend_2d` accept `loop:` and `speed:`. Negative speed plays backwards; a
nonlooping state holds its endpoint. State fades keep the source and destination playing and
use at most eight clips (four per side for two-dimensional states). A fade completes before another transition is evaluated at the next
frame boundary; changing parameters during a fade does not interrupt it.

`animation.state` returns the current destination state's name as a String, including during a
fade, or `nil` under manual control. `clip` and `time` refer to the dominant destination clip;
equal weights choose the earlier blend point (grid points are ordered by y, then x). `pause` freezes the graph's clocks and fade.
Calling `use` again installs a fresh controller with its defaults.

`mix` and `unmix` retain the graph and affect the separate overlay as usual. A valid manual
`play` or `follow_scene` detaches the graph immediately. Fades across this control boundary are
not supported; fades between graph states are. An invalid manual clip request preserves the
active graph.

`NetworkSync` sends the evaluated clips, times, weights and speeds, including remaining fade
time. Receivers advance those tracks without installing a graph or evaluating its conditions.
Only the owner changes parameters. Every machine needs the same clips; malformed or unknown
graph tracks are ignored without replacing the active controller. Graphs currently support
flat clip states, one-dimensional blends and rectangular two-dimensional grids; there are no
hierarchical trees or irregular point-cloud blends.

## Events

A character's own clips can call a block at a moment of a clip: a footstep, the frame a sword
hits, the start and end of an attack window.

```ruby
class Fighter < Behaviour
  def start
    animation.on(:Attack, at: 0.4) { |event| strike }        # 40% into the clip
    animation.on(:Attack, seconds: 0.7) { @window = false }   # or seconds into it
    @steps = animation.on_marker(:footstep) { Audio.play(:step) }
  end
end
```

`at:` is normalized, from 0 (the clip's start) to 1 (its end); `seconds:` is converted with the
clip's duration. `on` and `on_marker` return a listener that `animation.off(listener)` removes.
The block is given an `Animation::Event`: `game_object`, `clip`, `name` (the marker's, or nil)
and `at`. Blocks run in the scripts' update phase, before the behaviours' `update`, never while
clips are being evaluated; an error in one is reported and the game goes on.

Each moment fires exactly once every time the clip's time passes it:

- a looping clip fires it on every lap, including when one frame wraps round past the end;
- a long frame fires every moment it went past, in the order the clip plays them, lap after lap
  (at most eight laps per frame);
- a clip played backwards fires its moments from the end towards the start;
- a clip played once fires a moment at `at: 1.0` when it reaches the end, and nothing while it
  holds its last pose; one that starts over (`restart: true`, `seek`) passes its moments again.

Which clips fire:

- the clip a character plays, or fades to: the clip it fades from stops firing as the fade
  begins, and the new one fires from its first frame (a moment at 0 included);
- in a blend (`blend_1d`, `blend_2d`) only the clip with the most weight fires; blended clips
  share their phase, so a footstep at the same moment in Walk and Run fires once, from whichever
  leads;
- on a state transition the destination state fires from its start; the source state is silent
  during the fade;
- a clip mixed over the character (`mix`) fires while its weight is at least 0.5.

The scene's own clips (`Animation`) do not fire events, nor do graphs received from another
machine (`NetworkSync`): their owner's machine hears them.

### Markers authored in Blender

Blender's glTF exporter writes an action's custom properties to the animation's `extras` (with
**Include > Custom Properties** checked). RedGem reads one string property, `redgem_events`, as
a comma-separated list of `name@time` markers, time normalized from 0 to 1:

```
redgem_events = "footstep@0.25, footstep@0.75, hit@0.4"
```

Add it to the action in the Action editor's sidebar (Custom Properties). Timeline and pose
markers are not exported by Blender's glTF exporter, so they cannot be used. Entries that are not
`name@time` with a time from 0 to 1 are skipped with a warning when the scene loads.
`animation.on_marker(:footstep)` then listens for every marker of that name in any of the
character's clips.

## Root motion

Root motion lets a clip move the character instead of the mesh: an attack that lunges, a walk
cycle that covers the ground it shows.

```ruby
animation.root_motion = true                      # every clip of this character
animation.play(:Lunge, root_motion: true)         # or one clip (root_motion: false stops it)
```

The root of a clip is the highest node it moves or turns below the character's own object
(usually the skeleton's root bone). Each frame, its horizontal travel and its turning around the
world's up axis (yaw) go to the character, and the root keeps its rest place and heading
horizontally, so the mesh stays over the character's collider. Its vertical motion stays in the
pose: a jump clip lifts the mesh, not the character, which gravity and the ground keep in
place. A loop that wraps round adds a whole lap's travel each time.

A character with a `CharacterController` (`body = character`) moves through the physics: the
travel is added to its next step on top of `character.move`, and it slides along walls and
climbs steps as it would anyway (a frame late). Any other object is moved there directly.

When clips blend (a fade, a blend tree, a transition) the motion is their travel weighted as they
are shown. A clip mixed over the character (`mix`) adds none. Root motion only comes from the
character's own clips, not the scene's; a clip that moves the character's own object, not a node
under it, has no root motion.
