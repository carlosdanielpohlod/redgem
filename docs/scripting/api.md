---
description: "Every class and method of the scripting API, a line each (also: redgem api)."
---

# API reference

The scripting API of RedGem games: the classes and methods scripts use. Generated from docs/scripting/api.json, which tests keep in step with the engine.

| Class | What it is |
|---|---|
| [`Behaviour`](#behaviour) | A Ruby class attached to an object: the engine calls its methods as the game runs. |
| [`GameObject`](#gameobject) | An object of the scene, as scripts see it. |
| [`Transform`](#transform) | Where an object is, how it is turned and how big it is. |
| [`Vector3`](#vector3) | A point or a direction in meters: x, y, z. |
| [`Scene`](#scene) | The objects of the running scene. |
| [`SceneScript`](#scenescript) | Sets a level up from Ruby when it loads: scripts/<scene>_scene.rb. |
| [`Input`](#input) | Keyboard, mouse, gamepad and touch, read every frame. |
| [`Time`](#time) | The game's clock (added to Ruby's Time). |
| [`Physics`](#physics) | Questions to the physics world: rays, moving shapes and shapes in place. |
| [`Joint`](#joint) | What holds two bodies together, or one to the world (made by Physics.joint). |
| [`CharacterController`](#charactercontroller) | The body of an object with body = "character": walks, jumps, climbs steps. |
| [`Rigidbody`](#rigidbody) | The body of an object with body = "dynamic" or "kinematic" (or a vehicle's chassis). |
| [`Vehicle`](#vehicle) | The body of an object with body = "vehicle" and wheels. |
| [`Camera`](#camera) | The camera the game is seen through, and what it does on its own: follow, orbit, shake. |
| [`Camera::Orbit`](#cameraorbit) | What Camera.orbit returns: the camera's angles and distance around what it orbits, for scripts to read and change. |
| [`Light`](#light) | game_object.light: the light an object shines with. |
| [`Animation`](#animation) | The scene's own animations (exported from Blender), by name. |
| [`Animator`](#animator) | game_object.animation (animation in a behaviour): an object's own clips. |
| [`Animation::Graph`](#animationgraph) | Named states and transitions for one character (animation.use(graph)). |
| [`Audio`](#audio) | Sounds and music from the game's sounds/ folder. |
| [`UI`](#ui) | What a script draws over the game this frame: texts, rectangles, pictures, buttons. |
| [`UI::Focus`](#uifocus) | Which widget of a screen the keyboard and a gamepad work: up and down move it, wrapping. |
| [`Debug`](#debug) | Developer diagnostics: console messages, frame timings and immediate world drawing. |
| [`Game`](#game) | The game itself. |
| [`Settings`](#settings) | How the player chose the game to be shown and heard; a change takes effect at once. |
| [`GameMenu`](#gamemenu) | A behaviour: title screen, pause menu, end screen and settings screen with its controls page; arrows or the D-pad move the focus, Enter or A picks. |
| [`GameMenu::Screen`](#gamemenuscreen) | A screen of the GameMenu. |
| [`Particles`](#particles) | Short-lived sparks, smoke and dust. |
| [`Particles::Emitter`](#particlesemitter) | A continuous particle effect attached to an object, returned by Particles.emitter. |
| [`Scatter`](#scatter) | Great numbers of copies of a mesh (grass, trees, rocks), drawn only. |
| [`Save`](#save) | What the game keeps between runs. |
| [`HTTP`](#http) | Web requests; the answer comes to the block. |
| [`HTTP::Response`](#httpresponse) | What a web request's block is given. |
| [`I18n`](#i18n) | The game's texts in several languages (locales/*.json). |
| [`Network`](#network) | Games played together: on a local network, through a relay or on a dedicated server. |
| [`Network::Decision`](#networkdecision) | What an authenticate block decides with. |
| [`NetworkSync`](#networksync) | A behaviour: the same object in the same place on every machine. |

## Behaviour

A Ruby class attached to an object: the engine calls its methods as the game runs. See [behaviours.md](behaviours.md).

- `start`: called once, before the first update
- `update(dt)`: called every frame; dt is the seconds since the last one
- `fixed_update(dt)`: called 60 times a second, with the physics
- `on_trigger_enter(other)`: something entered this object's trigger, or it entered one
- `on_trigger_exit(other)`: something left this object's trigger
- `on_collision_enter(other)`: this object's body touched another
- `on_collision_exit(other)`: this object's body stopped touching another
- `on_destroy`: the object is being destroyed
- `game_object`: the object this behaviour is attached to
- `name`: the object's name
- `transform`: the object's Transform
- `character`: the object's CharacterController, or nil
- `rigidbody`: the object's Rigidbody, or nil
- `vehicle`: the object's Vehicle, or nil
- `animation`: the object's own Animator
- `props`: the object's custom properties, as a Hash
- `prop(key, default = nil)`: a setting: what add_component was given, else the object's property, else the default
- `find(name)`: the object called name, or nil
- `find!(name)`: find that stops with a clear error when the object is missing
- `find_all(tag)`: every object with that tag
- `destroy(target = game_object)`: removes an object (this one unless said) at the end of the step
- `get_component(klass)`: another behaviour of this object, or nil
- `after(seconds, &block)`: runs the block once, that many seconds from now
- `every(seconds, &block)`: runs the block every that many seconds
- `tween(seconds, ease: :smooth, &block)`: calls the block with t from 0 to 1 over that many seconds
- `cancel_timers`: stops every after, every and tween of this behaviour

## GameObject

An object of the scene, as scripts see it. See [behaviours.md](behaviours.md).

- `name`: its name
- `id`: its number in the running game
- `stable_id`: its persistent scene-file identity as a String; nil for runtime copies
- `capture_state`: a versioned checkpoint of local pose, tint and visibility, suitable for Save
- `restore_state(state)`: validates a checkpoint then restores pose and appearance; teleports physical descendants and returns self
- `tag`: its tag property
- `tag?(value)`: whether its tag is value
- `props`: its custom properties
- `alive?`: false once it was destroyed
- `destroy`: removes it at the end of the step
- `visible`: whether it is drawn
- `visible = value`: shows or hides it
- `behaviours`: its behaviours
- `parent`: the object it is under, or nil
- `children`: the objects under it
- `transform`: its Transform
- `character`: its CharacterController, or nil
- `rigidbody`: its Rigidbody, or nil
- `vehicle`: its Vehicle, or nil
- `light`: the Light it shines with, or nil
- `animation`: its Animator
- `animation_offset`: seconds its animations run ahead of the original's (a spawned copy)
- `animation_offset = seconds`: sets that offset
- `get_component(klass)`: one of its behaviours, or nil
- `add_component(klass, **settings)`: attaches a behaviour; settings are what its prop reads
- `tint`: the color its colors are multiplied by
- `tint = color`: multiplies its colors: [1.0, 0.5, 0.5]

## Transform

Where an object is, how it is turned and how big it is.

- `position`: its position in the world (Vector3)
- `position = vector`: moves it; a physics object is teleported
- `local_position`: its position relative to its parent
- `local_position = vector`: moves it relative to its parent
- `rotation`: its rotation, Euler degrees (Vector3)
- `rotation = degrees`: turns it to those Euler degrees
- `rotate(degrees)`: turns it by those Euler degrees
- `look_at(target, up = Vector3.up)`: turns it to face a point
- `translate(offset)`: moves it by an offset
- `scale`: its scale (Vector3)
- `scale = vector`: resizes it
- `forward`: the direction it faces (-Z)
- `right`: its right (+X)
- `up`: its up (+Y)

## Vector3

A point or a direction in meters: x, y, z.

- `Vector3.new(x, y, z)`: a vector
- `Vector3[x, y, z]`: the same as new
- `Vector3.zero`: (0, 0, 0)
- `Vector3.one`: (1, 1, 1)
- `Vector3.up`: (0, 1, 0)
- `Vector3.down`: (0, -1, 0)
- `Vector3.left`: (-1, 0, 0)
- `Vector3.right`: (1, 0, 0)
- `Vector3.forward`: (0, 0, -1)
- `Vector3.back`: (0, 0, 1)
- `Vector3.blender(x, y, z)`: a vector given in Blender's axes (Z up)
- `x`: its x
- `y`: its y
- `z`: its z
- `+ other`: sum
- `- other`: difference
- `-@`: the opposite vector
- `* number`: scaled
- `/ number`: divided
- `== other`: whether the numbers are exactly the same
- `dot(other)`: dot product
- `cross(other)`: cross product
- `length`: its length
- `length_squared`: its length squared (faster)
- `normalized`: the same direction, length 1
- `clamped(max_length = 1.0)`: the same direction, no longer than max_length
- `lerp(other, t)`: the point t of the way to other
- `flat`: the same with y = 0
- `with(x:, y:, z:)`: a copy with some parts changed: v.with(y: 0)
- `distance_to(other)`: the distance to another point
- `approx?(other, tolerance = 0.0001)`: whether two vectors are about the same
- `to_a`: [x, y, z]
- `set(x, y, z)`: changes it in place
- `add!(other)`: adds in place
- `sub!(other)`: subtracts in place
- `scale!(number)`: scales in place
- `normalize!`: normalizes in place

`==` compares the numbers exactly, and equal vectors are one key of a `Hash`. A vector used as a key must not be changed in place (`set`, `add!`...) while the `Hash` holds it: use a `dup` as the key. The scene keeps positions, rotations and scales in 32-bit floats, so a vector that goes through it comes back rounded: compare what comes back from the scene with `approx?(other, tolerance)`. The in-place methods change the vector and hand it back: `dup` one you got from someone else first. Arithmetic whose result would not fit a float, and dividing by zero, raise an `ArgumentError`; a vector changed in place is then left as it was.

## Scene

The objects of the running scene. See [gameplay.md](gameplay.md).

- `Scene.find(name)`: the object called name, or nil
- `Scene.find_by_id(identity)`: the current scene object with that persistent String/Symbol identity, or nil
- `Scene.find!(name)`: find that stops with a clear error when the object is missing
- `Scene.find_all(tag)`: every object with that tag
- `Scene.objects`: every object
- `Scene.spawn(template, position: nil, rotation: nil, physics: true)`: copies a prefab = true template, with its physics and scripts; returns the copy

## SceneScript

Sets a level up from Ruby when it loads: scripts/<scene>_scene.rb. See [behaviours.md#setting-a-level-up-in-ruby](behaviours.md#setting-a-level-up-in-ruby).

- `setup`: runs when the level loads, before any behaviour starts
- `find(name)`: the object called name, or nil
- `find!(name)`: find that stops with a clear error when the object is missing
- `find_all(tag)`: every object with that tag

## Input

Keyboard, mouse, gamepad and touch, read every frame. See [gameplay.md](gameplay.md).

- `Input.axis(name)`: :horizontal or :vertical, from -1 to 1 (keys, stick)
- `Input.down?(key)`: whether a key is held
- `Input.pressed?(key)`: whether a key went down this frame
- `Input.released?(key)`: whether a key went up this frame
- `Input.action?(name)`: whether a named action (game settings) is held
- `Input.action_pressed?(name)`: whether a named action started this frame
- `Input.action_released?(name, pad: 0)`: whether a named action ended this frame
- `Input.define_action(name, *bindings, keys: [], buttons: [])`: adds an action of the game's own (besides game.json's input section)
- `Input.define_axis(name, negative: [], positive: [], pad_axis: nil, invert: false)`: adds an axis of the game's own: define_axis(:steer, negative: ["key:a"], positive: ["key:d"])
- `Input.actions`: the names of the actions
- `Input.axes`: the names of the axes
- `Input.axis_stick(name)`: the stick that moves an axis (:left_x), the game's for every player; nil when none does
- `Input.bindings(name, pad: 0)`: what a player bound to an action: ["key:space", "pad:a"]
- `Input.bindings_changes`: a count that goes up whenever actions, axes or bindings may have changed
- `Input.bind(name, binding = nil, key: nil, button: nil, side: nil, pad: 0)`: binds a key or a button to an action; kept between runs
- `Input.unbind(name, binding = nil, key: nil, button: nil, side: nil, pad: 0)`: takes a binding off an action
- `Input.reset_bindings(name = nil, pad: nil)`: back to the game's own bindings (of one action, or all; of one pad, or all)
- `Input.capture_next(cancel: ["key:escape", "pad:b", "pad:back"], &block)`: the next key or button pressed, to the block: |binding, pad| (nil: called off)
- `Input.capturing?`: whether a capture waits for a key
- `Input.capturing_for?(block)`: whether a capture still waits to call that block (not once another took its place)
- `Input.cancel_capture`: calls a capture off
- `Input.mouse_x`: the pointer's x, in pixels
- `Input.mouse_y`: the pointer's y, in pixels
- `Input.mouse_dx`: how far the mouse moved this frame, across
- `Input.mouse_dy`: how far the mouse moved this frame, up and down
- `Input.mouse_wheel`: how far the wheel turned this frame
- `Input.mouse_down?(button)`: whether a mouse button (:left, :right, :middle) is held
- `Input.mouse_pressed?(button)`: whether a mouse button went down this frame
- `Input.mouse_released?(button)`: whether a mouse button went up this frame
- `Input.mouse_locked = locked`: hides the pointer and keeps it in the window (first-person cameras)
- `Input.mouse_locked?`: whether the pointer is locked
- `Input.pads`: the numbers of the gamepads plugged in
- `Input.gamepad?`: whether a gamepad is connected
- `Input.gamepad_axis(axis)`: :left_x, :left_y, :right_x, :right_y, :left_trigger, :right_trigger
- `Input.gamepad_down?(button)`: whether a gamepad button (:a, :b, :x, :y...) is held
- `Input.gamepad_pressed?(button)`: whether a gamepad button went down this frame
- `Input.gamepad_released?(button)`: whether a gamepad button went up this frame
- `Input.text`: what was typed this frame
- `Input.touch_controls = on`: shows the on-screen stick and buttons of a phone
- `Input.touch_controls?`: whether they are shown

Every action, axis, binding and gamepad call takes `pad:` (0 to 3: the player; 0 when left out). Actions and axes come from `game.json`'s `input` section and `define_action`/`define_axis`; a player's bindings are kept between runs, beside the game's save. See [Controls](gameplay.md#controls).

## Time

The game's clock (added to Ruby's Time).

- `Time.delta`: seconds since the last frame
- `Time.fixed_delta`: seconds of a physics step (1/60)
- `Time.elapsed`: seconds since the game started
- `Time.frame`: how many frames have run
- `Time.scale`: how fast game time runs (1 is normal)
- `Time.scale = value`: slows or speeds the game; 0 pauses it

## Physics

Questions to the physics world: rays, moving shapes and shapes in place. See [gameplay.md](gameplay.md).

- `Physics.raycast(origin, direction, max_distance = 1000, skip = nil, layers: nil, ignore: nil)`: the first thing a ray hits: {object:, point:, normal:, distance:}, or nil (skip: an object it sees through)
- `Physics.raycast_all(origin, direction, max_distance = 1000, layers: nil, ignore: nil)`: everything a ray hits, nearest first
- `Physics.sphere_cast(from, to, radius, layers: nil, ignore: nil)`: the first thing a ball moving from `from` to `to` touches, or nil
- `Physics.box_cast(from, to, size, rotation: nil, layers: nil, ignore: nil)`: the same for a box (its whole size; rotation in degrees)
- `Physics.capsule_cast(from, to, radius, height, rotation: nil, layers: nil, ignore: nil)`: the same for an upright capsule (height end to end)
- `Physics.overlap_sphere(center, radius, layers: nil, ignore: nil)`: the hits of everything inside a ball, nearest first
- `Physics.overlap_box(center, size, rotation: nil, layers: nil, ignore: nil)`: the same inside a box
- `Physics.overlap_capsule(center, radius, height, rotation: nil, layers: nil, ignore: nil)`: the same inside a capsule
- `Physics.layers`: the game's collision layers, as Symbols (game.json's physics layers)
- `Physics.joint(kind, object, other = nil, anchor: nil, other_anchor: nil, axis: nil, limits: nil, motor_speed: nil, motor_torque: nil, min: nil, max: nil, spring: nil, damping: nil, break_force: nil)`: joins two objects, or one to the world (other nil): :fixed, :hinge, :slider, :point or :distance

`layers:` is a layer's name or a list of them (every layer when nil); `ignore:` is an object or a list of objects to see through. Hits are `{object:, point:, normal:, distance:}`, each object once, nearest first, at most 64; triggers are never hit. A cast that starts inside something hits it at distance 0.

## Joint

What holds two bodies together, or one to the world (made by Physics.joint). See [gameplay.md](gameplay.md).

- `kind`: :fixed, :hinge, :slider, :point or :distance
- `object`: the object it holds
- `other`: the other object it holds, or nil for the world
- `alive?`: whether it still holds them
- `broken?`: whether it was pulled past its break force
- `remove`: lets go
- `enabled?`: whether it holds right now
- `enabled = flag`: lets go for a while, or holds again
- `break_force`: the pull (newtons) or twist (newton meters) it breaks at; 0 never breaks
- `break_force = newtons`: sets the pull it breaks at
- `motor(speed, torque)`: a hinge turns at that speed (degrees per second) with at most that torque; 0 torque lets it go
- `angle`: how far a hinge turned from where it started, in degrees

Points and axes are in world space; `anchor:` defaults to the object's position, `other_anchor:` (a distance joint's other end) to the other object's, `axis:` to up. A hinge's `limits:` are degrees [low, high] from where it starts (low -180 to 0, high 0 to 180), a slider's are meters along its axis; a hinge's `motor_speed:` needs a `motor_torque:`. A distance joint keeps between `min:` (0) and `max:` (how far apart the ends are) apart, springy with `spring:` (hertz) and `damping:`. Each kind refuses options it has no use for, and `motor` and `angle` raise for anything but a hinge. A joint needs a dynamic body at one end (one whose bodies were all made kinematic since holds nothing until one is dynamic again); it goes when either object is destroyed or the scene changes.

## CharacterController

The body of an object with body = "character": walks, jumps, climbs steps. See [gameplay.md](gameplay.md).

- `move(velocity)`: walks with that velocity (m/s) this frame
- `jump(speed)`: jumps up at that speed
- `grounded?`: whether it stands on something
- `velocity`: its velocity
- `layer`: its collision layer, a Symbol
- `layer = name`: moves it to another collision layer

## Rigidbody

The body of an object with body = "dynamic" or "kinematic" (or a vehicle's chassis). See [gameplay.md](gameplay.md).

- `velocity`: its velocity
- `velocity = vector`: sets its velocity
- `add_impulse(vector)`: pushes it
- `add_force(force, at: nil)`: pushes it during the next step (newtons); at a point (world space), it also turns it
- `add_torque(torque)`: turns it during the next step (newton meters)
- `angular_velocity`: how fast it spins, degrees per second around each axis
- `angular_velocity = vector`: sets how fast it spins
- `mass`: its mass in kilograms
- `mass = kilograms`: changes its mass (more than 0)
- `kinematic?`: whether scripts move it (instead of forces)
- `kinematic = flag`: turns a dynamic body kinematic, or back
- `dynamic?`: whether forces move it
- `dynamic = flag`: turns a kinematic body dynamic, or back
- `gravity_scale`: how much gravity pulls it (1 is normal)
- `gravity_scale = scale`: 0 floats, 2 falls twice as hard
- `sleep`: stops simulating it until something touches it
- `wake`: simulates it again
- `sleeping?`: whether it sleeps
- `layer`: its collision layer, a Symbol
- `layer = name`: moves it to another collision layer

Forces and torques act during the next physics step only: add them every `fixed_update` to keep pushing. A body made dynamic again starts at rest. `kinematic=` and `dynamic=` raise for a vehicle.

## Vehicle

The body of an object with body = "vehicle" and wheels. See [gameplay.md](gameplay.md).

- `drive(throttle: 0.0, steering: 0.0, brake: 0.0, handbrake: 0.0)`: drives it this frame (each from -1 or 0 to 1)
- `stop`: brakes fully
- `speed`: its speed in m/s, signed
- `velocity`: its velocity

Wheel meshes must have their rotation applied (the axle along X). `drive`'s inputs are the one setting the engine clamps, since an analog stick may push a little past full.

## Camera

The camera the game is seen through, and what it does on its own: follow, orbit, shake. See [gameplay.md#follow-orbit-and-shake-the-camera](gameplay.md#follow-orbit-and-shake-the-camera).

- `Camera.main`: the object holding the camera
- `Camera.screen_ray(x, y)`: [origin, direction] through a pixel of the screen
- `Camera.world_to_screen(point)`: [x, y]: where a Vector3 (or an object) is on the screen, in the pixels UI draws in; nil behind the camera
- `Camera.fov`: its vertical field of view, in degrees
- `Camera.fov = degrees`: sets the field of view
- `Camera.motion_blur`: how much the picture streaks with speed, 0 to 1
- `Camera.motion_blur = amount`: sets it
- `Camera.follow(target, offset: [0, 3, 6], relative: false, smoothing: 0.15, look_at: true, look_height: 1.0, avoid_walls: true, dead_zone: 0)`: follows an object from now on: offset in meters (relative: in its own axes), smoothing in seconds to close half the way, in front of walls between
- `Camera.orbit(target, distance: 6, yaw: 0, pitch: 20, distances: 1..30, pitches: -80..80, smoothing: 0.08, input: true, height: 1.0, avoid_walls: true)`: orbits an object or a point, looking at it, turned by dragging with the right mouse button or the right stick, zoomed by the wheel; returns a Camera::Orbit
- `Camera.stop_following`: stops following or orbiting; the camera stays where it is
- `Camera.following?`: whether it follows or orbits something (an object destroyed is let go)
- `Camera.shake(strength, duration: 0.5)`: shakes it: strength (0 to 1) adds to its trauma, held at 1, fading over duration seconds; on top of follow and orbit
- `Camera.trauma`: how much it shakes now, 0 to 1
- `Camera.stop_shaking`: stops the shake at once

## Camera::Orbit

What Camera.orbit returns: the camera's angles and distance around what it orbits, for scripts to read and change. See [gameplay.md#follow-orbit-and-shake-the-camera](gameplay.md#follow-orbit-and-shake-the-camera).

- `yaw`: degrees around it: 0 behind it (its +Z side), 90 on its right
- `pitch`: degrees above it
- `distance`: meters from it
- `target`: the object or point it orbits
- `turn(yaw, pitch)`: turns by degrees, the pitch kept within its range
- `zoom(meters)`: comes nearer (negative) or goes further, within its range

## Light

game_object.light: the light an object shines with. See [gameplay.md#light-that-flickers-and-fades](gameplay.md#light-that-flickers-and-fades).

- `intensity`: watts for a point or spot light, lux for the sun
- `intensity = value`: sets it; shows from the next frame
- `color`: its color [r, g, b], 0 to 255
- `color = color`: :orange or [r, g, b] from 0 to 255

## Animation

The scene's own animations (exported from Blender), by name. See [animation.md](animation.md).

- `Animation.names`: the names of the scene's animations
- `Animation.play(name, loop: true, speed: 1.0, restart: false)`: plays one
- `Animation.pause(name)`: pauses one
- `Animation.stop(name)`: stops one
- `Animation.playing?(name)`: whether it plays
- `Animation.time(name)`: where it is, in seconds
- `Animation.duration(name)`: how long it is, in seconds
- `Animation.seek(name, seconds)`: jumps to a moment

## Animator

game_object.animation (animation in a behaviour): an object's own clips. See [animation.md](animation.md).

- `play(name, loop: true, speed: 1.0, fade: 0.0, restart: false)`: plays a clip, fading from the last one
- `mix(name, weight: 1.0, loop: true, speed: 1.0)`: plays a clip over the one playing
- `unmix`: stops the clip mixed over
- `pause`: pauses its clips
- `follow_scene(fade: 0.0)`: goes back to the scene's animations
- `clip`: the clip playing
- `playing?`: whether a clip plays
- `time`: where the clip is, in seconds
- `use(graph)`: installs an Animation::Graph
- `set(parameter, value)`: sets a parameter of the graph
- `state`: the graph's state, or nil

## Animation::Graph

Named states and transitions for one character (animation.use(graph)). See [animation.md](animation.md).

- `Animation::Graph.new(initial:, parameters: {})`: a graph starting at a state, with its parameters and their defaults
- `clip(name, clip, loop: true, speed: 1.0)`: a state that plays one clip
- `blend_1d(name, parameter:, clips:, loop: true, speed: 1.0)`: a state that blends clips by a number: clips: [[0.0, :Idle], [5.0, :Run]]
- `blend_2d(name, parameters:, clips:, loop: true, speed: 1.0)`: a rectangular two-dimensional grid: parameters: [:x, :y], clips: [[[x, y], :Clip], ...]
- `transition(from, to, parameter:, equals: nil, above: nil, below: nil, fade: 0.0)`: goes from one state to another when a parameter says so

## Audio

Sounds and music from the game's sounds/ folder.

- `Audio.play(name, volume: 1.0, pitch: 1.0)`: plays a sound once
- `Audio.music(name, loop: true, volume: 0.7)`: plays music
- `Audio.stop_music`: stops the music
- `Audio.music_pitch = pitch`: changes the music's pitch
- `Audio.music_volume = volume`: changes the music's volume
- `Audio.volume = volume`: the volume of everything
- `Audio.play_at(name, position, loop: false, volume: 1.0, pitch: 1.0, reference_distance: 1.0, max_distance: 50.0)`: plays a sound at a place in the world; returns its id, or nil
- `Audio.listener(position, forward, up)`: where the ears are (move it with the camera)
- `Audio.move(id, position)`: moves a sound played with play_at
- `Audio.playing?(id)`: whether that sound still plays
- `Audio.stop(id)`: stops that sound
- `Audio.stop_sources`: stops every sound played with play_at

`play_at` sources use stereo panning and inverse-distance volume, fading to silence at `max_distance`. `volume` must be 0 or more and `pitch` from 0.01 to 8; `0 < reference_distance < max_distance`. Positions and directions may be arrays or `Vector3`. Update `Audio.listener(position, forward, up)` when the camera moves: nothing follows an object by itself. At most 64 sources play at once; old ids are never reused.

## UI

What a script draws over the game this frame: texts, rectangles, pictures, buttons. See [gameplay.md](gameplay.md).

- `UI.text(str, x:, y:, size:, color:, alpha:, align:)`: draws a text
- `UI.rect(x:, y:, width:, height:, color:, alpha:, radius:)`: draws a rectangle
- `UI.image(name, x:, y:, width:, height:)`: draws a picture of images/
- `UI.button(label, x:, y:, width:, height:)`: draws a button; true the frame it is clicked
- `UI.text_field(value, x:, y:, width:, height:, max_length:, secret:)`: a field to type in; returns the new value
- `UI.width`: the width of the screen, in pixels
- `UI.height`: the height of the screen, in pixels
- `UI.text_width(str, size:)`: how wide a text is, in pixels (in the face selected)
- `UI.toggle(label, on, x:, y:, width:, height:, focused: false)`: an on/off row of a settings screen; returns whether it is on
- `UI.choice(label, choices, index, x:, y:, width:, height:, focused: false)`: a row that picks one of choices; returns the index chosen
- `UI.slider(label, value, x:, y:, width:, height:, min: 0.0, max: 1.0, step: 0.05, focused: false)`: a row with a slider; returns the value
- `UI.font`: the face texts are drawn with: a file name of fonts/, or nil for the built-in one
- `UI.font = name`: draws the next texts with fonts/<name>.ttf or .otf (nil: the built-in face)

A focused widget (`focused: true`) is worked by Enter or a gamepad's A, and left and right change it. The built-in face adds glyphs as they are asked for (Greek, Cyrillic, accented letters, `€`); for another face or script, put a `.ttf` or `.otf` in `fonts/` and set `UI.font`. A missing or unreadable face falls back to the built-in one, with a warning; glyphs a face lacks show as `?`; there is no bidirectional layout or complex-script shaping.

## UI::Focus

Which widget of a screen the keyboard and a gamepad work: up and down move it, wrapping. See [gameplay.md](gameplay.md).

- `UI::Focus.new`: a focus on the first widget
- `update(count)`: moves it among count widgets, as up or down was pressed this frame
- `at?(index)`: whether the widget of that index has it: pass it as focused:
- `move_to(index)`: puts it on a widget (one clicked, or the one a screen comes back to)
- `index`: the widget that has it
- `reset`: back to the first widget

## Debug

Developer diagnostics: console messages, frame timings and immediate world drawing.

- `Debug.log(message)`: says something
- `Debug.warn(message)`: warns
- `Debug.error(message)`: reports an error
- `Debug.profiling?`: whether foreground frame timings are being collected
- `Debug.profiling = enabled`: starts a fresh profile window when enabled; disabling retains the last summary
- `Debug.stats_visible?`: whether the developer statistics overlay was requested
- `Debug.stats_visible = visible`: shows frame timings and FPS; showing also enables profiling, hiding leaves collection running
- `Debug.frame_stats`: a snapshot with samples, fps and latest/mean/peak timings in milliseconds
- `Debug.reset_frame_stats`: clears statistics and discards any unfinished measurement
- `Debug.line(from, to, color: :green, alpha: 255, depth_test: true)`: draws a world-space line for this frame; returns whether it was accepted
- `Debug.box(center, size, color: :green, alpha: 255, depth_test: true)`: draws an axis-aligned wire box with full nonnegative size for this frame; returns whether it was accepted

Profiling is off by default. The overlay uses the shared game HUD, including editor Play and recordings; it is hidden when profiling is off. Timings use the real steady clock, independently of Time.scale or fixed-step replay. The most recent 120 completed frames form the window; reads during a frame expose only completed frames. FPS uses completion-to-completion intervals, and is zero until an interval is available. `latest`, `mean` and `peak` contain `frame_ms`, `interval_ms`, `has_interval`, and `input_ms`, `network_ms`, `http_ms`, `update_ms`, `physics_ms`, `presentation_ms`, `scene_ms`. Input includes pending script startup; network and HTTP include their callbacks; update includes animations, scripts, particles, loading UI and physics. Physics includes fixed-update callbacks, body synchronization and contacts, so its time overlaps update. Presentation includes uploads, drawing and any frame-cap or synchronization waits. Scene includes request polling, synchronous reads and activation; the asynchronous worker is not instrumented, while offline fixed-step waits count as scene wall time. These are foreground wall times, not GPU timers or CPU utilization. Phase times are inclusive and must not be summed as a partition. Overlay timings and captures are nondeterministic; leave the overlay off for reproducible pictures. Debug.line and Debug.box use world coordinates and live for one frame: submit from ready/update/fixed_update. At most 4096 commands are accepted per frame; later calls return false. They are local-only and return false without a presenter, while still validating arguments. depth_test: false draws through objects. Shape frames use unjittered rendering and reset temporal history; normal temporal sampling resumes afterwards. Native windows allocate one cached color/depth target only when shapes are requested; editor, scaled and recorded views reuse their target. If that extra native target is unavailable, the game still draws with a warning and the shapes are omitted.

## Game

The game itself. See [gameplay.md#a-loading-screen](gameplay.md#a-loading-screen).

- `Game.load_scene(name)`: switches to scenes/<name>.glb ("main" is scene.glb); Ruby globals survive
- `Game.load_scene_async(name)`: queues a background scene read; the old world survives failed reads and cancellation
- `Game.loading?`: a scene request is queued or being read
- `Game.loading_scene`: the requested scene name, or nil
- `Game.loading_error`: the last asynchronous read error, or nil; cleared by the next load request
- `Game.cancel_scene_load`: cancels a pending asynchronous request; returns whether one was pending
- `Game.on_loading(&block)`: draws the loading screen each frame with unscaled delta; no block restores the default
- `Game.scene_name`: the scene running
- `Game.title`: the game's title
- `Game.quit`: ends the game
- `Game.stop_recording`: ends a video recorded with --record; the game goes on
- `Game.version`: the version in game.json ("1.2.0"), or nil

While an asynchronous read is pending, physics, animations and normal behaviour callbacks pause.
Input, networking, HTTP callbacks, audio and presentation continue. The loading callback can use
`UI`, handle cancel buttons and animate using its unscaled delta; `Time.delta` is zero during
loading. The pointer is released while the screen is shown. The default screen draws a translated
loading label over the old world. A callback error is reported once and disables it for that load.

Unsafe names are rejected immediately; missing or unreadable files are reported through
`Game.loading_error` after the request runs. The old world stays intact if reading fails or is cancelled. The most recent request wins;
`Game.load_scene` also supersedes an asynchronous request. Globals survive successful activation,
but the old scene's loading callback is removed before the next scene's setup runs. Errors are
reported through `Game.loading_error` and the event log. At most eight background requests/results can be
outstanding across the process, using one shared reader; a full queue fails without changing the
world. Cancelling an active read discards its result; it does not interrupt the file parser.

Only file reading and glTF parsing happen on the worker. Physics creation, script setup, texture
decoding and GPU upload still happen on the owning thread and may hitch during activation. The
old and incoming scene coexist in memory during reading. Fixed-step offline runs (including
recordings and scripted replays) wait for the reader before advancing, then show exactly one
loading frame before activation so completion does not depend on thread scheduling. Networked runs and dedicated-server rooms keep the
background reader even in fixed-step mode; activation still runs on the server loop and can stall
other rooms sharing that loop. Closing the process waits for an active parser to return.

## Settings

How the player chose the game to be shown and heard; a change takes effect at once. See [../shipping.md#notes](../shipping.md#notes).

- `Settings.window_mode`: :windowed, :fullscreen or :borderless
- `Settings.window_mode = mode`: changes it
- `Settings.fullscreen?`: whether the game fills the screen
- `Settings.fullscreen = on`: fullscreen, or back to a window
- `Settings.resolution`: [width, height] of the window, in pixels
- `Settings.resolution = size`: resizes the window: one of resolutions
- `Settings.resolutions`: the sizes the monitor offers: [[w, h], ...]
- `Settings.render_scale`: the share of the window's size the world is drawn at (0.5 to 2)
- `Settings.render_scale = scale`: changes it
- `Settings.vsync`: whether frames wait for the screen
- `Settings.vsync = on`: changes it
- `Settings.fps_cap`: frames a second at most; nil for none
- `Settings.fps_cap = cap`: changes it
- `Settings.quality`: :low, :medium, :high, :ultra or :custom
- `Settings.quality = quality`: the most a level's environment gets of what it asks for
- `Settings.quality_details`: what the custom quality allows: {msaa:, antialias:, shadow_cascades:, ...}
- `Settings.quality_details = details`: changes some of them, and makes the quality :custom
- `Settings.volume`: the player's volume, 0 to 1 (it multiplies Audio.volume)
- `Settings.volume = volume`: changes it
- `Settings.save`: keeps them for the next runs; false where they last for the run only

Until the player chooses, everything is what `game.json` says. `Settings.save` keeps them only in a built game a player plays: in the editor's Play, `redgem run`, a server's rooms and tests they last for the run, and `save` returns false.

## GameMenu

A behaviour: title screen, pause menu, end screen and settings screen with its controls page; arrows or the D-pad move the focus, Enter or A picks. See [gameplay.md#pause-and-the-flow-between-screens](gameplay.md#pause-and-the-flow-between-screens).

- `GameMenu.instance`: the menu of the scene, or nil
- `show(screen)`: opens a screen: :title, :pause, :over, :settings or one of the game's
- `show_settings`: opens the settings screen (window, resolution, quality, volume...; its Controls button opens the controls page)
- `resume`: closes the menu and goes on
- `restart`: starts the scene again
- `finish(message)`: opens the end screen with a message
- `open?`: whether the menu is open
- `screen`: the screen open
- `screens`: the screens, by name (:title, :pause, :over, :settings, :controls and the game's own)
- `button_box(index)`: where the menu's button of that index is: [x, y, width, height]

## GameMenu::Screen

A screen of the GameMenu. See [gameplay.md#pause-and-the-flow-between-screens](gameplay.md#pause-and-the-flow-between-screens).

- `GameMenu::Screen.new(heading:, options:, back:)`: a screen with a heading and options: [[label, action], ...]
- `add_option(label, index:, &action)`: adds an option
- `heading`: its heading
- `heading = text`: changes its heading
- `options`: its options
- `back`: the screen Esc goes back to
- `opened`: called when it is shown
- `closed`: called when it is left
- `draw(first_button)`: draws what else it shows
- `space_above`: pixels kept between its heading and its buttons
- `typing?`: whether letters are typed into it
- `rows`: how many widgets it draws above its buttons that take the focus
- `first_button_share`: where its first button sits, a share of the height (nil: the menu's)
- `focus`: its UI::Focus, which the menu gives it
- `focus = focus`: sets it

## Particles

Short-lived sparks, smoke and dust. See [gameplay.md#sparks-dust-and-smoke](gameplay.md#sparks-dust-and-smoke).

- `Particles.burst(position, count:, color:, alpha:, glow:, lit:, cast_shadow:, soft:, direction:, spread:, speed:, life:, size:, gravity:, image:, frames:, fps:, spin:)`: adds particles; returns how many (size, color and alpha may be lists over their life)
- `Particles.emitter(object, rate: 24, offset: Vector3.zero, **settings)`: starts a continuous local effect; returns a Particles::Emitter handle; uses burst settings except count
- `Particles.count`: how many are alive
- `Particles.clear`: removes all particles and stops every emitter

Emitters follow game time and stop on object destruction or scene changes. They are local visual effects, not replicated. Without a presenter, creation returns an inactive handle and no emitter is allocated or simulated. Births follow ordinary movement; movement over 10 meters per update starts them at the destination. Up to 256 emitters fairly share 4096 live particles; missed births are dropped.

## Particles::Emitter

A continuous particle effect attached to an object, returned by Particles.emitter. See [gameplay.md#sparks-dust-and-smoke](gameplay.md#sparks-dust-and-smoke).

- `active?`: whether this emitter is still emitting from a live object
- `stop`: stops new births; existing particles finish their lives

## Scatter

Great numbers of copies of a mesh (grass, trees, rocks), drawn only. See [../levels.md#grass-forests-and-cities](../levels.md#grass-forests-and-cities).

- `Scatter.place(template, count:, area:, ground: nil, seed: 0, scale: 1.0, align: false, tints:, tint_variation:, fade:, normals_up:, lods:)`: places copies; returns a set (count, remove)
- `Scatter.clear`: removes every set
- `count`: how many copies a set placed
- `remove`: removes a set

## Save

What the game keeps between runs. See [online-services.md#saving-on-a-server](online-services.md#saving-on-a-server).

- `Save.get(key, default = nil)`: a value, or the default
- `Save.set(key, value)`: keeps a value (nil removes it)
- `Save[key]`: the same as get
- `Save[key] = value`: the same as set
- `Save.update(key, default = nil, &block)`: reads and writes a key with nothing in between
- `Save.delete(key)`: removes a key
- `Save.clear`: removes everything
- `Save.keys`: every key
- `Save.slot`: the selected save slot name; default keeps the existing save file
- `Save.slot = name`: selects a named slot for this engine; default restores the original store
- `Save.slots`: sorted names of persisted slots and slots opened in this run, including default

Values are numbers, texts, true/false, `Vector3`, and arrays and hashes of those; hashes come back with string keys. Named slots start on default for every engine and keep their selection across scenes. Selection is local to each engine, including shared-database server rooms. Names are 1–48 lowercase ASCII letters/digits, hyphens or underscores, starting with a letter/digit. At most 64 named stores may be opened per run/local backend and 4096 names listed; invalid names or unavailable backends leave selection unchanged and raise a Ruby error. The existing default file/table is preserved. Named files use their own atomic writes/backups; SQLite uses separate slot tables. Save.update writes to its entry store even if its block selects another slot. Slots keep values, not object-state snapshots or migration metadata.

## HTTP

Web requests; the answer comes to the block. See [online-services.md#http](online-services.md#http).

- `HTTP.get(url, headers: {}, timeout: 15, &block)`: GET
- `HTTP.post(url, json: nil, body: nil, headers: {}, timeout: 15, &block)`: POST
- `HTTP.put(url, json: nil, body: nil, headers: {}, timeout: 15, &block)`: PUT
- `HTTP.patch(url, json: nil, body: nil, headers: {}, timeout: 15, &block)`: PATCH
- `HTTP.delete(url, headers: {}, timeout: 15, &block)`: DELETE
- `HTTP.request(verb, url, json: nil, body: nil, headers: {}, timeout: 15, &block)`: any verb

## HTTP::Response

What a web request's block is given. See [online-services.md#http](online-services.md#http).

- `ok?`: whether it came back with a 2xx status
- `status`: its status
- `headers`: its headers
- `body`: its body
- `json`: its body read as JSON
- `error`: why there was no answer, or nil

## I18n

The game's texts in several languages (locales/*.json). See [i18n.md](i18n.md).

- `I18n.t(key, **values)`: the text of a key in the language in use, its {placeholders} filled
- `I18n.locale`: the language in use (:en)
- `I18n.locale = language`: changes the language
- `I18n.available_locales`: the languages of the game's locales/ files

## Network

Games played together: on a local network, through a relay or on a dedicated server. See [multiplayer.md](multiplayer.md).

- `Network.host(port: 7777, name: nil, visible: true, room: false)`: hosts a game
- `Network.join(address = nil, port: 7777, room: nil, auth: nil)`: joins a game (an address, or a room code)
- `Network.leave`: leaves the game
- `Network.online?`: whether this machine is in a game
- `Network.connecting?`: whether a join is on its way
- `Network.host?`: whether this machine hosts
- `Network.server?`: whether this machine is a dedicated server
- `Network.id`: this machine's peer id
- `Network.peers`: the peers of the game
- `Network.players`: the peers without a dedicated server
- `Network.port`: the port of the game
- `Network.room`: the room code, on a relay
- `Network.relay`: the relay's address
- `Network.relay = address`: sets the relay's address
- `Network.search`: looks for games on the local network
- `Network.searching?`: whether it is looking
- `Network.stop_search`: stops looking
- `Network.games`: the games found
- `Network.discovery_port`: the port games are looked for on
- `Network.discovery_port = port`: changes it
- `Network.emit(name, data = nil, to: :all)`: sends a message (:all, :host or a peer id)
- `Network.on(name, &block)`: receives a message: |data, from|
- `Network.on_connect(&block)`: this machine is in the game
- `Network.on_disconnect(&block)`: the game is gone
- `Network.on_join(&block)`: someone arrived: |peer|
- `Network.on_leave(&block)`: someone left: |peer|
- `Network.on_fail(&block)`: a join did not work out: |reason|
- `Network.authenticate(&block)`: decides who gets in: |peer, auth, decide|
- `Network.identity(peer)`: who a peer was accepted as
- `Network.player_name(peer)`: a peer's name
- `Network.spawn(template, owner: Network.id, position:, rotation:)`: spawns a copy on every machine
- `Network.allow_spawn(*templates)`: lets guests spawn those templates

## Network::Decision

What an authenticate block decides with. See [online-services.md](online-services.md).

- `accept(identity:, name: nil)`: lets the peer in
- `deny(reason = "not let in")`: keeps the peer out

## NetworkSync

A behaviour: the same object in the same place on every machine. See [multiplayer.md#objects-in-the-same-place-everywhere](multiplayer.md#objects-in-the-same-place-everywhere).

- `mine?`: whether this machine owns it
- `owner`: the peer that owns it
- `owner = peer`: gives it to a peer
- `key`: the name it is known by on every machine
- `assign(owner:, key:)`: sets its owner and key

## Notes

Script errors are logged with a backtrace and only disable the failing behaviour; the game keeps running. Every method takes finite numbers a float can hold: infinity, NaN or a number too large raises an `ArgumentError` instead of reaching the scene. A setting with a range raises outside it and says the range; nothing is quietly clamped, except `Vehicle#drive`'s inputs. Assigning `transform.position` to a physics object teleports it (moving bodies also stop); changing its rotation keeps its velocity. Images may be PNG, JPEG or static WebP (a name without an extension looks for `.webp` last), at most 8192 pixels a side; animated WebP and KTX2/Basis are not read. Missing or unreadable images (scene textures, `UI.image`, particle images) show an opaque magenta-and-black checkerboard, with a warning; a material that never named a texture keeps its plain color, and no path reads outside the game's folder.
