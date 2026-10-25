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
