# API reference

The classes and methods scripts use most. For how they fit together, see [Behaviours](behaviours.md)
and [Gameplay recipes](gameplay.md).

| Object | Highlights |
|--------|-----------|
| `Behaviour` | `start`, `update(dt)`, `fixed_update(dt)`, `on_trigger_enter/exit(other)`, `on_collision_enter/exit(other)`, `on_destroy`; helpers `game_object`, `name`, `transform`, `character`, `rigidbody`, `vehicle`, `props`, `prop`, `find`, `find_all`, `destroy`, `get_component`; timers `after(seconds) { }`, `every(seconds) { }`, `tween(seconds, ease: :smooth) { |t| }`, `cancel_timers` |
| `GameObject` | `name`, `tag`, `tag?`, `props`, `alive?`, `destroy`, `visible` / `visible=`, `behaviours`, `parent`, `children`, `transform`, `character`, `rigidbody`, `vehicle`, `get_component(Klass)`, `add_component(Klass, speed: 2.0)`, `animation_offset(=)` (seconds a spawned copy's animations run ahead of the original's) |
| `Transform` | `position(=)`, `local_position(=)`, `rotation(=)` (Euler degrees), `rotate`, `look_at`, `translate`, `scale(=)`, `forward`, `right`, `up` |
| `CharacterController` | `move(velocity)`, `jump(speed)`, `grounded?`, `velocity` |
| `Rigidbody` | `velocity(=)`, `add_impulse(vector)` |
| `Vehicle` | `drive(throttle:, steering:, brake:, handbrake:)`, `stop`, `speed` (m/s, signed), `velocity` |
| `Physics` | `raycast(origin, direction, max_distance = 1000, ignore = nil)` → `{object:, point:, normal:, distance:}` or `nil` |
| `Input` | `axis(:horizontal / :vertical)`, `down?(:space)`, `pressed?`, `released?`, `action?(:jump)`, `action_pressed?`; mouse: `mouse_x`, `mouse_y`, `mouse_dx`, `mouse_dy`, `mouse_wheel`, `mouse_down?(:left)`, `mouse_pressed?`, `mouse_released?`, `mouse_locked=`; gamepad: `gamepad?`, `gamepad_axis(:left_x)`, `gamepad_down?(:a)`, `gamepad_pressed?`, `gamepad_released?` |
| `Time` | `delta`, `fixed_delta`, `elapsed`, `frame`, `scale(=)` (0 pauses the game) |
| `Scene` / `Camera` | `Scene.find(name)`, `Scene.find_all(tag)`, `Scene.objects`, `Camera.main`, `Camera.screen_ray(x, y)` → `[origin, direction]`, `Camera.fov(=)` (degrees), `Camera.motion_blur(=)` (0..1: the picture streaks away from its center, as when moving fast) |
| `SceneScript` | `setup`, `find`, `find_all`, `find!` (see [Setting a level up in Ruby](behaviours.md#setting-a-level-up-in-ruby)) |
| `Scene.spawn` | `Scene.spawn("Bullet", position: v, rotation: euler)` copies a `prefab = true` template (hidden and inert in the game) with its physics and scripts; copies of animated characters move like the original |
| `Particles` | `Particles.burst(position, count:, color:, alpha:, glow:, direction:, spread:, speed:, life:, size:, gravity:)` → how many were added, `Particles.count`, `Particles.clear` |
| `Animation` | `Animation.names`, `play(name, loop: true, speed: 1.0, restart: false)`, `pause(name)`, `stop(name)`, `playing?(name)`, `time(name)`, `duration(name)`, `seek(name, seconds)` |
| `Save` | `Save.get(key, default = nil)`, `Save[key]`, `Save[key] = value` (numbers, texts, true/false), `Save.delete(key)`, `Save.clear`, `Save.keys` |
| `Audio` | `Audio.play(:jump, volume:, pitch:)`, `Audio.music(:theme, loop: true, volume: 0.7)`, `Audio.stop_music`, `Audio.music_pitch = 1.2`, `Audio.music_volume = 0.5` (change them every frame for an engine's drone), `Audio.volume = 0.8` (files in `sounds/`) |
| `UI` / `Debug` / `Game` | `UI.text(str, x:, y:, size:, color:, alpha:, align:)`, `UI.rect(x:, y:, width:, height:, color:, alpha:, radius:)`, `UI.image(name, x:, y:, width:, height:)`, `UI.button(label, x:, y:, width:, height:)` → clicked?, `UI.width`, `UI.height`, `UI.text_width(str, size:)`, `Debug.log/warn/error`, `Game.quit`, `Game.screenshot(name)`, `Game.load_scene("level2")` (`scenes/level2.glb`; `"main"` is `scene.glb`; Ruby globals survive), `Game.scene_name` |
| `Vector3` | `Vector3.new(x, y, z)`, `Vector3.zero`, `one`, `up`, `down`, `left`, `right`, `forward`, `back`; arithmetic, `dot`, `cross`, `length`, `normalized`, `clamped(max_length = 1.0)`, `lerp`, `flat`, `with(y: 0)`, `distance_to`, `approx?`, `to_a` |

Script errors are logged with a backtrace and only disable the failing behaviour; the game keeps running.
Assigning `transform.position` to a physics object teleports it (moving bodies also stop); changing
its rotation keeps its velocity. Vehicle wheel meshes must have their rotation applied (axle along X).

