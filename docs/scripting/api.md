---
description: "Every class and method of the scripting API, a line each (also: redgem api)."
---

# API reference

The scripting API of RedGem games: the classes and methods scripts use. Generated from docs/scripting/api.json, which tests keep in step with the engine.

| Class | What it is |
|---|---|
| [`Behaviour`](#behaviour) | A Ruby class attached to an object: the engine calls its methods as the game runs. |
| [`Behaviour::Timer`](#behaviourtimer) | What after, every and tween return: something waiting to happen, which can be called off. |
| [`GameObject`](#gameobject) | An object of the scene, as scripts see it. |
| [`Transform`](#transform) | Where an object is, how it is turned and how big it is. |
| [`Vector3`](#vector3) | A point or a direction in meters: x, y, z. |
| [`Scene`](#scene) | The objects of the running scene. |
| [`SceneScript`](#scenescript) | Sets a level up from Ruby when it loads: scripts/<scene>_scene.rb. |
| [`Input`](#input) | Keyboard, mouse, gamepad and touch, read every frame. |
| [`Time`](#time) | The game's clock (added to Ruby's Time). |
| [`Physics`](#physics) | Questions to the physics world: rays, moving shapes and shapes in place. |
| [`Joint`](#joint) | What holds two bodies together, or one to the world (made by Physics.joint). |
| [`Navigation`](#navigation) | Where the characters a game moves can walk, and the way from one place to another. |
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
| [`UI::ScrollList`](#uiscrolllist) | Rows in a box that scrolls, drawn only inside it: the wheel, Page Up/Down, a gamepad's bumpers and right stick, and a focus. |
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

- `start -> void`: called once, before the first update
- `update(Float dt) -> void`: called every frame; dt is the seconds since the last one
- `fixed_update(Float dt) -> void`: called 60 times a second, with the physics
- `on_trigger_enter(GameObject other) -> void`: something entered this object's trigger, or it entered one
- `on_trigger_exit(GameObject other) -> void`: something left this object's trigger
- `on_collision_enter(GameObject other) -> void`: this object's body touched another
- `on_collision_exit(GameObject other) -> void`: this object's body stopped touching another
- `on_destroy -> void`: the object is being destroyed
- `game_object -> GameObject`: the object this behaviour is attached to
- `name -> String`: the object's name
- `transform -> Transform`: the object's Transform
- `character -> CharacterController | nil`: the object's CharacterController, or nil
- `rigidbody -> Rigidbody | nil`: the object's Rigidbody, or nil
- `vehicle -> Vehicle | nil`: the object's Vehicle, or nil
- `animation -> Animator`: the object's own Animator
- `props -> Hash[String, untyped]`: the object's custom properties, as a Hash
- `prop(Symbol | String key, untyped default = nil) -> untyped`: a setting: what add_component was given, else the object's property, else the default
- `find(String | Symbol name) -> GameObject | nil`: the object called name, or nil
- `find!(String | Symbol name) -> GameObject`: find that stops with a clear error when the object is missing
- `find_all(String | Symbol tag) -> Array[GameObject]`: every object with that tag
- `destroy(GameObject target = game_object) -> void`: removes an object (this one unless said) at the end of the step
- `get_component(Class klass) -> Behaviour | nil`: another behaviour of this object, or nil
- `after(Numeric seconds) { () -> void } -> Behaviour::Timer`: runs the block once, that many seconds from now
- `every(Numeric seconds) { () -> void } -> Behaviour::Timer`: runs the block every that many seconds
- `tween(Numeric seconds, ease: Symbol = :smooth) { (Float) -> void } -> Behaviour::Timer`: calls the block with t from 0 to 1 over that many seconds
- `cancel_timers -> void`: stops every after, every and tween of this behaviour

## Behaviour::Timer

What after, every and tween return: something waiting to happen, which can be called off.

- `cancel -> void`: calls it off (also from inside its own block)
- `cancelled? -> bool`: whether it was called off

## GameObject

An object of the scene, as scripts see it. See [behaviours.md](behaviours.md).

- `name -> String`: its name
- `id -> Integer`: its number in the running game
- `stable_id -> String | nil`: its persistent scene-file identity as a String; nil for runtime copies
- `capture_state -> Hash[Symbol, untyped]`: a versioned checkpoint of local pose, tint and visibility, suitable for Save
- `restore_state(Hash[Symbol, untyped] state) -> self`: validates a checkpoint then restores pose and appearance; teleports physical descendants and returns self
- `tag -> String | nil`: its tag property
- `tag?(String | Symbol value) -> bool`: whether its tag is value
- `props -> Hash[String, untyped]`: its custom properties
- `alive? -> bool`: false once it was destroyed
- `destroy -> void`: removes it at the end of the step
- `visible -> bool`: whether it is drawn
- `visible = bool value`: shows or hides it
- `behaviours -> Array[Behaviour]`: its behaviours
- `parent -> GameObject | nil`: the object it is under, or nil
- `parent = GameObject | nil object`: puts it under another object (nil: at the root), where it stays in the world, then moves with it; never under itself or what it carries
- `set_parent(GameObject | nil object, keep_world: bool = true) -> GameObject | nil`: like parent=; keep_world: false keeps its local transform instead, so the new parent places it; returns the parent
- `children -> Array[GameObject]`: the objects under it
- `transform -> Transform`: its Transform
- `character -> CharacterController | nil`: its CharacterController, or nil
- `rigidbody -> Rigidbody | nil`: its Rigidbody, or nil
- `vehicle -> Vehicle | nil`: its Vehicle, or nil
- `light -> Light | nil`: the Light it shines with, or nil
- `animation -> Animator`: its Animator
- `animation_offset -> Float`: seconds its animations run ahead of the original's (a spawned copy)
- `animation_offset = Numeric seconds`: sets that offset
- `get_component(Class klass) -> Behaviour | nil`: one of its behaviours, or nil
- `add_component(Class klass, **untyped settings) -> Behaviour | nil`: attaches a behaviour; settings are what its prop reads
- `tint -> Vector3`: the color its colors are multiplied by
- `tint = Vector3 | Array[Numeric] color`: multiplies its colors: [1.0, 0.5, 0.5]
- `morph(String | Symbol name, Numeric | nil weight = nil) -> Numeric | nil`: the weight of a shape key (morph target) of its mesh and the meshes under it, or sets it: morph(:Smile, 0.7); nil when no part has the key, ArgumentError when setting one none has
- `morph_names -> Array[Symbol]`: the names of the shape keys of its mesh and the meshes under it, as symbols

## Transform

Where an object is, how it is turned and how big it is.

- `position -> Vector3`: its position in the world (Vector3)
- `position = Vector3 vector`: moves it; a physics object is teleported
- `local_position -> Vector3`: its position relative to its parent
- `local_position = Vector3 vector`: moves it relative to its parent
- `rotation -> Vector3`: its rotation, Euler degrees (Vector3)
- `rotation = Vector3 degrees`: turns it to those Euler degrees
- `rotate(Vector3 degrees) -> void`: turns it by those Euler degrees
- `look_at(Vector3 | GameObject target, Vector3 up = Vector3.up) -> void`: turns it to face a point
- `translate(Vector3 offset) -> void`: moves it by an offset
- `scale -> Vector3`: its scale (Vector3)
- `scale = Vector3 vector`: resizes it
- `forward -> Vector3`: the direction it faces (-Z)
- `right -> Vector3`: its right (+X)
- `up -> Vector3`: its up (+Y)

## Vector3

A point or a direction in meters: x, y, z.

- `Vector3.new(Numeric x, Numeric y, Numeric z) -> Vector3`: a vector
- `Vector3[Numeric x, Numeric y, Numeric z] -> Vector3`: the same as new
- `Vector3.zero -> Vector3`: (0, 0, 0)
- `Vector3.one -> Vector3`: (1, 1, 1)
- `Vector3.up -> Vector3`: (0, 1, 0)
- `Vector3.down -> Vector3`: (0, -1, 0)
- `Vector3.left -> Vector3`: (-1, 0, 0)
- `Vector3.right -> Vector3`: (1, 0, 0)
- `Vector3.forward -> Vector3`: (0, 0, -1)
- `Vector3.back -> Vector3`: (0, 0, 1)
- `Vector3.blender(Numeric x, Numeric y, Numeric z) -> Vector3`: a vector given in Blender's axes (Z up)
- `x -> Float`: its x
- `y -> Float`: its y
- `z -> Float`: its z
- `+ Vector3 other -> Vector3`: sum
- `- Vector3 other -> Vector3`: difference
- `-@ -> Vector3`: the opposite vector
- `* Numeric number -> Vector3`: scaled
- `/ Numeric number -> Vector3`: divided
- `== untyped other -> bool`: whether the numbers are exactly the same
- `dot(Vector3 other) -> Float`: dot product
- `cross(Vector3 other) -> Vector3`: cross product
- `length -> Float`: its length
- `length_squared -> Float`: its length squared (faster)
- `normalized -> Vector3`: the same direction, length 1
- `clamped(Numeric max_length = 1.0) -> Vector3`: the same direction, no longer than max_length
- `lerp(Vector3 other, Numeric t) -> Vector3`: the point t of the way to other
- `flat -> Vector3`: the same with y = 0
- `with(x: Numeric, y: Numeric, z: Numeric) -> Vector3`: a copy with some parts changed: v.with(y: 0)
- `distance_to(Vector3 other) -> Float`: the distance to another point
- `approx?(Vector3 other, Numeric tolerance = 1e-6) -> bool`: whether two vectors are about the same
- `to_a -> [Float, Float, Float]`: [x, y, z]
- `set(Numeric x, Numeric y, Numeric z) -> self`: changes it in place
- `add!(Vector3 other) -> self`: adds in place
- `sub!(Vector3 other) -> self`: subtracts in place
- `scale!(Numeric number) -> self`: scales in place
- `normalize! -> self`: normalizes in place

`==` compares the numbers exactly, and equal vectors are one key of a `Hash`. A vector used as a key must not be changed in place (`set`, `add!`...) while the `Hash` holds it: use a `dup` as the key. The scene keeps positions, rotations and scales in 32-bit floats, so a vector that goes through it comes back rounded: compare what comes back from the scene with `approx?(other, tolerance)`. The in-place methods change the vector and hand it back: `dup` one you got from someone else first. Arithmetic whose result would not fit a float, and dividing by zero, raise an `ArgumentError`; a vector changed in place is then left as it was.

## Scene

The objects of the running scene. See [gameplay.md](gameplay.md).

- `Scene.find(String | Symbol name) -> GameObject | nil`: the object called name, or nil
- `Scene.find_by_id(String | Symbol identity) -> GameObject | nil`: the current scene object with that persistent String/Symbol identity, or nil
- `Scene.find!(String | Symbol name, wanted_by: Class | nil = nil) -> GameObject`: find that stops with a clear error when the object is missing
- `Scene.find_all(String | Symbol tag) -> Array[GameObject]`: every object with that tag
- `Scene.objects -> Array[GameObject]`: every object
- `Scene.spawn(String | GameObject template, position: Vector3 | nil = nil, rotation: Vector3 | nil = nil, physics: bool = true, name: String | nil = nil, parent: GameObject | nil = nil) -> GameObject | nil`: copies a prefab = true template, with its physics and scripts, named name: (the template's by default) and under parent:, keeping its place in the world; returns the copy

## SceneScript

Sets a level up from Ruby when it loads: scripts/<scene>_scene.rb. See [behaviours.md#setting-a-level-up-in-ruby](behaviours.md#setting-a-level-up-in-ruby).

- `setup -> void`: runs when the level loads, before any behaviour starts
- `find(String | Symbol name) -> GameObject | nil`: the object called name, or nil
- `find!(String | Symbol name) -> GameObject`: find that stops with a clear error when the object is missing
- `find_all(String | Symbol tag) -> Array[GameObject]`: every object with that tag

## Input

Keyboard, mouse, gamepad and touch, read every frame. See [gameplay.md](gameplay.md).

- `Input.axis(Symbol name, pad: Integer = 0) -> Float`: :horizontal or :vertical, from -1 to 1 (keys, stick)
- `Input.down?(Symbol key) -> bool`: whether a key is held
- `Input.pressed?(Symbol key) -> bool`: whether a key went down this frame
- `Input.released?(Symbol key) -> bool`: whether a key went up this frame
- `Input.action?(Symbol name, pad: Integer = 0) -> bool`: whether a named action (game settings) is held
- `Input.action_pressed?(Symbol name, pad: Integer = 0) -> bool`: whether a named action started this frame
- `Input.action_released?(Symbol name, pad: Integer = 0) -> bool`: whether a named action ended this frame
- `Input.define_action(Symbol name, *String bindings, keys: Array[Symbol] = [], buttons: Array[Symbol] = [], mouse: Array[Symbol] = []) -> void`: adds an action of the game's own (besides game.json's input section)
- `Input.define_axis(Symbol name, negative: Array[String] = [], positive: Array[String] = [], pad_axis: Symbol | nil = nil, invert: bool = false) -> void`: adds an axis of the game's own: define_axis(:steer, negative: ["key:a"], positive: ["key:d"])
- `Input.actions -> Array[Symbol]`: the names of the actions
- `Input.axes -> Array[Symbol]`: the names of the axes
- `Input.axis_stick(Symbol name) -> Symbol | nil`: the stick that moves an axis (:left_x), the game's for every player; nil when none does
- `Input.bindings(Symbol name, pad: Integer = 0) -> Array[String]`: what a player bound to an action: ["key:space", "mouse:left", "pad:a"]
- `Input.bindings_changes -> Integer`: a count that goes up whenever actions, axes or bindings may have changed
- `Input.bind(Symbol name, String | nil binding = nil, key: Symbol | nil = nil, button: Symbol | nil = nil, mouse: Symbol | nil = nil, side: Symbol | nil = nil, pad: Integer = 0) -> void`: binds a key, a mouse button (mouse: :left, :wheel_up) or a gamepad button to an action; kept between runs
- `Input.unbind(Symbol name, String | nil binding = nil, key: Symbol | nil = nil, button: Symbol | nil = nil, mouse: Symbol | nil = nil, side: Symbol | nil = nil, pad: Integer = 0) -> void`: takes a binding off an action
- `Input.reset_bindings(Symbol | nil name = nil, pad: Integer | nil = nil) -> void`: back to the game's own bindings (of one action, or all; of one pad, or all)
- `Input.capture_next(cancel: Array[String] = ["key:escape", "pad:b", "pad:back"], mouse: bool = true) { (String | nil, Integer | nil) -> void } -> void`: the next key, mouse button, wheel turn or gamepad button pressed, to the block: |binding, pad| (nil: called off); mouse: false leaves the mouse out
- `Input.capturing? -> bool`: whether a capture waits for a key
- `Input.capturing_for?(Proc block) -> bool`: whether a capture still waits to call that block (not once another took its place)
- `Input.cancel_capture -> void`: calls a capture off
- `Input.mouse_x -> Float`: the pointer's x, in pixels
- `Input.mouse_y -> Float`: the pointer's y, in pixels
- `Input.mouse_dx -> Float`: how far the mouse moved this frame, across
- `Input.mouse_dy -> Float`: how far the mouse moved this frame, up and down
- `Input.mouse_wheel -> Float`: how far the wheel turned this frame
- `Input.mouse_down?(Symbol button) -> bool`: whether a mouse button (:left, :right, :middle, :side, :extra) is held
- `Input.mouse_pressed?(Symbol button) -> bool`: whether a mouse button went down this frame
- `Input.mouse_released?(Symbol button) -> bool`: whether a mouse button went up this frame
- `Input.mouse_locked = bool locked`: hides the pointer and keeps it in the window (first-person cameras)
- `Input.mouse_locked? -> bool`: whether the pointer is locked
- `Input.pads -> Array[Integer]`: the numbers of the gamepads plugged in
- `Input.gamepad?(pad: Integer = 0) -> bool`: whether a gamepad is connected
- `Input.gamepad_axis(Symbol axis, pad: Integer = 0) -> Float`: :left_x, :left_y, :right_x, :right_y, :left_trigger, :right_trigger
- `Input.gamepad_down?(Symbol button, pad: Integer = 0) -> bool`: whether a gamepad button (:a, :b, :x, :y...) is held
- `Input.gamepad_pressed?(Symbol button, pad: Integer = 0) -> bool`: whether a gamepad button went down this frame
- `Input.gamepad_released?(Symbol button, pad: Integer = 0) -> bool`: whether a gamepad button went up this frame
- `Input.text -> String`: what was typed this frame
- `Input.touch_controls = bool on`: shows the on-screen stick and buttons of a phone
- `Input.touch_controls? -> bool`: whether they are shown

Every action, axis, binding and gamepad call takes `pad:` (0 to 3: the player; 0 when left out). Actions and axes come from `game.json`'s `input` section and `define_action`/`define_axis`; a player's bindings are kept between runs, beside the game's save. See [Controls](gameplay.md#controls).

## Time

The game's clock (added to Ruby's Time).

- `Time.delta -> Float`: seconds since the last frame
- `Time.unscaled_delta -> Float`: seconds since the last frame, whatever Time.scale (also while paused)
- `Time.fixed_delta -> Float`: seconds of a physics step (1/60)
- `Time.elapsed -> Float`: seconds since the game started
- `Time.frame -> Integer`: how many frames have run
- `Time.scale -> Float`: how fast game time runs (1 is normal)
- `Time.scale = Numeric value`: slows or speeds the game; 0 pauses it

## Physics

Questions to the physics world: rays, moving shapes and shapes in place. See [gameplay.md](gameplay.md).

- `Physics.raycast(Vector3 origin, Vector3 direction, Numeric max_distance = 1000.0, GameObject | nil skip = nil, layers: Array[Symbol] | Symbol | nil = nil, ignore: Array[GameObject] | GameObject | nil = nil) -> Hash[Symbol, untyped] | nil`: the first thing a ray hits: {object:, point:, normal:, distance:}, or nil (skip: an object it sees through)
- `Physics.raycast_all(Vector3 origin, Vector3 direction, Numeric max_distance = 1000.0, layers: Array[Symbol] | Symbol | nil = nil, ignore: Array[GameObject] | GameObject | nil = nil) -> Array[Hash[Symbol, untyped]]`: everything a ray hits, nearest first
- `Physics.sphere_cast(Vector3 from, Vector3 to, Numeric radius, layers: Array[Symbol] | Symbol | nil = nil, ignore: Array[GameObject] | GameObject | nil = nil) -> Hash[Symbol, untyped] | nil`: the first thing a ball moving from `from` to `to` touches, or nil
- `Physics.box_cast(Vector3 from, Vector3 to, Vector3 size, rotation: Vector3 | nil = nil, layers: Array[Symbol] | Symbol | nil = nil, ignore: Array[GameObject] | GameObject | nil = nil) -> Hash[Symbol, untyped] | nil`: the same for a box (its whole size; rotation in degrees)
- `Physics.capsule_cast(Vector3 from, Vector3 to, Numeric radius, Numeric height, rotation: Vector3 | nil = nil, layers: Array[Symbol] | Symbol | nil = nil, ignore: Array[GameObject] | GameObject | nil = nil) -> Hash[Symbol, untyped] | nil`: the same for an upright capsule (height end to end)
- `Physics.overlap_sphere(Vector3 center, Numeric radius, layers: Array[Symbol] | Symbol | nil = nil, ignore: Array[GameObject] | GameObject | nil = nil) -> Array[Hash[Symbol, untyped]]`: the hits of everything inside a ball, nearest first
- `Physics.overlap_box(Vector3 center, Vector3 size, rotation: Vector3 | nil = nil, layers: Array[Symbol] | Symbol | nil = nil, ignore: Array[GameObject] | GameObject | nil = nil) -> Array[Hash[Symbol, untyped]]`: the same inside a box
- `Physics.overlap_capsule(Vector3 center, Numeric radius, Numeric height, rotation: Vector3 | nil = nil, layers: Array[Symbol] | Symbol | nil = nil, ignore: Array[GameObject] | GameObject | nil = nil) -> Array[Hash[Symbol, untyped]]`: the same inside a capsule
- `Physics.layers -> Array[Symbol]`: the game's collision layers, as Symbols (game.json's physics layers)
- `Physics.joint(Symbol kind, GameObject object, GameObject | nil other = nil, anchor: Vector3 | nil = nil, other_anchor: Vector3 | nil = nil, axis: Vector3 | nil = nil, limits: Array[Numeric] | nil = nil, motor_speed: Numeric | nil = nil, motor_torque: Numeric | nil = nil, min: Numeric | nil = nil, max: Numeric | nil = nil, spring: Numeric | nil = nil, damping: Numeric | nil = nil, break_force: Numeric | nil = nil) -> Joint`: joins two objects, or one to the world (other nil): :fixed, :hinge, :slider, :point or :distance

`layers:` is a layer's name or a list of them (every layer when nil); `ignore:` is an object or a list of objects to see through. Hits are `{object:, point:, normal:, distance:}`, each object once, nearest first, at most 64; triggers are never hit. A cast that starts inside something hits it at distance 0.

## Joint

What holds two bodies together, or one to the world (made by Physics.joint). See [gameplay.md](gameplay.md).

- `kind -> Symbol`: :fixed, :hinge, :slider, :point or :distance
- `object -> GameObject`: the object it holds
- `other -> GameObject | nil`: the other object it holds, or nil for the world
- `alive? -> bool`: whether it still holds them
- `broken? -> bool`: whether it was pulled past its break force
- `remove -> void`: lets go
- `enabled? -> bool`: whether it holds right now
- `enabled = bool flag`: lets go for a while, or holds again
- `break_force -> Float`: the pull (newtons) or twist (newton meters) it breaks at; 0 never breaks
- `break_force = Numeric newtons`: sets the pull it breaks at
- `motor(Numeric speed, Numeric torque) -> void`: a hinge turns at that speed (degrees per second) with at most that torque; 0 torque lets it go
- `angle -> Float`: how far a hinge turned from where it started, in degrees

Points and axes are in world space; `anchor:` defaults to the object's position, `other_anchor:` (a distance joint's other end) to the other object's, `axis:` to up. A hinge's `limits:` are degrees [low, high] from where it starts (low -180 to 0, high 0 to 180), a slider's are meters along its axis; a hinge's `motor_speed:` needs a `motor_torque:`. A distance joint keeps between `min:` (0) and `max:` (how far apart the ends are) apart, springy with `spring:` (hertz) and `damping:`. Each kind refuses options it has no use for, and `motor` and `angle` raise for anything but a hinge. A joint needs a dynamic body at one end (one whose bodies were all made kinematic since holds nothing until one is dynamic again); it goes when either object is destroyed or the scene changes.

## Navigation

Where the characters a game moves can walk, and the way from one place to another. See [gameplay.md#chase-the-player-navigation](gameplay.md#chase-the-player-navigation).

- `Navigation.path(Vector3 from, Vector3 to) -> Array[Vector3] | nil`: the points to walk through from near `from` to near `to` (Vector3s, both ends included), or nil when there is no way there
- `Navigation.nearest(Vector3 point) -> Vector3 | nil`: the walkable point nearest a point (within 8 m of it), or nil
- `Navigation.walkable?(Vector3 point) -> bool`: whether a point is on walkable ground, up to a character's height above it
- `Navigation.draw(Vector3 around, Numeric radius = 10, color: Color = :blue, alpha: Integer = 255) -> Integer`: shows the walkable cells within a radius for this frame, as Debug.box does; returns how many
- `Navigation.bake -> Hash[Symbol, Numeric | bool]`: bakes the scene's grid again now (a level a script changed); returns what stats does
- `Navigation.stats -> Hash[Symbol, Numeric | bool]`: how the grid was baked: {columns:, cells:, cell_size:, cells_enlarged:, milliseconds:}

The grid is baked from the scene's static bodies as it loads when game.json has a `navigation` section (`cell_size` 0.25, `agent_radius` 0.4, `agent_height` 1.8, `max_step` 0.4, `max_slope` 45 degrees, `bake_on_load` true), and otherwise when a script first asks. Moving bodies, characters and triggers are not obstacles, and a level changed after the bake is not seen until `Navigation.bake`. A path goes through cells side by side no more than `max_step` apart, never cutting a corner, and is pulled straight. A search looks at no more than 16384 cells: a way longer than that gives nil, as one to a place nobody can reach does. A grid has at most 262144 columns: a larger level is baked with larger cells, with a warning.

## CharacterController

The body of an object with body = "character": walks, jumps, climbs steps. See [gameplay.md](gameplay.md).

- `move(Vector3 velocity) -> void`: walks with that velocity (m/s) this frame
- `jump(Numeric speed) -> void`: jumps up at that speed
- `grounded? -> bool`: whether it stands on something
- `velocity -> Vector3`: its velocity
- `layer -> Symbol`: its collision layer, a Symbol
- `layer = Symbol name`: moves it to another collision layer

## Rigidbody

The body of an object with body = "dynamic" or "kinematic" (or a vehicle's chassis). See [gameplay.md](gameplay.md).

- `velocity -> Vector3`: its velocity
- `velocity = Vector3 vector`: sets its velocity
- `add_impulse(Vector3 vector) -> void`: pushes it
- `add_force(Vector3 force, at: Vector3 | nil = nil) -> void`: pushes it during the next step (newtons); at a point (world space), it also turns it
- `add_torque(Vector3 torque) -> void`: turns it during the next step (newton meters)
- `angular_velocity -> Vector3`: how fast it spins, degrees per second around each axis
- `angular_velocity = Vector3 vector`: sets how fast it spins
- `mass -> Float`: its mass in kilograms
- `mass = Numeric kilograms`: changes its mass (more than 0)
- `kinematic? -> bool`: whether scripts move it (instead of forces)
- `kinematic = bool flag`: turns a dynamic body kinematic, or back
- `dynamic? -> bool`: whether forces move it
- `dynamic = bool flag`: turns a kinematic body dynamic, or back
- `gravity_scale -> Float`: how much gravity pulls it (1 is normal)
- `gravity_scale = Numeric scale`: 0 floats, 2 falls twice as hard
- `sleep -> void`: stops simulating it until something touches it
- `wake -> void`: simulates it again
- `sleeping? -> bool`: whether it sleeps
- `layer -> Symbol`: its collision layer, a Symbol
- `layer = Symbol name`: moves it to another collision layer

Forces and torques act during the next physics step only: add them every `fixed_update` to keep pushing. A body made dynamic again starts at rest. `kinematic=` and `dynamic=` raise for a vehicle.

## Vehicle

The body of an object with body = "vehicle" and wheels. See [gameplay.md](gameplay.md).

- `drive(throttle: Numeric = 0.0, steering: Numeric = 0.0, brake: Numeric = 0.0, handbrake: Numeric = 0.0) -> void`: drives it this frame (each from -1 or 0 to 1)
- `stop -> void`: brakes fully
- `speed -> Float`: its speed in m/s, signed
- `velocity -> Vector3`: its velocity

Wheel meshes must have their rotation applied (the axle along X). `drive`'s inputs are the one setting the engine clamps, since an analog stick may push a little past full.

## Camera

The camera the game is seen through, and what it does on its own: follow, orbit, shake. See [gameplay.md#follow-orbit-and-shake-the-camera](gameplay.md#follow-orbit-and-shake-the-camera).

- `Camera.main -> GameObject | nil`: the camera object the game is seen through: the one chosen with main= or cut_to, else the scene's first
- `Camera.main = GameObject | nil camera`: sees the game through another camera object at once (a cut in progress stops); nil goes back to the scene's first; a scene loaded anew starts at its first
- `Camera.cut_to(GameObject camera, fade: Numeric = 0) -> GameObject`: cuts to another camera; with a fade (seconds), through black: darker over its first half, cut at the darkest frame, back over the second half, also while paused; returns the camera
- `Camera.cutting? -> bool`: whether a cut with a fade is still playing
- `Camera.screen_ray(Numeric x, Numeric y) -> [Vector3, Vector3] | nil`: [origin, direction] through a pixel of the screen
- `Camera.world_to_screen(Vector3 | GameObject point) -> [Float, Float] | nil`: [x, y]: where a Vector3 (or an object) is on the screen, in the pixels UI draws in; nil behind the camera
- `Camera.fov -> Float | nil`: its vertical field of view, in degrees
- `Camera.fov = Numeric degrees`: sets the field of view
- `Camera.motion_blur -> Float`: how much the picture streaks with speed, 0 to 1
- `Camera.motion_blur = Numeric amount`: sets it
- `Camera.follow(GameObject target, offset: Vector3 | Array[Numeric] = [0, 3, 6], relative: bool = false, smoothing: Numeric = 0.15, look_at: bool = true, look_height: Numeric = 1.0, avoid_walls: bool = true, dead_zone: Numeric = 0.0, snap: bool = true) -> void`: follows an object from now on: offset in meters (relative: in its own axes), smoothing in seconds to close half the way, in front of walls between; snap: false flies there
- `Camera.orbit(GameObject | Vector3 target, distance: Numeric = 6.0, yaw: Numeric = 0.0, pitch: Numeric = 20.0, distances: Range[Numeric] = 1.0..30.0, pitches: Range[Numeric] = -80.0..80.0, smoothing: Numeric = 0.08, input: bool = true, height: Numeric = 1.0, avoid_walls: bool = true) -> Camera::Orbit`: orbits an object or a point, looking at it, turned by dragging with the right mouse button or the right stick and zoomed by the wheel, also while paused; returns a Camera::Orbit
- `Camera.stop_following -> void`: stops following or orbiting; the camera stays where it is
- `Camera.following? -> bool`: whether it follows or orbits something (an object destroyed is let go)
- `Camera.shake(Numeric strength, duration: Numeric = 0.5) -> Float`: shakes what it shows (never the camera object): strength (0 to 1) adds to its trauma, held at 1, fading over duration seconds
- `Camera.trauma -> Float`: how much it shakes now, 0 to 1
- `Camera.stop_shaking -> void`: stops the shake at once

## Camera::Orbit

What Camera.orbit returns: the camera's angles and distance around what it orbits, for scripts to read and change. See [gameplay.md#follow-orbit-and-shake-the-camera](gameplay.md#follow-orbit-and-shake-the-camera).

- `yaw -> Float`: degrees around it, -180 to 180: 0 behind it (its +Z side), 90 on its right
- `pitch -> Float`: degrees above it
- `distance -> Float`: meters from it
- `target -> GameObject | Vector3`: the object or point it orbits
- `turn(Numeric yaw, Numeric pitch) -> void`: turns by degrees (the yaw wraps round, eased the short way), the pitch kept within its range
- `zoom(Numeric meters) -> void`: comes nearer (negative) or goes further, within its range

## Light

game_object.light: the light an object shines with. See [gameplay.md#light-that-flickers-and-fades](gameplay.md#light-that-flickers-and-fades).

- `intensity -> Float`: watts for a point or spot light, lux for the sun
- `intensity = Numeric value`: sets it; shows from the next frame
- `color -> [Integer, Integer, Integer]`: its color [r, g, b], 0 to 255
- `color = Color color`: :orange or [r, g, b] from 0 to 255
- `cone_angles -> [Float, Float] | nil`: [inner, outer] in degrees for a spot light, or nil for other lights
- `set_cone(inner: Numeric, outer: Numeric) -> nil`: sets both spot angles between 0 and 90 degrees, at least 0.1 degrees apart

## Animation

The scene's own animations (exported from Blender), by name. See [animation.md](animation.md).

- `Animation.names -> Array[String]`: the names of the scene's animations
- `Animation.play(String | Symbol name, loop: bool = true, speed: Numeric = 1.0, restart: bool = false) -> void`: plays one
- `Animation.pause(String | Symbol name) -> void`: pauses one
- `Animation.stop(String | Symbol name) -> void`: stops one
- `Animation.playing?(String | Symbol name) -> bool`: whether it plays
- `Animation.time(String | Symbol name) -> Float`: where it is, in seconds
- `Animation.duration(String | Symbol name) -> Float`: how long it is, in seconds
- `Animation.seek(String | Symbol name, Numeric seconds) -> void`: jumps to a moment

## Animator

game_object.animation (animation in a behaviour): an object's own clips. See [animation.md](animation.md).

- `play(String | Symbol name, loop: bool = true, speed: Numeric = 1.0, fade: Numeric = 0.0, restart: bool = false) -> void`: plays a clip, fading from the last one
- `mix(String | Symbol name, weight: Numeric = 1.0, loop: bool = true, speed: Numeric = 1.0) -> void`: plays a clip over the one playing
- `unmix -> void`: stops the clip mixed over
- `pause -> void`: pauses its clips
- `follow_scene(fade: Numeric = 0.0) -> void`: goes back to the scene's animations
- `clip -> String | nil`: the clip playing
- `playing? -> bool`: whether a clip plays
- `time -> Float`: where the clip is, in seconds
- `use(Animation::Graph graph) -> void`: installs an Animation::Graph
- `set(Symbol parameter, untyped value) -> void`: sets a parameter of the graph
- `state -> Symbol | nil`: the graph's state, or nil

## Animation::Graph

Named states and transitions for one character (animation.use(graph)). See [animation.md](animation.md).

- `Animation::Graph.new(initial: Symbol, parameters: Hash[Symbol, untyped] = {}) -> Animation::Graph`: a graph starting at a state, with its parameters and their defaults
- `clip(Symbol name, String | Symbol clip, loop: bool = true, speed: Numeric = 1.0) -> self`: a state that plays one clip
- `blend_1d(Symbol name, parameter: Symbol, clips: Array[[Numeric, String | Symbol]], loop: bool = true, speed: Numeric = 1.0) -> self`: a state that blends clips by a number: clips: [[0.0, :Idle], [5.0, :Run]]
- `blend_2d(Symbol name, parameters: [Symbol, Symbol], clips: Array[[[Numeric, Numeric], String | Symbol]], loop: bool = true, speed: Numeric = 1.0) -> self`: a rectangular two-dimensional grid: parameters: [:x, :y], clips: [[[x, y], :Clip], ...]
- `transition(Symbol from, Symbol to, parameter: Symbol, equals: untyped = nil, above: Numeric | nil = nil, below: Numeric | nil = nil, fade: Numeric = 0.0) -> self`: goes from one state to another when a parameter says so

## Audio

Sounds and music from the game's sounds/ folder.

- `Audio.play(String | Symbol name, volume: Numeric = 1.0, pitch: Numeric = 1.0, loop: bool = false, group: Symbol = :effects) -> Integer | nil`: plays a sound; returns its id (for stop, playing? and set), or nil
- `Audio.music(String | Symbol name, loop: bool = true, volume: Numeric = 0.7, fade: Numeric = 0.0) -> void`: plays music, crossfading over fade seconds from the track playing
- `Audio.stop_music(fade: Numeric = 0.0) -> void`: stops the music, fading it out over fade seconds
- `Audio.music_pitch = Numeric pitch`: changes the music's pitch
- `Audio.music_volume = Numeric volume`: changes the music's volume
- `Audio.volume = Numeric volume`: the volume of everything
- `Audio.set(Integer id, volume: Numeric | nil = nil, pitch: Numeric | nil = nil) -> bool`: changes the volume or pitch of a sound that plays; false once it stopped
- `Audio.play_at(String | Symbol name, Vector3 position, loop: bool = false, volume: Numeric = 1.0, pitch: Numeric = 1.0, reference_distance: Numeric = 1.0, max_distance: Numeric = 50.0, group: Symbol = :effects) -> Integer | nil`: plays a sound at a place in the world; returns its id, or nil
- `Audio.listener(Vector3 position, Vector3 forward, Vector3 up) -> void`: where the ears are (move it with the camera)
- `Audio.move(Integer id, Vector3 position) -> void`: moves a sound played with play_at
- `Audio.playing?(Integer id) -> bool`: whether that sound still plays
- `Audio.stop(Integer id) -> void`: stops that sound
- `Audio.stop_sources -> void`: stops every sound played with play or play_at

Every sound played gets an id, flat (`play`) or positional (`play_at`) alike: `stop`, `playing?` and `set` take either. A sound that does not loop is decoded once and each play is a voice of its own; loops and music stream, so a crossfade plays two tracks for its length. A sound plays in a mix group, `:music`, `:effects` (the default) or `:voice`, and is heard at its volume times the player's volume of its group (`Settings.music_volume`, `effects_volume`, `voice_volume`) times the master volume. A track that cannot play leaves the one playing as it was. `play_at` sources use stereo panning and inverse-distance volume, fading to silence at `max_distance`. `volume` must be 0 or more and `pitch` from 0.01 to 8; `0 < reference_distance < max_distance`. Positions and directions may be arrays or `Vector3`. Update `Audio.listener(position, forward, up)` when the camera moves: nothing follows an object by itself. At most 64 sounds with ids play at once; old ids are never reused.

## UI

What a script draws over the game this frame: texts, rectangles, pictures, buttons. See [gameplay.md](gameplay.md).

- `UI.text(String str, x: Numeric, y: Numeric, size: Numeric = 24, color: Color = :white, alpha: Integer = 255, align: Symbol = :left, width: Numeric | nil = nil, anchor: Symbol | nil = nil) -> void`: draws a text; width: wraps it at word boundaries
- `UI.rect(x: Numeric, y: Numeric, width: Numeric, height: Numeric, color: Color = :black, alpha: Integer = 255, radius: Numeric = 0, anchor: Symbol | nil = nil) -> void`: draws a rectangle
- `UI.image(String | Symbol name, x: Numeric, y: Numeric, width: Numeric = 0, height: Numeric = 0, alpha: Integer = 255, anchor: Symbol | nil = nil) -> void`: draws a picture of images/
- `UI.button(String label, x: Numeric, y: Numeric, width: Numeric, height: Numeric, color: Color = :dark, hover_color: Color = :ruby, text_color: Color = :white, size: Numeric | nil = nil, focused: bool = false, confirm: Array[Array[Symbol]] = [[:enter], [:a]], anchor: Symbol | nil = nil) -> bool`: draws a button; true the frame it is clicked
- `UI.text_field(String value, x: Numeric, y: Numeric, width: Numeric, height: Numeric, max_length: Integer = 64, color: Color = :dark, text_color: Color = :white, secret: bool = false, anchor: Symbol | nil = nil) -> String`: a field to type in; returns the new value
- `UI.keyboard_visible? -> bool`: whether a phone's on-screen keyboard covers the bottom of the screen
- `UI.width -> Integer`: the width of the screen, in pixels
- `UI.height -> Integer`: the height of the screen, in pixels
- `UI.text_width(String str, size: Numeric = 24) -> Integer`: how wide a text is, in pixels (in the face selected)
- `UI.wrap(String str, width: Numeric, size: Numeric = 24) -> Array[String]`: the lines a text wraps to at word boundaries, none wider than width
- `UI.text_height(String str, size: Numeric = 24, width: Numeric | nil = nil) -> Integer`: how tall a text is drawn, in pixels (wrapped to width, when given)
- `UI.line_height(Numeric size) -> Integer`: pixels from a line's top to the next one's at a text size
- `UI.anchored(Symbol anchor, x: Numeric, y: Numeric, width: Numeric = 0, height: Numeric = 0) -> [Integer, Integer]`: the top-left [x, y] of a box placed at an anchor, x and y from it
- `UI.clip(x: Numeric, y: Numeric, width: Numeric, height: Numeric, anchor: Symbol | nil = nil) { () -> void } -> void`: draws what the block draws only inside a box
- `UI.toggle(String label, bool on, x: Numeric, y: Numeric, width: Numeric, height: Numeric, focused: bool = false, anchor: Symbol | nil = nil) -> bool`: an on/off row of a settings screen; returns whether it is on
- `UI.choice(String label, Array[String] choices, Integer index, x: Numeric, y: Numeric, width: Numeric, height: Numeric, focused: bool = false, anchor: Symbol | nil = nil) -> Integer`: a row that picks one of choices; returns the index chosen
- `UI.slider(String label, Numeric value, x: Numeric, y: Numeric, width: Numeric, height: Numeric, min: Numeric = 0.0, max: Numeric = 1.0, step: Numeric = 0.05, focused: bool = false, anchor: Symbol | nil = nil) -> Float`: a row with a slider; returns the value
- `UI.font -> String | nil`: the face texts are drawn with: a file name of fonts/, or nil for the built-in one
- `UI.font = String | nil name`: draws the next texts with fonts/<name>.ttf or .otf (nil: the built-in face)

A focused widget (`focused: true`) is worked by Enter or a gamepad's A, and left and right change it. An anchor (`:top_left`, `:top`, `:top_right`, `:left`, `:center`, `:right`, `:bottom_left`, `:bottom`, `:bottom_right`) makes x and y offsets from that point of the screen, and puts the matching corner of what is drawn there: `UI.text("Lives", anchor: :top_right, x: -20, y: 20)`. With an anchor, a text's box is as wide as `width:` or its widest line, and `align:` lays its lines out inside it; an image is placed by the width and height given. A text breaks at its own line breaks, and with `width:` also at spaces; a word longer than a line is cut between letters. Lines are `UI.line_height(size)` apart (1.25 times the size). Inside `UI.clip`, the pointer outside the box is over nothing drawn in it. The built-in face adds glyphs as they are asked for (Greek, Cyrillic, accented letters, `€`); for another face or script, put a `.ttf` or `.otf` in `fonts/` and set `UI.font`. A missing or unreadable face falls back to the built-in one, with a warning; glyphs a face lacks show as `?`; there is no bidirectional layout or complex-script shaping.

## UI::Focus

Which widget of a screen the keyboard and a gamepad work: up and down move it, wrapping. See [gameplay.md](gameplay.md).

- `UI::Focus.new -> UI::Focus`: a focus on the first widget
- `update(Integer count) -> void`: moves it among count widgets, as up or down was pressed this frame
- `at?(Integer index) -> bool`: whether the widget of that index has it: pass it as focused:
- `move_to(Integer index) -> void`: puts it on a widget (one clicked, or the one a screen comes back to)
- `index -> Integer`: the widget that has it
- `reset -> void`: back to the first widget

## UI::ScrollList

Rows in a box that scrolls, drawn only inside it: the wheel, Page Up/Down, a gamepad's bumpers and right stick, and a focus. See [gameplay.md](gameplay.md).

- `UI::ScrollList.new -> UI::ScrollList`: a list scrolled to its first row
- `draw(Integer count, x: Numeric, y: Numeric, width: Numeric, height: Numeric, row_height: Numeric, gap: Numeric = 0, focus: UI::Focus | nil = nil, first: Integer = 0, anchor: Symbol | nil = nil, input: bool = true) { (Integer, Hash[Symbol, Integer]) -> void } -> void`: draws count rows in the box, calling the block with each shown row's index and box
- `offset -> Numeric`: how far it is scrolled, in pixels from its first row's top
- `offset = Numeric pixels`: scrolls it (kept within its rows when it is drawn)
- `shown -> Range[Integer]`: the indexes of the rows that show, as a range
- `scrolls? -> bool`: whether its rows are taller than its box
- `reset -> void`: back to its first row

Given a focus (`UI::Focus`), the list scrolls to the row the focus moves to, and Page Up and Page Down move the focus a page; row 0 is the focus' `first`, so rows may come after other widgets. Without one, up and down (arrows, D-pad) scroll a row. The wheel scrolls it while the pointer is over it, and scrolling away from the focused row stays until the focus moves. `input: false` leaves the keys and the wheel to something else that frame. A bar beside its right edge shows how far it is scrolled.

## Debug

Developer diagnostics: console messages, frame timings and immediate world drawing.

- `Debug.log(untyped message) -> void`: says something
- `Debug.warn(untyped message) -> void`: warns
- `Debug.error(untyped message) -> void`: reports an error
- `Debug.profiling? -> bool`: whether foreground frame timings are being collected
- `Debug.profiling = bool enabled`: starts a fresh profile window when enabled; disabling retains the last summary
- `Debug.stats_visible? -> bool`: whether the developer statistics overlay was requested
- `Debug.stats_visible = bool visible`: shows frame timings and FPS; showing also enables profiling, hiding leaves collection running
- `Debug.frame_stats -> Hash[Symbol, untyped]`: a snapshot with samples, fps and latest/mean/peak timings in milliseconds
- `Debug.reset_frame_stats -> void`: clears statistics and discards any unfinished measurement
- `Debug.line(Vector3 from, Vector3 to, color: Color = :green, alpha: Integer = 255, depth_test: bool = true) -> bool`: draws a world-space line for this frame; returns whether it was accepted
- `Debug.box(Vector3 center, Vector3 size, color: Color = :green, alpha: Integer = 255, depth_test: bool = true) -> bool`: draws an axis-aligned wire box with full nonnegative size for this frame; returns whether it was accepted

Profiling is off by default. The overlay uses the shared game HUD, including editor Play and recordings; it is hidden when profiling is off. Timings use the real steady clock, independently of Time.scale or fixed-step replay. The most recent 120 completed frames form the window; reads during a frame expose only completed frames. FPS uses completion-to-completion intervals, and is zero until an interval is available. `latest`, `mean` and `peak` contain `frame_ms`, `interval_ms`, `has_interval`, and `input_ms`, `network_ms`, `http_ms`, `update_ms`, `physics_ms`, `presentation_ms`, `scene_ms`. Input includes pending script startup; network and HTTP include their callbacks; update includes animations, scripts, particles, loading UI and physics. Physics includes fixed-update callbacks, body synchronization and contacts, so its time overlaps update. Presentation includes uploads, drawing and any frame-cap or synchronization waits. Scene includes request polling, synchronous reads and activation; the asynchronous worker is not instrumented, while offline fixed-step waits count as scene wall time. These are foreground wall times, not GPU timers or CPU utilization. Phase times are inclusive and must not be summed as a partition. Overlay timings and captures are nondeterministic; leave the overlay off for reproducible pictures. Debug.line and Debug.box use world coordinates and live for one frame: submit from ready/update/fixed_update. At most 4096 commands are accepted per frame; later calls return false. They are local-only and return false without a presenter, while still validating arguments. depth_test: false draws through objects. Shape frames use unjittered rendering and reset temporal history; normal temporal sampling resumes afterwards. Native windows allocate one cached color/depth target only when shapes are requested; editor, scaled and recorded views reuse their target. If that extra native target is unavailable, the game still draws with a warning and the shapes are omitted.

## Game

The game itself. See [gameplay.md#a-loading-screen](gameplay.md#a-loading-screen).

- `Game.load_scene(String | Symbol name) -> void`: switches to scenes/<name>.glb ("main" is scene.glb); Ruby globals survive
- `Game.load_scene_async(String | Symbol name) -> void`: queues a background scene read; the old world survives failed reads and cancellation
- `Game.loading? -> bool`: a scene request is queued or being read
- `Game.loading_scene -> String | nil`: the requested scene name, or nil
- `Game.loading_error -> String | nil`: the last asynchronous read error, or nil; cleared by the next load request
- `Game.cancel_scene_load -> bool`: cancels a pending asynchronous request; returns whether one was pending
- `Game.on_loading ?{ (Float) -> void } -> void`: draws the loading screen each frame with unscaled delta; no block restores the default
- `Game.scene_name -> String`: the scene running
- `Game.title -> String`: the game's title
- `Game.quit -> void`: ends the game
- `Game.stop_recording -> void`: ends a video recorded with --record; the game goes on
- `Game.version -> String | nil`: the version in game.json ("1.2.0"), or nil

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

- `Settings.window_mode -> Symbol`: :windowed, :fullscreen or :borderless
- `Settings.window_mode = Symbol mode`: changes it
- `Settings.fullscreen? -> bool`: whether the game fills the screen
- `Settings.fullscreen = bool on`: fullscreen, or back to a window
- `Settings.resolution -> [Integer, Integer]`: [width, height] of the window, in pixels
- `Settings.resolution = [Integer, Integer] size`: resizes the window: one of resolutions
- `Settings.resolutions -> Array[[Integer, Integer]]`: the sizes the monitor offers: [[w, h], ...]
- `Settings.render_scale -> Float`: the share of the window's size the world is drawn at (0.5 to 2)
- `Settings.render_scale = Numeric scale`: changes it
- `Settings.vsync -> bool`: whether frames wait for the screen
- `Settings.vsync = bool on`: changes it
- `Settings.fps_cap -> Integer | nil`: frames a second at most; nil for none
- `Settings.fps_cap = Integer | nil cap`: changes it
- `Settings.quality -> Symbol`: :low, :medium, :high, :ultra or :custom
- `Settings.quality = Symbol quality`: the most a level's environment gets of what it asks for
- `Settings.quality_details -> Hash[Symbol, untyped]`: what the custom quality allows: {msaa:, antialias:, shadow_cascades:, ...}
- `Settings.quality_details = Hash[Symbol, untyped] details`: changes some of them, and makes the quality :custom
- `Settings.volume -> Float`: the player's volume, 0 to 1 (it multiplies Audio.volume)
- `Settings.volume = Numeric volume`: changes it
- `Settings.music_volume -> Float`: the player's volume of the music group, 0 to 1
- `Settings.music_volume = Numeric volume`: changes it
- `Settings.effects_volume -> Float`: the player's volume of the effects group, 0 to 1
- `Settings.effects_volume = Numeric volume`: changes it
- `Settings.voice_volume -> Float`: the player's volume of the voice group, 0 to 1
- `Settings.voice_volume = Numeric volume`: changes it
- `Settings.save -> bool`: keeps them for the next runs; false where they last for the run only

Until the player chooses, everything is what `game.json` says. `Settings.save` keeps them only in a built game a player plays: in the editor's Play, `redgem run`, a server's rooms and tests they last for the run, and `save` returns false.

## GameMenu

A behaviour: title screen, pause menu, end screen and settings screen with its controls page; arrows or the D-pad move the focus, Enter or A picks. See [gameplay.md#pause-and-the-flow-between-screens](gameplay.md#pause-and-the-flow-between-screens).

- `GameMenu.instance -> GameMenu | nil`: the menu of the scene, or nil
- `show(Symbol screen) -> void`: opens a screen: :title, :pause, :over, :settings or one of the game's
- `show_settings -> void`: opens the settings screen (window, resolution, quality, volume...; its Controls button opens the controls page)
- `resume -> void`: closes the menu and goes on
- `restart -> void`: starts the scene again
- `finish(String message) -> void`: opens the end screen with a message
- `open? -> bool`: whether the menu is open
- `screen -> Symbol | nil`: the screen open
- `screens -> Hash[Symbol, GameMenu::Screen]`: the screens, by name (:title, :pause, :over, :settings, :controls and the game's own)
- `button_box(Integer index) -> [Integer, Integer, Integer, Integer]`: where the menu's button of that index is: [x, y, width, height]

## GameMenu::Screen

A screen of the GameMenu. See [gameplay.md#pause-and-the-flow-between-screens](gameplay.md#pause-and-the-flow-between-screens).

- `GameMenu::Screen.new(heading: String | Proc, options: Array[[String | Proc, Proc]], back: Symbol | nil) -> GameMenu::Screen`: a screen with a heading and options: [[label, action], ...]
- `add_option(String | Proc label, index: Integer = -1) { () -> void } -> void`: adds an option
- `heading -> String`: its heading
- `heading = String | Proc text`: changes its heading
- `options -> Array[[String, Proc]]`: its options
- `back -> Symbol | nil`: the screen Esc goes back to
- `opened -> void`: called when it is shown
- `closed -> void`: called when it is left
- `draw([Integer, Integer, Integer, Integer] first_button) -> void`: draws what else it shows
- `space_above -> Integer`: pixels kept between its heading and its buttons
- `typing? -> bool`: whether letters are typed into it
- `rows -> Integer`: how many widgets it draws above its buttons that take the focus
- `first_button_share -> Float | nil`: where its first button sits, a share of the height (nil: the menu's)
- `focus -> UI::Focus | nil`: its UI::Focus, which the menu gives it
- `focus = UI::Focus | nil focus`: sets it

## Particles

Short-lived sparks, smoke and dust. See [gameplay.md#sparks-dust-and-smoke](gameplay.md#sparks-dust-and-smoke).

- `Particles.burst(Vector3 position, count: Integer = 12, color: Color | Array[Color] = :white, alpha: Integer | Array[Integer] = 255, speed: Numeric = 3.0, spread: Numeric = 180.0, direction: Vector3 = Vector3.up, life: Numeric = 0.8, size: Numeric | Array[Numeric] = 0.12, gravity: Numeric = 1.0, glow: bool = false, lit: bool = false, cast_shadow: bool = false, soft: Numeric = 0.0, image: String | Symbol | nil = nil, frames: [Integer, Integer] = [1, 1], fps: Numeric = 0.0, spin: Numeric = 0.0) -> Integer`: adds particles; returns how many (size, color and alpha may be lists over their life)
- `Particles.emitter(GameObject object, rate: Numeric = 24.0, offset: Vector3 = Vector3.zero, **untyped settings) -> Particles::Emitter`: starts a continuous local effect; returns a Particles::Emitter handle; uses burst settings except count
- `Particles.count -> Integer`: how many are alive
- `Particles.clear -> void`: removes all particles and stops every emitter

Emitters follow game time and stop on object destruction or scene changes. They are local visual effects, not replicated. Without a presenter, creation returns an inactive handle and no emitter is allocated or simulated. Births follow ordinary movement; movement over 10 meters per update starts them at the destination. Up to 256 emitters fairly share 4096 live particles; missed births are dropped.

## Particles::Emitter

A continuous particle effect attached to an object, returned by Particles.emitter. See [gameplay.md#sparks-dust-and-smoke](gameplay.md#sparks-dust-and-smoke).

- `active? -> bool`: whether this emitter is still emitting from a live object
- `stop -> void`: stops new births; existing particles finish their lives

## Scatter

Great numbers of copies of a mesh (grass, trees, rocks), drawn only. See [../levels.md#grass-forests-and-cities](../levels.md#grass-forests-and-cities).

- `Scatter.place(String | GameObject template, count: Integer, area: [Vector3, Vector3], ground: GameObject | nil = nil, seed: Integer = 0, scale: Numeric | Range[Numeric] = 1.0, align: bool = false, tints: Array[Color] | nil = nil, tint_variation: Numeric = 0.0, fade: Numeric = 0.0, normals_up: Numeric = 0.0, lods: Array[[String, Numeric]] | nil = nil) -> Scatter`: places copies; returns a set (count, remove)
- `Scatter.clear -> void`: removes every set
- `count -> Integer`: how many copies a set placed
- `remove -> void`: removes a set

## Save

What the game keeps between runs. See [online-services.md#saving-on-a-server](online-services.md#saving-on-a-server).

- `Save.get(String | Symbol key, untyped default = nil) -> untyped`: a value, or the default
- `Save.set(String | Symbol key, untyped value) -> untyped`: keeps a value (nil removes it)
- `Save[String | Symbol key] -> untyped`: the same as get
- `Save[String | Symbol key] = untyped value`: the same as set
- `Save.update(String | Symbol key, untyped default = nil) { (untyped) -> untyped } -> untyped`: reads and writes a key with nothing in between
- `Save.delete(String | Symbol key) -> void`: removes a key
- `Save.clear -> void`: removes everything
- `Save.keys -> Array[Symbol]`: every key
- `Save.slot -> String`: the selected save slot name; default keeps the existing save file
- `Save.slot = String | Symbol name`: selects a named slot for this engine; default restores the original store
- `Save.slots -> Array[String]`: sorted names of persisted slots and slots opened in this run, including default

Values are numbers, texts, true/false, `Vector3`, and arrays and hashes of those; hashes come back with string keys. Named slots start on default for every engine and keep their selection across scenes. Selection is local to each engine, including shared-database server rooms. Names are 1–48 lowercase ASCII letters/digits, hyphens or underscores, starting with a letter/digit. At most 64 named stores may be opened per run/local backend and 4096 names listed; invalid names or unavailable backends leave selection unchanged and raise a Ruby error. The existing default file/table is preserved. Named files use their own atomic writes/backups; SQLite uses separate slot tables. Save.update writes to its entry store even if its block selects another slot. Slots keep values, not object-state snapshots or migration metadata.

## HTTP

Web requests; the answer comes to the block. See [online-services.md#http](online-services.md#http).

- `HTTP.get(String url, headers: Hash[String | Symbol, untyped] = {}, timeout: Numeric = 15.0, redirects: bool = true) ?{ (HTTP::Response) -> void } -> Integer`: GET
- `HTTP.post(String url, json: untyped = nil, body: String | nil = nil, headers: Hash[String | Symbol, untyped] = {}, timeout: Numeric = 15.0, redirects: bool = true) ?{ (HTTP::Response) -> void } -> Integer`: POST
- `HTTP.put(String url, json: untyped = nil, body: String | nil = nil, headers: Hash[String | Symbol, untyped] = {}, timeout: Numeric = 15.0, redirects: bool = true) ?{ (HTTP::Response) -> void } -> Integer`: PUT
- `HTTP.patch(String url, json: untyped = nil, body: String | nil = nil, headers: Hash[String | Symbol, untyped] = {}, timeout: Numeric = 15.0, redirects: bool = true) ?{ (HTTP::Response) -> void } -> Integer`: PATCH
- `HTTP.delete(String url, headers: Hash[String | Symbol, untyped] = {}, timeout: Numeric = 15.0, redirects: bool = true) ?{ (HTTP::Response) -> void } -> Integer`: DELETE
- `HTTP.request(String | Symbol verb, String url, json: untyped = nil, body: String | nil = nil, headers: Hash[String | Symbol, untyped] = {}, timeout: Numeric = 15.0, redirects: bool = true) ?{ (HTTP::Response) -> void } -> Integer`: any verb

## HTTP::Response

What a web request's block is given. See [online-services.md#http](online-services.md#http).

- `ok? -> bool`: whether it came back with a 2xx status
- `status -> Integer`: its status
- `headers -> Hash[String, String]`: its headers
- `body -> String`: its body (decompressed when it came with gzip or deflate)
- `json -> untyped`: its body read as JSON
- `error -> String | nil`: why there was no answer, or nil
- `url -> String | nil`: the address the answer came from, once redirects were followed

## I18n

The game's texts in several languages (locales/*.json). See [i18n.md](i18n.md).

- `I18n.t(String | Symbol key, **untyped values) -> String`: the text of a key in the language in use, its {placeholders} filled
- `I18n.locale -> Symbol`: the language in use (:en)
- `I18n.locale = Symbol | String language`: changes the language
- `I18n.available_locales -> Array[Symbol]`: the languages of the game's locales/ files

## Network

Games played together: on a local network, through a relay or on a dedicated server. See [multiplayer.md](multiplayer.md).

- `Network.host(port: Integer = 7777, name: String | nil = nil, visible: bool = true, room: bool = false, waiting_per_address: Integer = Network::WAITING_PER_ADDRESS, fingerprint: String | nil = nil) -> bool`: hosts a game, encrypted (fingerprint: is the relay's, with room: true)
- `Network.join(String | nil address = nil, port: Integer = 7777, room: String | nil = nil, auth: untyped = nil, fingerprint: String | nil = nil) -> bool`: joins a game (an address, or a room code), encrypted; fingerprint: is who must answer
- `Network.fingerprint -> String | nil`: the fingerprint of the game hosted here, or nil
- `Network.host_code -> String | nil`: the short code players who join the game hosted here check it with, or nil
- `Network.leave -> void`: leaves the game
- `Network.online? -> bool`: whether this machine is in a game
- `Network.connecting? -> bool`: whether a join is on its way
- `Network.host? -> bool`: whether this machine hosts
- `Network.server? -> bool`: whether this machine is a dedicated server
- `Network.id -> Integer`: this machine's peer id
- `Network.peers -> Array[Integer]`: the peers of the game
- `Network.players -> Array[Integer]`: the peers without a dedicated server
- `Network.port -> Integer`: the port of the game
- `Network.room -> String | nil`: the room code, on a relay
- `Network.relay -> String | nil`: the relay's address
- `Network.relay = String | nil address`: sets the relay's address
- `Network.search -> bool`: looks for games on the local network
- `Network.searching? -> bool`: whether it is looking
- `Network.stop_search -> void`: stops looking
- `Network.games -> Array[Hash[Symbol, untyped]]`: the games found
- `Network.discovery_port -> Integer`: the port games are looked for on
- `Network.discovery_port = Integer port`: changes it
- `Network.emit(String | Symbol name, untyped data = nil, to: Symbol | Integer = :all) -> void`: sends a message (:all, :host or a peer id)
- `Network.on(String | Symbol name) { (untyped, Integer) -> void } -> void`: receives a message: |data, from|
- `Network.on_connect { () -> void } -> void`: this machine is in the game
- `Network.on_disconnect { () -> void } -> void`: the game is gone
- `Network.on_join { (Integer) -> void } -> void`: someone arrived: |peer|
- `Network.on_leave { (Integer) -> void } -> void`: someone left: |peer|
- `Network.on_fail { (String | nil) -> void } -> void`: a join did not work out: |reason|
- `Network.authenticate { (Integer, untyped, Network::Decision) -> void } -> void`: decides who gets in: |peer, auth, decide|
- `Network.identity(Integer peer) -> untyped`: who a peer was accepted as
- `Network.player_name(Integer peer) -> String | nil`: a peer's name
- `Network.spawn(String template, owner: Integer = Network.id, position: Vector3 | nil = nil, rotation: Vector3 | nil = nil) -> GameObject | nil`: spawns a copy on every machine
- `Network.allow_spawn(*String templates) -> void`: lets guests spawn those templates

## Network::Decision

What an authenticate block decides with. See [online-services.md](online-services.md).

- `accept(identity: untyped, name: String | nil = nil) -> void`: lets the peer in
- `deny(String reason = "not let in") -> void`: keeps the peer out

## NetworkSync

A behaviour: the same object in the same place on every machine. See [multiplayer.md#objects-in-the-same-place-everywhere](multiplayer.md#objects-in-the-same-place-everywhere).

- `mine? -> bool`: whether this machine owns it
- `owner -> Integer`: the peer that owns it
- `owner = Integer peer`: gives it to a peer
- `key -> String`: the name it is known by on every machine
- `assign(owner: Integer, key: String = self.key) -> void`: sets its owner and key

## Notes

Script errors are logged with a backtrace and only disable the failing behaviour; the game keeps running. Every method takes finite numbers a float can hold: infinity, NaN or a number too large raises an `ArgumentError` instead of reaching the scene. A setting with a range raises outside it and says the range; nothing is quietly clamped, except `Vehicle#drive`'s inputs. Assigning `transform.position` to a physics object teleports it (moving bodies also stop); changing its rotation keeps its velocity. Images may be PNG, JPEG or static WebP (a name without an extension looks for `.webp` last), at most 8192 pixels a side; animated WebP and KTX2/Basis are not read. Missing or unreadable images (scene textures, `UI.image`, particle images) show an opaque magenta-and-black checkerboard, with a warning; a material that never named a texture keeps its plain color, and no path reads outside the game's folder.
