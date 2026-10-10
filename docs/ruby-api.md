# Ruby API

| Object | Highlights |
|--------|-----------|
| `Behaviour` | `start`, `update(dt)`, `fixed_update(dt)`, `on_trigger_enter/exit(other)`, `on_collision_enter/exit(other)`, `on_destroy`; helpers `game_object`, `name`, `transform`, `character`, `rigidbody`, `vehicle`, `props`, `prop`, `find`, `find_all`, `destroy`, `get_component` |
| `GameObject` | `name`, `tag`, `tag?`, `props`, `alive?`, `destroy`, `visible` / `visible=`, `behaviours`, `parent`, `children`, `transform`, `character`, `rigidbody`, `vehicle`, `get_component(Klass)`, `add_component(Klass, speed: 2.0)`, `animation_offset(=)` (seconds a spawned copy's animations run ahead of the original's) |
| `Transform` | `position(=)`, `local_position(=)`, `rotation(=)` (Euler degrees), `rotate`, `look_at`, `translate`, `scale(=)`, `forward`, `right`, `up` |
| `CharacterController` | `move(velocity)`, `jump(speed)`, `grounded?`, `velocity` |
| `Rigidbody` | `velocity(=)`, `add_impulse(vector)` |
| `Vehicle` | `drive(throttle:, steering:, brake:, handbrake:)`, `stop`, `speed` (m/s, signed), `velocity` |
| `Physics` | `raycast(origin, direction, max_distance = 1000, ignore = nil)` → `{object:, point:, normal:, distance:}` or `nil` |
| `Input` | `axis(:horizontal / :vertical)`, `down?(:space)`, `pressed?`, `released?`, `action?(:jump)`, `action_pressed?` |
| `Time` | `delta`, `fixed_delta`, `elapsed`, `frame` |
| `Scene` / `Camera` | `Scene.find(name)`, `Scene.find_all(tag)`, `Scene.objects`, `Camera.main` |
| `SceneScript` | `setup`, `find`, `find_all`, `find!` (see *Setting a scene up in Ruby*) |
| `Scene.spawn` | `Scene.spawn("Bullet", position: v, rotation: euler)` copies a `prefab = true` template (hidden and inert in the game) with its physics and scripts; copies of animated characters move like the original |
| `Audio` | `Audio.play(:jump, volume:, pitch:)`, `Audio.music(:theme, loop: true)`, `Audio.stop_music`, `Audio.volume = 0.8` (files in `sounds/`) |
| `UI` / `Debug` / `Game` | `UI.text(str, x:, y:, size:, color:)`, `Debug.log/warn/error`, `Game.quit`, `Game.screenshot(name)`, `Game.load_scene("level2")` (`scenes/level2.glb`; `"main"` is `scene.glb`; Ruby globals survive), `Game.scene_name` |
| `Vector3` | arithmetic, `dot`, `cross`, `length`, `normalized`, `lerp`, `flat`, `with(y: 0)`, `distance_to` |

### Setting a scene up in Ruby

Behaviours can be attached in Blender (`script = "coin.rb"`) or from Ruby. A scene's setup script
is `scripts/<scene>_scene.rb` (`main_scene.rb` for `scene.glb`, `level2_scene.rb` for
`scenes/level2.glb`); its `setup` runs when the scene loads, before any behaviour starts:

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

A behaviour reads its settings with `prop(:speed, default)`: what `add_component` was given, else
the object's Blender property of that name, else the default. So the scene file can stay about
what is there and where, and Ruby about what it does. Scripts never `require` each other: the
first use of a class (`Patrol`) loads `scripts/patrol.rb`, behaviour or not.

Script errors are logged with a backtrace and only disable the failing behaviour; the game keeps running.
Assigning `transform.position` to a physics object teleports it (moving bodies also stop); changing
its rotation keeps its velocity. Vehicle wheel meshes must have their rotation applied (axle along X).

