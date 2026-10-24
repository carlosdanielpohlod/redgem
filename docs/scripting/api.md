# API reference

The classes and methods scripts use most. For how they fit together, see [Behaviours](behaviours.md)
and [Gameplay recipes](gameplay.md).

| Object | Highlights |
|--------|-----------|
| `Behaviour` | `start`, `update(dt)`, `fixed_update(dt)`, `on_trigger_enter/exit(other)`, `on_collision_enter/exit(other)`, `on_destroy`; helpers `game_object`, `name`, `transform`, `character`, `rigidbody`, `vehicle`, `props`, `prop`, `find`, `find_all`, `destroy`, `get_component`; timers `after(seconds) { }`, `every(seconds) { }`, `tween(seconds, ease: :smooth) { |t| }`, `cancel_timers` |
| `GameObject` | `name`, `tag`, `tag?`, `props`, `alive?`, `destroy`, `visible` / `visible=`, `behaviours`, `parent`, `children`, `transform`, `character`, `rigidbody`, `vehicle`, `get_component(Klass)`, `add_component(Klass, speed: 2.0)`, `tint(=)` (multiplies its colors: `[1.0, 0.5, 0.5]`), `light` (the light it shines with, or `nil`), `animation` (its own clips, see `Animator`), `animation_offset(=)` (seconds a spawned copy's animations run ahead of the original's) |
| `Transform` | `position(=)`, `local_position(=)`, `rotation(=)` (Euler degrees), `rotate`, `look_at`, `translate`, `scale(=)`, `forward`, `right`, `up` |
| `CharacterController` | `move(velocity)`, `jump(speed)`, `grounded?`, `velocity` |
| `Rigidbody` | `velocity(=)`, `add_impulse(vector)` |
| `Light` | `game_object.light`: `intensity(=)` (watts for a point or spot light, lux for the sun, as in Blender), `color(=)` (`:orange` or `[r, g, b]` from 0 to 255); a change shows from the next frame (see [Gameplay recipes](gameplay.md#light-that-flickers-and-fades)) |
| `Vehicle` | `drive(throttle:, steering:, brake:, handbrake:)`, `stop`, `speed` (m/s, signed), `velocity` |
| `Physics` | `raycast(origin, direction, max_distance = 1000, ignore = nil)` → `{object:, point:, normal:, distance:}` or `nil` |
| `Input` | `axis(:horizontal / :vertical)`, `down?(:space)`, `pressed?`, `released?`, `action?(:jump)`, `action_pressed?`; mouse: `mouse_x`, `mouse_y`, `mouse_dx`, `mouse_dy`, `mouse_wheel`, `mouse_down?(:left)`, `mouse_pressed?`, `mouse_released?`, `mouse_locked=`; gamepad: `gamepad?`, `gamepad_axis(:left_x)`, `gamepad_down?(:a)`, `gamepad_pressed?`, `gamepad_released?`; `text` (typed this frame); `touch_controls=` (the on-screen stick and buttons of a phone) |
| `Time` | `delta`, `fixed_delta`, `elapsed`, `frame`, `scale(=)` (0 pauses the game) |
| `Scene` / `Camera` | `Scene.find(name)`, `Scene.find_all(tag)`, `Scene.objects`, `Camera.main`, `Camera.screen_ray(x, y)` → `[origin, direction]`, `Camera.fov(=)` (degrees), `Camera.motion_blur(=)` (0..1: the picture streaks away from its center, as when moving fast) |
| `SceneScript` | `setup`, `find`, `find_all`, `find!` (see [Setting a level up in Ruby](behaviours.md#setting-a-level-up-in-ruby)) |
| `Scene.spawn` | `Scene.spawn("Bullet", position: v, rotation: euler, physics: true)` copies a `prefab = true` template (hidden and inert in the game) with its physics and scripts; copies of animated characters move like the original |
| `Scatter` | `Scatter.place("GrassTuft", count: 50_000, area: [corner, corner], ground: Scene.find("Ground"), seed: 7, scale: 0.7..1.4, align: true)` puts copies of an object's mesh (its scale, tint, drawing distances and `cast_shadow` too) in great numbers: only drawn, never objects of their own, the same places for the same seed on every machine. Each stands where a ray straight down meets `ground` (none where it misses it); without `ground`, on the area's bottom. `align: true` leans them with the slope. How they look: `tints: [[r, g, b], ...]` (each copy a color along the list), `tint_variation: 0.1` (brighter or darker by up to that share), `fade: 8.0` (meters over which they fade out before the template's `visible_to` instead of popping), `normals_up: 0.7` (grass lit as the ground it stands on). `lods: [["TreeMid", 40.0], ["TreeFar", 120.0]]` replaces the mesh at increasing distances (see [distance LODs](../levels.md#scatter-distance-lods)). Returns a set: `count` (how many were placed), `remove`; `Scatter.clear` removes them all. A scene holds at most 262,144 (see [Levels](../levels.md#grass-forests-and-cities)) |
| `Particles` | `Particles.burst(position, count:, color:, alpha:, glow:, lit:, cast_shadow:, soft:, direction:, spread:, speed:, life:, size:, gravity:, image:, frames: [columns, rows], fps:, spin:)` → how many were added (`size`, `color` and `alpha` may be lists of up to 4 values over the particles' life: see [Gameplay recipes](gameplay.md#over-their-life)), `Particles.count`, `Particles.clear` |
| `Animation` | `Animation.names`, `play(name, loop: true, speed: 1.0, restart: false)`, `pause(name)`, `stop(name)`, `playing?(name)`, `time(name)`, `duration(name)`, `seek(name, seconds)` |
| `Animator` | `game_object.animation` (or `animation` in a behaviour): `play(name, loop: true, speed: 1.0, fade: 0.0, restart: false)`, `mix(name, weight: 1.0, loop: true, speed: 1.0)`, `unmix`, `pause`, `follow_scene(fade: 0.0)`, `clip`, `playing?`, `time`; declarative [animation graphs](animation.md): `use(graph)`, `set(parameter, value)`, `state` |
| `Network` | `host(port: 7777, name:, visible: true, room: false)`, `join(address, port: 7777)` / `join(room: "K7QM3P")` / `join(address, room: "K7QM3P")` (a room on a dedicated server), `relay=`, `room`, `search`, `searching?`, `stop_search`, `games`, `discovery_port=`, `leave`, `online?`, `connecting?`, `host?`, `server?` (this machine is a dedicated server: it plays nobody), `id`, `peers`, `players` (the peers without a dedicated server), `port`, `emit(name, data = nil, to: :all / :host / id)`, `on(name) { \|data, from\| }`, `on_connect`, `on_disconnect`, `on_join { \|peer\| }`, `on_leave { \|peer\| }`, `join(address, auth: { token: })`, `authenticate { \|peer, auth, decide\| }` (`decide.accept(identity:, name:)`, `decide.deny(reason)`), `identity(peer)`, `player_name(peer)`, `on_fail { \|reason\| }` (see [Online services](online-services.md)), `spawn(template, owner: Network.id, position:, rotation:)`, `allow_spawn(*templates)` (see [Multiplayer](multiplayer.md)) |
| `NetworkSync` | A behaviour: `add_component(NetworkSync, owner: peer_id, key: "name", interpolate: true, delay: nil, animation: true)` (its own animation goes along: the others see the same clips, from the same moment) (see [Multiplayer](multiplayer.md#objects-in-the-same-place-everywhere)), `mine?`, `owner`, `key`, `assign(owner:, key:)` |
| `Save` | `Save.get(key, default = nil)`, `Save[key]`, `Save[key] = value` (numbers, texts, true/false, `Vector3`, arrays and hashes of those; hashes come back with string keys; `nil` removes the key), `Save.update(key, default = nil) { \|value\| new_value }` (reads and writes the key with nothing in between: on a dedicated server, what changes a value another room may change too), `Save.delete(key)`, `Save.clear`, `Save.keys`; on a dedicated server, the server's own (see [Online services](online-services.md#saving-on-a-server)) |
| `HTTP` | `HTTP.get/post/put/patch/delete(url, json:, body:, headers:, timeout: 15) { \|response\| }`; `response.ok?`, `status`, `headers`, `body`, `json`, `error` (see [Online services](online-services.md#http)) |
| `Audio` | [Positional sources](#positional-audio): `Audio.play_at`, `listener`, `move`, `playing?`, `stop`, `stop_sources`; `Audio.play(:jump, volume:, pitch:)`, `Audio.music(:theme, loop: true, volume: 0.7)`, `Audio.stop_music`, `Audio.music_pitch = 1.2`, `Audio.music_volume = 0.5` (change them every frame for an engine's drone), `Audio.volume = 0.8` (files in `sounds/`) |
| `UI` / `Debug` / `Game` | `UI.text(str, x:, y:, size:, color:, alpha:, align:)`, `UI.rect(x:, y:, width:, height:, color:, alpha:, radius:)`, `UI.image(name, x:, y:, width:, height:)`, `UI.button(label, x:, y:, width:, height:)` → clicked?, `UI.text_field(value, x:, y:, width:, height:, max_length:, secret:)` → the new value (`secret: true` shows stars); for settings screens `UI.toggle(label, on, x:, y:, width:, height:, focused:)` → on?, `UI.choice(label, choices, index, ...)` → the index chosen, `UI.slider(label, value, min:, max:, step:, ...)` → the value, and `UI::Focus.new` (`update(count)` moves it with the arrows or the D-pad, `at?(index)` is passed as `focused:`; a focused widget is worked by Enter or a gamepad's A, and left and right change it), `UI.width`, `UI.height`, `UI.text_width(str, size:)`, `Debug.log/warn/error`, `Game.quit`, `Game.stop_recording` (ends a video being recorded with `--record`, see [Building and sharing](../shipping.md#recording-a-video); the game goes on), `Game.load_scene("level2")` (`scenes/level2.glb`; `"main"` is `scene.glb`; Ruby globals survive), `Game.scene_name`, `Game.title`, `Game.version` (the `version` in `game.json`, such as `"1.2.0"`; `nil` when it gives none, see [Building and sharing](../shipping.md#icon-version-and-splash-picture)) |
| `GameMenu` | A behaviour: title screen, pause menu, end screen and settings screen (window mode, resolution, render scale, vsync, frame cap, quality, master volume; it keeps them when it is left). Up and down (arrows, D-pad) move the focus, Enter or a gamepad's A picks. `GameMenu.instance`, `show(:pause)`, `show_settings`, `resume`, `restart`, `finish("You win!")`, `open?`, `screen`, `screens` (`:title`, `:pause`, `:over`, `:settings`, and the game's own), `button_box(index)`; `GameMenu::Screen.new(heading:, options:, back:)`, `add_option(label, index:) { }`; settings `title`, `start_open`; its labels are the engine's texts `game.menu.*`, which a game's `locales/` files may change (see [Texts in several languages](i18n.md#the-engines-own-texts)); never opens on a dedicated server (see [Gameplay recipes](gameplay.md#pause-and-the-flow-between-screens)) |
| `Settings` | How the player chose the game to be shown and heard; each change takes effect at once, `Settings.save` keeps them for the next runs (only in the built game a player plays: in the editor's Play, `redgem run`, a server's rooms and tests they last for the run, and `save` returns false) (defaults: `game.json`, see [Building and sharing](../shipping.md#notes)). `window_mode(=)` (`:windowed`, `:fullscreen`, `:borderless`), `fullscreen?`, `fullscreen =`, `resolution(=)` (`[1920, 1080]`), `resolutions` (the monitor's: `[[w, h], ...]`), `render_scale(=)` (0.5 to 2: the world drawn at that share of the window's size, the interface at the window's own), `vsync(=)`, `fps_cap(=)` (`nil`: none), `quality(=)` (`:low`, `:medium`, `:high`, `:ultra`, `:custom`: the most a level's environment gets of what it asks for, never more), `quality_details(=)` (`{ msaa: 4, antialias: true, shadow_cascades: 2, shadow_distance: 0.5, scatter_density: 0.5, scatter_distance: 0.5, sky_detail: 0.5, ambient_occlusion: 1.0, bloom: 1.0 }`: setting some makes the quality `:custom`), `volume(=)` (the player's volume, 0 to 1: it multiplies the game's own `Audio.volume`, and each keeps its own) |
| `I18n` | `I18n.t("hud.coins", count: 3)` → the text of a key in the language in use, its `{count}` filled (see [Texts in several languages](i18n.md)), `I18n.locale` (`:en`), `I18n.locale = :pt`, `I18n.available_locales` (the languages of the game's `locales/` files) |
| `Vector3` | `Vector3.new(x, y, z)`, `Vector3.zero`, `one`, `up`, `down`, `left`, `right`, `forward`, `back`; arithmetic, `dot`, `cross`, `length`, `normalized`, `clamped(max_length = 1.0)`, `lerp`, `flat`, `with(y: 0)`, `distance_to`, `approx?`, `to_a`; in place, for math done thousands of times a frame: `set(x, y, z)`, `add!(v)`, `sub!(v)`, `scale!(k)`, `normalize!` (they change the vector and hand it back: `dup` one you got from someone else first) |

`==` compares the numbers exactly, and equal vectors are one key of a `Hash` (`eql?` and `hash`
agree with it). As with an `Array`, a vector used as a key must not be changed in place
(`set`, `add!`...) while the `Hash` holds it: it would no longer be found by its value. Use a
`dup` as the key. The scene keeps positions, rotations and scales in 32-bit floats, so a vector
that goes through it comes back rounded: `transform.position = v` and then
`transform.position == v` can be false. Compare what came back from the scene with
`approx?(other, tolerance)`.

Script errors are logged with a backtrace and only disable the failing behaviour; the game keeps running.
Every native takes finite numbers a float can hold: infinity, NaN (`0.0 / 0.0`) or a number too
large (`1e300`) raises an `ArgumentError` instead of reaching the scene. So does `Vector3`
arithmetic whose result would leave that range, and dividing a `Vector3` by zero; a vector changed
in place (`add!`, `scale!`...) is left as it was when that happens. A setting with a range raises outside it and says the range
(`Camera.motion_blur must be from 0 to 1, not 3`); nothing is quietly clamped, except
`Vehicle#drive`'s inputs, which an analog stick may push a little past full.
Assigning `transform.position` to a physics object teleports it (moving bodies also stop); changing
its rotation keeps its velocity. Vehicle wheel meshes must have their rotation applied (axle along X).


## Positional audio

`Audio.play_at` plays an independent sound from `sounds/`, with world positions in meters.
It returns a source id, or `nil` when the sound, device or one of the 64 voices is unavailable.
Sources use stereo panning and inverse-distance volume, fading smoothly to silence at `max_distance`.
Stereo files are downmixed before positioning. Nonlooping effects use buffered playback; loops stream.

```ruby
Audio.listener([0, 0, 0], [0, 0, -1], [0, 1, 0])
source = Audio.play_at(:waterfall, [10, 0, -5], loop: true,
                       volume: 0.8, pitch: 1.0,
                       reference_distance: 2.0, max_distance: 60.0)
Audio.move(source, [8, 0, -5]) if source
Audio.stop(source) if source
```

Update `Audio.listener(position, forward, up)` when the camera moves, and `Audio.move(id, position)`
when a source moves. The listener defaults to the origin, looking along negative Z, with Y up.
Its forward and up directions must be nonzero and nonparallel. Vectors may be arrays or `Vector3`.
These calls do not attach a source to an object or camera automatically.

`volume` defaults to 1 and must be nonnegative; `pitch` defaults to 1 and must be from 0.01 to 8.
The default full-volume `reference_distance` is 1 meter and the silent `max_distance` is 50 meters;
both must be finite, with `0 < reference_distance < max_distance`. `loop` defaults to false.
`Audio.playing?(id)` reports whether a source is playing. `Audio.stop(id)` stops that source;
`Audio.stop_sources` releases all positional sources, leaving flat effects and music playing.
Finished sources release their resources automatically; changing the sounds folder clears them too.
Scene changes within the same game do not stop them automatically. Old ids are never reused.

## Missing images

Missing or unreadable images show an opaque magenta-and-black checkerboard, with a warning.
This applies to declared scene textures, `UI.image` and particle images. A material that never
specified a texture keeps its plain color. Refused paths still cannot read outside the game's folder.

## Game fonts and Unicode

The built-in UI font adds glyphs on demand for text such as `€`, Polish and Turkish letters,
Greek and Cyrillic. To use another face or a script it does not cover, put a TrueType `.ttf` or
OpenType `.otf` file in the game's `fonts/` folder and select it:

```ruby
UI.font = :custom  # fonts/custom.ttf or fonts/custom.otf
UI.text("€ Żółć ş Ω Ж", x: 20, y: 20, size: 28)
width = UI.text_width("€ Żółć ş Ω Ж", size: 28)
UI.font = nil      # return to the built-in face
```

A name may include a subfolder or extension. It must stay inside `fonts/`; links and paths outside
the game are refused. Font files are included when the game is built. `UI.font` returns the selected
name, or `nil` for the built-in face. Text captures the selected face when queued, so several faces
can appear in one frame. Measurement and center/right alignment use the selected face too, in both
the player and the editor's Play view. Widgets that draw through `UI.text` use the same selection.

Missing, refused or unreadable faces fall back to the built-in font, with a warning. Unsupported
glyphs use `?`; CJK needs a font containing those characters. Each face keeps at most 2048 requested
codepoints; the cache holds four named game faces beside the built-in face, with at most 32 MB per
font file. Changing games clears the named-face cache. New glyphs rebuild the face's atlas only
when first requested. Text is drawn as individual glyphs: bidirectional layout, complex-script
shaping and automatic fallback chains are not implemented.
