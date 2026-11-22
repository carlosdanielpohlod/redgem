---
description: "Every class and method of the scripting API, a line each (also: redgem api)."
---

# API reference

The scripting API of RedGem games: the classes and methods scripts use. Generated from docs/scripting/api.json, which tests keep in step with the engine. Each class and method is stable (from 1.0, no breaking change within a 1.x minor release) or experimental (it may change in any release).

| Class | What it is | Stability |
|---|---|---|
| [`Behaviour`](#behaviour) | A Ruby class attached to an object: the engine calls its methods as the game runs. | [Stable](stability.md) |
| [`Behaviour::Timer`](#behaviourtimer) | What after, every and tween return: something waiting to happen, which can be called off. | [Stable](stability.md) |
| [`GameObject`](#gameobject) | An object of the scene, as scripts see it. | [Stable](stability.md) |
| [`Transform`](#transform) | Where an object is, how it is turned and how big it is. | [Stable](stability.md) |
| [`Vector3`](#vector3) | A point or a direction in meters: x, y, z. | [Stable](stability.md) |
| [`Scene`](#scene) | The objects of the running scene. | [Stable](stability.md) |
| [`SceneScript`](#scenescript) | Sets a level up from Ruby when it loads: scripts/<scene>_scene.rb. | [Stable](stability.md) |
| [`Input`](#input) | Keyboard, mouse, gamepad and touch, read every frame. | [Stable](stability.md) |
| [`Time`](#time) | The game's clock (added to Ruby's Time). | [Stable](stability.md) |
| [`Physics`](#physics) | Questions to the physics world: rays, moving shapes and shapes in place. | [Stable](stability.md) |
| [`Joint`](#joint) | What holds two bodies together, or one to the world (made by Physics.joint, or by an object's Joint component in the editor: GameObject#joint). | [Experimental](stability.md) |
| [`Navigation`](#navigation) | Where the characters a game moves can walk, and the way from one place to another. | [Experimental](stability.md) |
| [`NavAgent`](#navagent) | A behaviour: a character that walks where it is sent along Navigation.path, steering clear of other agents and of moving bodies, and finding another way when something stands in its way. | [Experimental](stability.md) |
| [`Water`](#water) | Where the scene's water is: the height of its surface, and how deep a point is under it. | [Experimental](stability.md) |
| [`Environment`](#environment) | The scene's look while the game runs: sky, fog, light and effects, as its environment object describes them. | [Experimental](stability.md) |
| [`Material`](#material) | The glow of the scene's materials, by name, changed while the game runs: lit windows at night. | [Experimental](stability.md) |
| [`Terrain`](#terrain) | The ground layers of a terrain: which one is where, and its weights painted again by rules. | [Experimental](stability.md) |
| [`CharacterController`](#charactercontroller) | The body of an object with body = "character": walks, jumps, climbs steps. | [Stable](stability.md) |
| [`Rigidbody`](#rigidbody) | The body of an object with body = "dynamic" or "kinematic" (or a vehicle's chassis). | [Stable](stability.md) |
| [`Vehicle`](#vehicle) | The body of an object with body = "vehicle" and wheels. | [Stable](stability.md) |
| [`Camera`](#camera) | The camera the game is seen through, and what it does on its own: follow, orbit, shake. | [Stable](stability.md) |
| [`Camera::Orbit`](#cameraorbit) | What Camera.orbit returns: the camera's angles and distance around what it orbits, for scripts to read and change. | [Experimental](stability.md) |
| [`Light`](#light) | game_object.light: the light an object shines with. | [Stable](stability.md) |
| [`Animation`](#animation) | The scene's own animations (exported from Blender), by name. | [Stable](stability.md) |
| [`Animator`](#animator) | game_object.animation (animation in a behaviour): an object's own clips. | [Stable](stability.md) |
| [`Animation::Graph`](#animationgraph) | Named states and transitions for one character (animation.use(graph)). | [Stable](stability.md) |
| [`Animation::Event`](#animationevent) | What the block of an animation event is given (animation.on). | [Experimental](stability.md) |
| [`Audio`](#audio) | Sounds and music from the game's sounds/ folder. | [Stable](stability.md) |
| [`UI`](#ui) | What a script draws over the game this frame: texts, rectangles, pictures, buttons. | [Stable](stability.md) |
| [`UI::Focus`](#uifocus) | Which widget of a screen the keyboard and a gamepad work: up and down move it, wrapping. | [Stable](stability.md) |
| [`UI::ScrollList`](#uiscrolllist) | Rows in a box that scrolls, drawn only inside it: the wheel, Page Up/Down, a gamepad's bumpers and right stick, and a focus. | [Experimental](stability.md) |
| [`UI::Theme`](#uitheme) | How the interface and the menus look: fonts, sizes, colors, buttons, layout, background, logo, footer, transitions, sounds and music. Made of layers over the engine's defaults. | [Experimental](stability.md) |
| [`UI::ButtonState`](#uibuttonstate) | What a button is this frame: focused, under the pointer, held down, disabled; and the id of the option it is of. | [Experimental](stability.md) |
| [`Debug`](#debug) | Developer diagnostics: console messages, frame timings and immediate world drawing. | [Stable](stability.md) |
| [`Game`](#game) | The game itself. | [Stable](stability.md) |
| [`Settings`](#settings) | How the player chose the game to be shown and heard; a change takes effect at once. | [Stable](stability.md) |
| [`GameMenu`](#gamemenu) | A behaviour: title screen, pause menu, end screen, settings screen with its controls page, and the quest journal; arrows or the D-pad move the focus, Enter or A picks. | [Stable](stability.md) |
| [`GameMenu::Screen`](#gamemenuscreen) | A screen of the GameMenu. | [Stable](stability.md) |
| [`Particles`](#particles) | Short-lived sparks, smoke and dust. | [Stable](stability.md) |
| [`Particles::Emitter`](#particlesemitter) | A continuous particle effect attached to an object, returned by Particles.emitter. | [Experimental](stability.md) |
| [`Decals`](#decals) | Marks laid on the scene's surfaces: bullet holes, scorches, cracks. | [Experimental](stability.md) |
| [`Scatter`](#scatter) | Great numbers of copies of a mesh (grass, trees, rocks), drawn only. | [Stable](stability.md) |
| [`Save`](#save) | What the game keeps between runs. | [Stable](stability.md) |
| [`HTTP`](#http) | Web requests; the answer comes to the block. | [Stable](stability.md) |
| [`HTTP::Response`](#httpresponse) | What a web request's block is given. | [Stable](stability.md) |
| [`I18n`](#i18n) | The game's texts in several languages (locales/*.json). | [Stable](stability.md) |
| [`Network`](#network) | Games played together: on a local network, through a relay or on a dedicated server. | [Stable](stability.md) |
| [`Network::Decision`](#networkdecision) | What an authenticate block decides with. | [Stable](stability.md) |
| [`NetworkSync`](#networksync) | A behaviour: the same object in the same place on every machine. | [Stable](stability.md) |
| [`Interactable`](#interactable) | A behaviour: something the player can use or talk to; its behaviours hear on_interact(actor), on_focus(actor) and on_blur(actor). | [Experimental](stability.md) |
| [`Interactor`](#interactor) | A behaviour on the player's hero: picks the best Interactable in reach, shows its prompt and uses it with the :interact action or a tap. | [Experimental](stability.md) |
| [`Dialogue`](#dialogue) | Conversations written in dialogue files (dialogues/<name>.json): lines, choices, conditions and effects; each player has their own, decided on the host. | [Experimental](stability.md) |
| [`Dialogue::Conversation`](#dialogueconversation) | One player's way through a dialogue: the line it is at, the choices offered, and how it goes on. | [Experimental](stability.md) |
| [`Dialogue::Conversation::Choice`](#dialogueconversationchoice) | An answer the player may give. | [Experimental](stability.md) |
| [`Dialogue::Script`](#dialoguescript) | A dialogue file as it was read. | [Experimental](stability.md) |
| [`GameVariables`](#gamevariables) | The values every player shares, which dialogue conditions read and effects change; owned by the host over the network. | [Experimental](stability.md) |
| [`Talkable`](#talkable) | A behaviour: someone the player can talk to; using it (its Interactable, added with the prompt "Talk" when it has none) starts its dialogue. | [Experimental](stability.md) |
| [`Quests`](#quests) | The game's quests, written in quest files (quests/<name>.json): stages with objectives (go to, talk to, deliver, collect, defeat, a check); kept in GameVariables, so the party shares them and the host owns them. | [Experimental](stability.md) |
| [`Quests::Quest`](#questsquest) | One quest and where the party is in it (Quests[name] gives one). | [Experimental](stability.md) |
| [`Quests::Objective`](#questsobjective) | One thing the party does in a stage of a quest. | [Experimental](stability.md) |
| [`Quests::Definition`](#questsdefinition) | A quest file as it was read. | [Experimental](stability.md) |
| [`QuestTracker`](#questtracker) | A behaviour on the player's hero: tells the quests when it reaches a go_to objective's place, asks the game's checks on the host, and shows where the tracked quest's objectives happen (QuestMarkers). | [Experimental](stability.md) |
| [`QuestMarkers`](#questmarkers) | Where the tracked quest's objectives happen, on this machine's screen: a marker over each place the camera sees and a pointer at the edge toward one it does not; the game turns them off, restyles them, locates objectives or draws them. | [Experimental](stability.md) |
| [`QuestMarkers::Mark`](#questmarkersmark) | One place QuestMarkers shows: a marker on the screen, or a pointer at its edge. | [Experimental](stability.md) |
| [`DialogueBox`](#dialoguebox) | A behaviour: shows the player's conversation at the bottom of the screen, with its choices; keyboard, gamepad and touch go on, choose and leave. | [Experimental](stability.md) |
| [`Interaction`](#interaction) | Who uses what: the choice of a target, uses, and the game's handlers; over the network the host decides. | [Experimental](stability.md) |
| [`LineOfSight`](#lineofsight) | Whether one thing can see another: a ray to it hits nothing else first. | [Experimental](stability.md) |
| [`Perception`](#perception) | A behaviour: what an NPC sees (a cone checked by a ray) and hears (a radius); its behaviours hear on_see, on_lose_sight and on_hear. | [Experimental](stability.md) |
| [`Perception::Noise`](#perceptionnoise) | A noise someone heard (on_hear). | [Experimental](stability.md) |
| [`Brain`](#brain) | A behaviour: an NPC's state machine, with states, transitions, guards, timers and events. | [Experimental](stability.md) |
| [`GameClock`](#gameclock) | The time of day in the game's world, on game time: what schedules follow. | [Experimental](stability.md) |
| [`Schedule`](#schedule) | A behaviour: an NPC's day, what it does from what time by the GameClock; its behaviours hear on_schedule(activity). | [Experimental](stability.md) |
| [`HeadLook`](#headlook) | A behaviour: turns an NPC's head toward the nearest hero in range and in front, or toward its target. | [Experimental](stability.md) |
| [`VoiceMouth`](#voicemouth) | A behaviour: moves a speaker's mouth with the loudness of its voice, through a shape key (morph:) or a jaw bone (jaw:, angle:). | [Experimental](stability.md) |
| [`Dialogue::Voice`](#dialoguevoice) | The clip of a dialogue line's voice, playing from the speaker's head in the :voice group (made by the conversation shown). | [Experimental](stability.md) |

## Behaviour

A Ruby class attached to an object: the engine calls its methods as the game runs. See [behaviours.md](behaviours.md).

- `start -> void`: called once, before the first update
- `update(Float dt) -> void`: called every frame; dt is the seconds since the last one
- `fixed_update(Float dt) -> void`: called at the game's tick (game.json's tick_hz, 60 times a second by default), with the physics
- `on_trigger_enter(GameObject other) -> void`: something entered this object's trigger, or it entered one
- `on_trigger_exit(GameObject other) -> void`: something left this object's trigger
- `on_collision_enter(GameObject other) -> void`: this object's body touched another
- `on_collision_exit(GameObject other) -> void`: this object's body stopped touching another
- `on_destroy -> void`: the object is being destroyed
- `on_interact(GameObject actor) -> void`: an actor used this object (it has an Interactable; on the host in a game over the network) *Experimental.*
- `on_focus(GameObject actor) -> void`: this object became what an actor's interaction prompt is for *Experimental.*
- `on_blur(GameObject actor) -> void`: it stopped being what that prompt is for *Experimental.*
- `on_see(GameObject object) -> void`: a Perception on the object saw something come into sight *Experimental.*
- `on_lose_sight(GameObject object) -> void`: ... and saw it go out of sight *Experimental.*
- `on_hear(Perception::Noise noise) -> void`: a Perception on the object heard a noise *Experimental.*
- `on_schedule(Symbol activity) -> void`: a Schedule on the object came to the hour of another activity *Experimental.*
- `Behaviour.persist(*Symbol | String fields) -> Array[Symbol]`: fields Save.checkpoint keeps for a spawned copy, given back before it starts *Experimental.*
- `Behaviour.persisted_fields -> Array[Symbol]`: what persist named, in this class and those above it *Experimental.*
- `game_object -> GameObject`: the object this behaviour is attached to
- `name -> String`: the object's name
- `transform -> Transform`: the object's Transform
- `character -> CharacterController | nil`: the object's CharacterController, or nil
- `rigidbody -> Rigidbody | nil`: the object's Rigidbody, or nil
- `vehicle -> Vehicle | nil`: the object's Vehicle, or nil
- `animation -> Animator`: the object's own Animator
- `props -> Hash[Symbol, untyped]`: the object's custom properties, as a Hash with Symbol keys
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
- `stable_id -> String | nil`: its persistent scene-file identity as a String; nil for runtime copies *Experimental.*
- `capture_state -> Hash[Symbol, untyped]`: a versioned checkpoint of local pose, tint and visibility, suitable for Save *Experimental.*
- `restore_state(Hash[Symbol, untyped] state) -> self`: validates a checkpoint then restores pose and appearance; teleports physical descendants and returns self *Experimental.*
- `tag -> String | nil`: its tag property
- `tag?(String | Symbol value) -> bool`: whether its tag is value
- `props -> Hash[Symbol, untyped]`: its custom properties, as a Hash with Symbol keys
- `joint -> Joint | nil`: the Joint it came with in the scene (its Joint component, set up in the editor), or nil *Experimental.*
- `alive? -> bool`: false once it was destroyed
- `destroy -> void`: removes it at the end of the step
- `visible -> bool`: whether it is drawn
- `visible = bool value`: shows or hides it
- `view_model? -> bool`: whether it is drawn as the view model (its view_model property), with what is under it *Experimental.*
- `view_model = bool value`: makes it (and what is under it) the view model: a first-person weapon drawn over the world (never into a wall), with Camera.view_model_fov, its own light, no shadow on the world *Experimental.*
- `behaviours -> Array[Behaviour]`: its behaviours
- `parent -> GameObject | nil`: the object it is under, or nil
- `parent = GameObject | nil object`: puts it under another object (nil: at the root), where it stays in the world, then moves with it; never under itself or what it carries *Experimental.*
- `set_parent(GameObject | nil object, keep_world: bool = true) -> GameObject | nil`: like parent=; keep_world: false keeps its local transform instead, so the new parent places it; returns the parent *Experimental.*
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
- `morph(String | Symbol name, Numeric | nil weight = nil) -> Numeric | nil`: the weight of a shape key (morph target) of its mesh and the meshes under it, or sets it: morph(:Smile, 0.7); nil when no part has the key, ArgumentError when setting one none has *Experimental.*
- `morph_names -> Array[Symbol]`: the names of the shape keys of its mesh and the meshes under it, as symbols *Experimental.*

## Transform

Where an object is, how it is turned and how big it is.

- `position -> Vector3`: its position in the world (Vector3)
- `position = Vector3 vector`: moves it; a physics object is teleported
- `local_position -> Vector3`: its position relative to its parent
- `local_position = Vector3 vector`: moves it relative to its parent
- `rotation -> Vector3`: its rotation, Euler degrees (Vector3)
- `rotation = Vector3 degrees`: turns it to those Euler degrees
- `local_rotation -> Vector3`: its rotation relative to its parent, Euler degrees (Vector3) *Experimental.*
- `local_rotation = Vector3 degrees`: turns it to those Euler degrees relative to its parent *Experimental.*
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
- `Scene.find_by_id(String | Symbol identity) -> GameObject | nil`: the current scene object with that persistent String/Symbol identity, or nil *Experimental.*
- `Scene.find!(String | Symbol name, wanted_by: Class | nil = nil) -> GameObject`: find that stops with a clear error when the object is missing
- `Scene.find_all(String | Symbol tag) -> Array[GameObject]`: every object with that tag
- `Scene.objects -> Array[GameObject]`: every object
- `Scene.spawn(String | GameObject template, position: Vector3 | nil = nil, rotation: Vector3 | nil = nil, physics: bool = true, name: String | nil = nil, parent: GameObject | nil = nil) -> GameObject | nil`: copies a prefab = true template, with its physics and scripts, named name: (the template's by default) and under parent:, keeping its place in the world; returns the copy
- `Scene.spawn_prefab(String file, position: Vector3 | nil = nil, rotation: Vector3 | nil = nil, physics: bool = true, name: String | nil = nil, parent: GameObject | nil = nil) -> GameObject | nil`: instantiates a rigid GLB hierarchy from prefabs/ at the given world placement; returns its root *Experimental.*

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
- `Input.rumble? -> bool`: whether gamepads can rumble here: never yet (the window library has no vibration on desktops nor Android) *Experimental.*
- `Input.rumble(Integer pad, Numeric strength, Numeric seconds) -> bool`: rumbles a player's gamepad (strength 0 to 1); a hook that does nothing yet and returns false (see rumble?) *Experimental.*
- `Input.text -> String`: what was typed this frame
- `Input.touch_controls = bool on`: shows the on-screen stick and buttons of a phone
- `Input.touch_controls? -> bool`: whether they are shown

Every action, axis, binding and gamepad call takes `pad:` (0 to 3: the player; 0 when left out). Actions and axes come from `game.json`'s `input` section and `define_action`/`define_axis`; a player's bindings are kept between runs, beside the game's save. See [Controls](gameplay.md#controls).

## Time

The game's clock (added to Ruby's Time).

- `Time.delta -> Float`: seconds since the last frame
- `Time.unscaled_delta -> Float`: seconds since the last frame, whatever Time.scale (also while paused) *Experimental.*
- `Time.fixed_delta -> Float`: seconds of a physics step: 1 / game.json's tick_hz (1/60 by default)
- `Time.elapsed -> Float`: seconds since the game started
- `Time.frame -> Integer`: how many frames have run
- `Time.scale -> Float`: how fast game time runs (1 is normal)
- `Time.scale = Numeric value`: slows or speeds the game; 0 pauses it

## Physics

Questions to the physics world: rays, moving shapes and shapes in place. See [gameplay.md](gameplay.md).

- `Physics.raycast(Vector3 origin, Vector3 direction, Numeric max_distance = 1000.0, GameObject | nil skip = nil, layers: Array[Symbol] | Symbol | nil = nil, ignore: Array[GameObject] | GameObject | nil = nil) -> Hash[Symbol, untyped] | nil`: the first thing a ray hits: {object:, point:, normal:, distance:}, or nil (skip: an object it sees through)
- `Physics.raycast_all(Vector3 origin, Vector3 direction, Numeric max_distance = 1000.0, layers: Array[Symbol] | Symbol | nil = nil, ignore: Array[GameObject] | GameObject | nil = nil) -> Array[Hash[Symbol, untyped]]`: everything a ray hits, nearest first *Experimental.*
- `Physics.sphere_cast(Vector3 from, Vector3 to, Numeric radius, layers: Array[Symbol] | Symbol | nil = nil, ignore: Array[GameObject] | GameObject | nil = nil) -> Hash[Symbol, untyped] | nil`: the first thing a ball moving from `from` to `to` touches, or nil *Experimental.*
- `Physics.box_cast(Vector3 from, Vector3 to, Vector3 size, rotation: Vector3 | nil = nil, layers: Array[Symbol] | Symbol | nil = nil, ignore: Array[GameObject] | GameObject | nil = nil) -> Hash[Symbol, untyped] | nil`: the same for a box (its whole size; rotation in degrees) *Experimental.*
- `Physics.capsule_cast(Vector3 from, Vector3 to, Numeric radius, Numeric height, rotation: Vector3 | nil = nil, layers: Array[Symbol] | Symbol | nil = nil, ignore: Array[GameObject] | GameObject | nil = nil) -> Hash[Symbol, untyped] | nil`: the same for an upright capsule (height end to end) *Experimental.*
- `Physics.overlap_sphere(Vector3 center, Numeric radius, layers: Array[Symbol] | Symbol | nil = nil, ignore: Array[GameObject] | GameObject | nil = nil) -> Array[Hash[Symbol, untyped]]`: the hits of everything inside a ball, nearest first *Experimental.*
- `Physics.overlap_box(Vector3 center, Vector3 size, rotation: Vector3 | nil = nil, layers: Array[Symbol] | Symbol | nil = nil, ignore: Array[GameObject] | GameObject | nil = nil) -> Array[Hash[Symbol, untyped]]`: the same inside a box *Experimental.*
- `Physics.overlap_capsule(Vector3 center, Numeric radius, Numeric height, rotation: Vector3 | nil = nil, layers: Array[Symbol] | Symbol | nil = nil, ignore: Array[GameObject] | GameObject | nil = nil) -> Array[Hash[Symbol, untyped]]`: the same inside a capsule *Experimental.*
- `Physics.layers -> Array[Symbol]`: the game's collision layers, as Symbols (game.json's physics layers) *Experimental.*
- `Physics.joint(Symbol kind, GameObject object, GameObject | nil other = nil, anchor: Vector3 | nil = nil, other_anchor: Vector3 | nil = nil, axis: Vector3 | nil = nil, limits: Array[Numeric] | nil = nil, motor_speed: Numeric | nil = nil, motor_torque: Numeric | nil = nil, min: Numeric | nil = nil, max: Numeric | nil = nil, spring: Numeric | nil = nil, damping: Numeric | nil = nil, break_force: Numeric | nil = nil) -> Joint`: joins two objects, or one to the world (other nil): :fixed, :hinge, :slider, :point or :distance *Experimental.*

`layers:` is a layer's name or a list of them (every layer when nil); `ignore:` is an object or a list of objects to see through. Hits are `{object:, point:, normal:, distance:}`, each object once, nearest first, at most 64; triggers are never hit. A cast that starts inside something hits it at distance 0.

## Joint

What holds two bodies together, or one to the world (made by Physics.joint, or by an object's Joint component in the editor: GameObject#joint). *Experimental.* See [gameplay.md](gameplay.md).

- `kind -> Symbol`: :fixed, :hinge, :slider, :point or :distance *Experimental.*
- `object -> GameObject`: the object it holds *Experimental.*
- `other -> GameObject | nil`: the other object it holds, or nil for the world *Experimental.*
- `alive? -> bool`: whether it still holds them *Experimental.*
- `broken? -> bool`: whether it was pulled past its break force *Experimental.*
- `remove -> void`: lets go *Experimental.*
- `enabled? -> bool`: whether it holds right now *Experimental.*
- `enabled = bool flag`: lets go for a while, or holds again *Experimental.*
- `break_force -> Float`: the pull (newtons) or twist (newton meters) it breaks at; 0 never breaks *Experimental.*
- `break_force = Numeric newtons`: sets the pull it breaks at *Experimental.*
- `motor(Numeric speed, Numeric torque) -> void`: a hinge turns at that speed (degrees per second) with at most that torque; 0 torque lets it go *Experimental.*
- `angle -> Float`: how far a hinge turned from where it started, in degrees *Experimental.*

Points and axes are in world space; `anchor:` defaults to the object's position, `other_anchor:` (a distance joint's other end) to the other object's, `axis:` to up. A hinge's `limits:` are degrees [low, high] from where it starts (low -180 to 0, high 0 to 180), a slider's are meters along its axis; a hinge's `motor_speed:` needs a `motor_torque:`. A distance joint keeps between `min:` (0) and `max:` (how far apart the ends are) apart, springy with `spring:` (hertz) and `damping:`. Each kind refuses options it has no use for, and `motor` and `angle` raise for anything but a hinge. A joint needs a dynamic body at one end (one whose bodies were all made kinematic since holds nothing until one is dynamic again); it goes when either object is destroyed or the scene changes.

## Navigation

Where the characters a game moves can walk, and the way from one place to another. *Experimental.* See [gameplay.md#chase-the-player-navigation](gameplay.md#chase-the-player-navigation).

- `Navigation.path(Vector3 from, Vector3 to, avoid: Array[GameObject] | GameObject = []) -> Array[Vector3] | nil`: the points to walk through from near `from` to near `to` (Vector3s, both ends included), or nil when there is no way there; `avoid:` game objects whose bodies it keeps a character's width off *Experimental.*
- `Navigation.steer(Vector3 position, Vector3 velocity, Vector3 preferred, speed: Numeric, radius: Numeric, crowd: Array[[Vector3, Vector3, Numeric]] = [], ignore: Array[GameObject] | GameObject = []) -> Vector3`: the flat velocity nearest `preferred` that keeps clear of `crowd` ([position, velocity, radius] each, characters that steer too) and of the bodies that move near `position` (but `ignore`), and on walkable ground *Experimental.*
- `Navigation.nearest(Vector3 point) -> Vector3 | nil`: the walkable point nearest a point (within 8 m of it), or nil *Experimental.*
- `Navigation.walkable?(Vector3 point) -> bool`: whether a point is on walkable ground, up to a character's height above it *Experimental.*
- `Navigation.draw(Vector3 around, Numeric radius = 10, color: Color = :blue, alpha: Integer = 255) -> Integer`: shows the walkable cells within a radius for this frame, as Debug.box does; returns how many *Experimental.*
- `Navigation.bake -> Hash[Symbol, Numeric | bool]`: bakes the scene's grid again now (a level a script changed); returns what stats does *Experimental.*
- `Navigation.stats -> Hash[Symbol, Numeric | bool]`: how the grid was baked: {columns:, cells:, cell_size:, cells_enlarged:, milliseconds:} *Experimental.*

The grid is baked from the scene's static bodies as it loads when game.json has a `navigation` section (`cell_size` 0.25, `agent_radius` 0.4, `agent_height` 1.8, `max_step` 0.4, `max_slope` 45 degrees, `bake_on_load` true), and otherwise when a script first asks. Moving bodies, characters and triggers are not in the grid: a path goes around those of `avoid:`, and `steer` (or a NavAgent) around the rest, looking 3 seconds ahead. A level changed after the bake is not seen until `Navigation.bake`. A path goes through cells side by side no more than `max_step` apart, never cutting a corner, and is pulled straight. A search looks at no more than 16384 cells: a way longer than that gives nil, as one to a place nobody can reach does. A grid has at most 262144 columns: a larger level is baked with larger cells, with a warning.

## NavAgent

A behaviour: a character that walks where it is sent along Navigation.path, steering clear of other agents and of moving bodies, and finding another way when something stands in its way. *Experimental.* See [gameplay.md#crowds-agents-that-walk-among-others](gameplay.md#crowds-agents-that-walk-among-others).

- `go_to(Vector3 point) -> bool`: sends it to a point; false when there is no way there *Experimental.*
- `stop -> void`: stops it where it is *Experimental.*
- `arrived? -> bool`: whether it got where it was sent *Experimental.*
- `walking? -> bool`: whether it is on its way somewhere *Experimental.*
- `destination -> Vector3 | nil`: where it was sent last, or nil *Experimental.*
- `waypoints -> Array[Vector3]`: the points it still walks through, the next first *Experimental.*
- `velocity -> Vector3`: the velocity it walks at, as it steered this frame *Experimental.*
- `speed -> Float`: meters per second at most (setting `speed`, 3) *Experimental.*
- `radius -> Float`: how wide it is kept from others, in meters (setting `radius`, 0.4) *Experimental.*
- `arrive_distance -> Float`: how near the goal is there, in meters (setting `arrive_distance`, 0.3) *Experimental.*
- `NavAgent.all -> Array[NavAgent]`: the agents of the running scene *Experimental.*

Its object walks by its character controller (`character.move`), or by its transform when it has none. When it gets less than a quarter of the way it would walk in a second, it asks for another way around what moves within 2 m of it (other agents too).

## Water

Where the scene's water is: the height of its surface, and how deep a point is under it. *Experimental.* See [gameplay.md#float-in-water](gameplay.md#float-in-water).

- `Water.height_at(Numeric x, Numeric z) -> Float | nil`: the height of the water surface over that point of the ground, or nil where there is no water *Experimental.*
- `Water.depth_at(Vector3 point) -> Float | nil`: how far below the water surface a point is in meters (negative above it), or nil where there is no water *Experimental.*
- `Water.under?(Vector3 point) -> bool`: whether a point is below the water surface *Experimental.*

Every surface whose material has `water = true` fills the column under it down to whatever ground the level has, or to the next surface below. `height_at` gives the highest surface over a column; `depth_at` and `under?` look at the nearest surface above the point (or, with none above, the nearest below), so a point under a pool on a roof is in the lake beneath it. Heights are the calm surface: waves are not counted. The answers follow the water as the last physics step found it, and are nil (or false) outside a running game.

## Environment

The scene's look while the game runs: sky, fog, light and effects, as its environment object describes them. *Experimental.* See [../levels.md#changing-the-look-while-the-game-runs](../levels.md#changing-the-look-while-the-game-runs).

- `Environment[Symbol | String key] -> Float | String | bool | nil`: what the environment object says of a setting (a color as "#rrggbb"), or nil when it says nothing and the default applies *Experimental.*
- `Environment[Symbol | String key] = Numeric | String | Symbol | Array[Numeric] | bool | nil value`: changes a setting from the next frame drawn: a color takes a name, [r, g, b] from 0 to 255 or "#rrggbb"; nil gives the setting back to its default *Experimental.*
- `Environment.set(**untyped settings) -> nil`: changes several settings at once, as []= does each: all of them, or none when one is refused *Experimental.*
- `Environment.present? -> bool`: whether the scene has an environment object (environment = true) for scripts to change *Experimental.*

The settings are the environment object's properties (see Levels): `sky_image`, `sky_intensity`, `ambient`, `fog_color`, `fog_density`, `exposure`, `bloom`, `auto_exposure` and the rest. A new `sky_image` loads that picture, which takes a moment; a number or a color costs nothing. An unknown setting, or a value of the wrong kind, raises an ArgumentError (a String where a number goes: a TypeError). A scene without an environment object reads nil and refuses changes with a RuntimeError. Each machine keeps its own look: over the network, change it on every machine. A scene loaded anew starts from its own file.

## Material

The glow of the scene's materials, by name, changed while the game runs: lit windows at night. *Experimental.* See [../levels.md#glowing-materials-from-scripts](../levels.md#glowing-materials-from-scripts).

- `Material.set_emission(Symbol | String name, Symbol | Array[Numeric] color, strength: Numeric = 1.0) -> nil`: every material of that name glows with that color (a name, or [r, g, b] from 0 to 255) times strength (0 to 1000), over what the scene's file says *Experimental.*
- `Material.reset_emission(Symbol | String name) -> nil`: the materials of that name glow as the scene's file says again *Experimental.*
- `Material.emission(Symbol | String name) -> Hash[Symbol, untyped] | nil`: what set_emission gave the materials of that name, {color:, strength:}, or nil *Experimental.*

A material's name is the one it has in the scene's file (Blender's). The glow holds for every object with a material of that name, copies and prefabs spawned later and the scenes loaded after, until reset_emission. A negative strength raises an ArgumentError, an unknown color name too. Each machine keeps its own: over the network, set it on every machine. A game with no window (a dedicated server) keeps it and draws nothing.

## Terrain

The ground layers of a terrain: which one is where, and its weights painted again by rules. *Experimental.* See [../levels.md#painting-ground-layers-by-rules-experimental](../levels.md#painting-ground-layers-by-rules-experimental).

- `Terrain.layers(GameObject object) -> Array[String] | nil`: the names of the object's ground layers, in order, or nil when it has none *Experimental.*
- `Terrain.weights_at(GameObject object, Vector3 | Array[Numeric] position) -> Hash[String, Float] | nil`: each layer's share (0 to 1) of the ground under a point of the world, seen from above; nil without layers or past the terrain's edges *Experimental.*
- `Terrain.layer_at(GameObject object, Vector3 | Array[Numeric] position) -> String | nil`: the layer with the largest share under a point of the world; nil without layers or past the terrain's edges *Experimental.*
- `Terrain.paint_by_rules(GameObject object, Array[Hash[Symbol | String, untyped]] | nil rules = nil) -> Integer`: paints the weights by rules (as the layers file's "rules"; nil: the file's own) and returns how many texels changed; the next frame shows them *Experimental.*

A terrain has ground layers when its object names a layers file (terrain_layers = "island": terrains/island.json). Points are in the world; heights in rules are in the terrain mesh's own space. What paint_by_rules paints is kept while the scene runs and never written to the game's files: redgem scene paint writes them. A rule naming no layer of the file raises an ArgumentError, an object without layers too. Each machine keeps its own: over the network, paint on every machine.

## CharacterController

The body of an object with body = "character": walks, jumps, climbs steps. See [gameplay.md](gameplay.md).

- `move(Vector3 velocity) -> void`: walks with that velocity (m/s) this frame
- `jump(Numeric speed) -> void`: jumps up at that speed
- `grounded? -> bool`: whether it stands on something
- `resize(height: Numeric, radius: Numeric) -> bool`: makes it an upright capsule this tall (end to end) and wide, feet kept in place (to crouch); false and no change when that would grow into something (standing up under a ceiling) or it already has that size. Call it in fixed_update *Experimental.*
- `velocity -> Vector3`: its velocity
- `layer -> Symbol`: its collision layer, a Symbol *Experimental.*
- `layer = Symbol name`: moves it to another collision layer *Experimental.*

## Rigidbody

The body of an object with body = "dynamic" or "kinematic" (or a vehicle's chassis). See [gameplay.md](gameplay.md).

- `velocity -> Vector3`: its velocity
- `velocity = Vector3 vector`: sets its velocity
- `add_impulse(Vector3 vector) -> void`: pushes it
- `add_force(Vector3 force, at: Vector3 | nil = nil) -> void`: pushes it during the next step (newtons); at a point (world space), it also turns it *Experimental.*
- `add_torque(Vector3 torque) -> void`: turns it during the next step (newton meters) *Experimental.*
- `angular_velocity -> Vector3`: how fast it spins, degrees per second around each axis *Experimental.*
- `angular_velocity = Vector3 vector`: sets how fast it spins *Experimental.*
- `mass -> Float`: its mass in kilograms *Experimental.*
- `mass = Numeric kilograms`: changes its mass (more than 0) *Experimental.*
- `kinematic? -> bool`: whether scripts move it (instead of forces) *Experimental.*
- `kinematic = bool flag`: turns a dynamic body kinematic, or back *Experimental.*
- `dynamic? -> bool`: whether forces move it *Experimental.*
- `dynamic = bool flag`: turns a kinematic body dynamic, or back *Experimental.*
- `gravity_scale -> Float`: how much gravity pulls it (1 is normal) *Experimental.*
- `gravity_scale = Numeric scale`: 0 floats, 2 falls twice as hard *Experimental.*
- `sleep -> void`: stops simulating it until something touches it *Experimental.*
- `wake -> void`: simulates it again *Experimental.*
- `sleeping? -> bool`: whether it sleeps *Experimental.*
- `layer -> Symbol`: its collision layer, a Symbol *Experimental.*
- `layer = Symbol name`: moves it to another collision layer *Experimental.*

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
- `Camera.main = GameObject | nil camera`: sees the game through another camera object at once (a cut in progress stops); nil goes back to the scene's first; a scene loaded anew starts at its first *Experimental.*
- `Camera.cut_to(GameObject camera, fade: Numeric = 0) -> GameObject`: cuts to another camera; with a fade (seconds), through black: darker over its first half, cut at the darkest frame, back over the second half, also while paused; returns the camera *Experimental.*
- `Camera.cutting? -> bool`: whether a cut with a fade is still playing *Experimental.*
- `Camera.screen_ray(Numeric x, Numeric y) -> [Vector3, Vector3] | nil`: [origin, direction] through a point of the screen, in the interface's units (UI.mouse_x, UI.mouse_y)
- `Camera.world_to_screen(Vector3 | GameObject point) -> [Float, Float] | nil`: [x, y]: where a Vector3 (or an object) is on the screen, in the units UI draws in; nil behind the camera *Experimental.*
- `Camera.fov -> Float | nil`: its vertical field of view, in degrees
- `Camera.fov = Numeric degrees`: sets the field of view
- `Camera.view_model_fov -> Float | nil`: the view model's own vertical field of view in degrees (50 unless set), apart from the world's; nil without a camera *Experimental.*
- `Camera.view_model_fov = Numeric degrees`: sets the view model's field of view (10 to 150 degrees); the world's (fov) does not change it *Experimental.*
- `Camera.from_view_model(Vector3 point) -> Vector3`: where a point of the view model (a muzzle socket under the weapon) shows in the world, through the world's field of view: where a muzzle flash or a tracer drawn in the world starts *Experimental.*
- `Camera.motion_blur -> Float`: how much the picture streaks with speed, 0 to 1
- `Camera.motion_blur = Numeric amount`: sets it
- `Camera.auto_exposure -> bool | nil`: whether the picture adapts to how bright the scene is, as eyes do: true or false when the game said, nil when the scene's environment decides *Experimental.*
- `Camera.auto_exposure = bool | nil on`: turns auto exposure on (true) or off (false) for the game's camera; nil gives it back to the scene *Experimental.*
- `Camera.exposure_compensation -> Float | nil`: EV the adapted picture is brightened by (negative: darkened), or nil when the scene decides *Experimental.*
- `Camera.exposure_compensation = Numeric | nil ev`: sets it, -16 to 16; nil gives it back to the scene *Experimental.*
- `Camera.exposure_range -> [Float, Float] | nil`: [darkest, brightest]: the scene brightness auto exposure adapts within, in EV (0 is a scene whose average is middle grey), or nil when the scene decides *Experimental.*
- `Camera.exposure_range = [Numeric, Numeric] | nil range`: sets it, each end -16 to 16, the brightest not below the darkest; nil gives it back to the scene *Experimental.*
- `Camera.exposure_speed -> [Float, Float] | nil`: [up, down]: how fast auto exposure adapts, per second, when the scene got brighter and darker, or nil when the scene decides *Experimental.*
- `Camera.exposure_speed = [Numeric, Numeric] | nil speeds`: sets it, each 0 to 100 (0 never adapts); nil gives it back to the scene *Experimental.*
- `Camera.follow(GameObject target, offset: Vector3 | Array[Numeric] = [0, 3, 6], relative: bool = false, smoothing: Numeric = 0.15, look_at: bool = true, look_height: Numeric = 1.0, avoid_walls: bool = true, dead_zone: Numeric = 0.0, snap: bool = true) -> void`: follows an object from now on: offset in meters (relative: in its own axes), smoothing in seconds to close half the way, in front of walls between; snap: false flies there *Experimental.*
- `Camera.orbit(GameObject | Vector3 target, distance: Numeric = 6.0, yaw: Numeric = 0.0, pitch: Numeric = 20.0, distances: Range[Numeric] = 1.0..30.0, pitches: Range[Numeric] = -80.0..80.0, smoothing: Numeric = 0.08, input: bool = true, height: Numeric = 1.0, avoid_walls: bool = true) -> Camera::Orbit`: orbits an object or a point, looking at it, turned by dragging with the right mouse button or the right stick and zoomed by the wheel, also while paused; returns a Camera::Orbit *Experimental.*
- `Camera.stop_following -> void`: stops following or orbiting; the camera stays where it is *Experimental.*
- `Camera.following? -> bool`: whether it follows or orbits something (an object destroyed is let go) *Experimental.*
- `Camera.shake(Numeric strength, duration: Numeric = 0.5) -> Float`: shakes what it shows (never the camera object): strength (0 to 1) adds to its trauma, held at 1, fading over duration seconds *Experimental.*
- `Camera.trauma -> Float`: how much it shakes now, 0 to 1 *Experimental.*
- `Camera.stop_shaking -> void`: stops the shake at once *Experimental.*
- `Camera.backdrop_blur -> Float`: how much the whole picture is blurred evenly, 0 to 1 (a menu over the scene) *Experimental.*
- `Camera.backdrop_blur = Numeric amount`: sets it *Experimental.*

## Camera::Orbit

What Camera.orbit returns: the camera's angles and distance around what it orbits, for scripts to read and change. *Experimental.* See [gameplay.md#follow-orbit-and-shake-the-camera](gameplay.md#follow-orbit-and-shake-the-camera).

- `yaw -> Float`: degrees around it, -180 to 180: 0 behind it (its +Z side), 90 on its right *Experimental.*
- `pitch -> Float`: degrees above it *Experimental.*
- `distance -> Float`: meters from it *Experimental.*
- `target -> GameObject | Vector3`: the object or point it orbits *Experimental.*
- `turn(Numeric yaw, Numeric pitch) -> void`: turns by degrees (the yaw wraps round, eased the short way), the pitch kept within its range *Experimental.*
- `zoom(Numeric meters) -> void`: comes nearer (negative) or goes further, within its range *Experimental.*

## Light

game_object.light: the light an object shines with. See [gameplay.md#light-that-flickers-and-fades](gameplay.md#light-that-flickers-and-fades).

- `intensity -> Float`: watts for a point or spot light, lux for the sun
- `intensity = Numeric value`: sets it; shows from the next frame
- `color -> [Integer, Integer, Integer]`: its color [r, g, b], 0 to 255
- `color = Color color`: :orange or [r, g, b] from 0 to 255
- `cone_angles -> [Float, Float] | nil`: [inner, outer] in degrees for a spot light, or nil for other lights *Experimental.*
- `set_cone(inner: Numeric, outer: Numeric) -> nil`: sets both spot angles between 0 and 90 degrees, at least 0.1 degrees apart *Experimental.*

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

- `play(String | Symbol name, loop: bool = true, speed: Numeric = 1.0, fade: Numeric = 0.0, restart: bool = false, root_motion: bool | nil = nil) -> void`: plays a clip, fading from the last one; root_motion: true lets the clip move the object
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
- `on(String | Symbol clip, at: Numeric | nil = nil, seconds: Numeric | nil = nil) { (Animation::Event) -> void } -> Integer`: calls the block with an Animation::Event each time the clip passes a moment (at: 0 to 1, or seconds:); returns what off takes *Experimental.*
- `on_marker(String | Symbol name) { (Animation::Event) -> void } -> Integer`: calls the block each time a clip passes a marker of that name authored in Blender *Experimental.*
- `off(Integer listener) -> nil`: stops calling a block that on or on_marker returned *Experimental.*
- `root_motion = bool on`: whether all its clips move the object by their root's horizontal motion and turning *Experimental.*
- `root_motion? -> bool`: whether all its clips move the object *Experimental.*
- `look_at(Vector3 | GameObject target, bone: String | Symbol | GameObject = "Head", max_angle: Numeric = 60.0) -> void`: turns a bone (the head unless said) toward a point or an object's eyes, at most max_angle degrees off its clips' pose, easing in, until look_away *Experimental.*
- `look_away(bone: String | Symbol | GameObject = "Head") -> void`: the bone goes back to its clips' pose, easing out *Experimental.*
- `looking?(bone: String | Symbol | GameObject = "Head") -> bool`: whether a bone is turned toward something, or on its way back *Experimental.*
- `bone(String | Symbol name) -> GameObject | nil`: the object under the character with that name (or that name after a rig's prefix:), or nil *Experimental.*
- `bone!(String | Symbol name) -> GameObject`: bone that raises when the character has none of that name *Experimental.*
- `Animator.bone_named?(String part, String name) -> bool`: whether a part's name is that bone's: the name, or the name after a prefix: *Experimental.*

## Animation::Graph

Named states and transitions for one character (animation.use(graph)). See [animation.md](animation.md).

- `Animation::Graph.new(initial: Symbol, parameters: Hash[Symbol, untyped] = {}) -> Animation::Graph`: a graph starting at a state, with its parameters and their defaults
- `clip(Symbol name, String | Symbol clip, loop: bool = true, speed: Numeric = 1.0) -> self`: a state that plays one clip
- `blend_1d(Symbol name, parameter: Symbol, clips: Array[[Numeric, String | Symbol]], loop: bool = true, speed: Numeric = 1.0) -> self`: a state that blends clips by a number: clips: [[0.0, :Idle], [5.0, :Run]]
- `blend_2d(Symbol name, parameters: [Symbol, Symbol], clips: Array[[[Numeric, Numeric], String | Symbol]], loop: bool = true, speed: Numeric = 1.0) -> self`: a rectangular two-dimensional grid: parameters: [:x, :y], clips: [[[x, y], :Clip], ...] *Experimental.*
- `transition(Symbol from, Symbol to, parameter: Symbol, equals: untyped = nil, above: Numeric | nil = nil, below: Numeric | nil = nil, fade: Numeric = 0.0) -> self`: goes from one state to another when a parameter says so

## Animation::Event

What the block of an animation event is given (animation.on). *Experimental.* See [animation.md#events](animation.md#events).

- `game_object -> GameObject`: the object that listened *Experimental.*
- `clip -> String`: the clip's name *Experimental.*
- `name -> String | nil`: the marker's name, or nil for a moment the script named *Experimental.*
- `at -> Float`: where in the clip, from 0 (its start) to 1 (its end) *Experimental.*

## Audio

Sounds and music from the game's sounds/ folder.

- `Audio.play(String | Symbol name, volume: Numeric = 1.0, pitch: Numeric = 1.0, loop: bool = false, group: Symbol = :effects, priority: Integer = 128) -> Integer | nil`: plays a sound; returns its id (for stop, playing? and set), or nil
- `Audio.music(String | Symbol name, loop: bool = true, volume: Numeric = 0.7, fade: Numeric = 0.0) -> void`: plays music, crossfading over fade seconds from the track playing
- `Audio.stop_music(fade: Numeric = 0.0) -> void`: stops the music, fading it out over fade seconds
- `Audio.music_pitch = Numeric pitch`: changes the music's pitch
- `Audio.music_volume = Numeric volume`: changes the music's volume
- `Audio.volume = Numeric volume`: the volume of everything
- `Audio.doppler = Numeric scale`: how strong the Doppler effect is: 0 off, 1 real (the default), up to 10 *Experimental.*
- `Audio.ducking(volume: Numeric = 0.4, attack: Numeric = 0.15, release: Numeric = 0.6) -> bool`: how far and how fast the music goes down while a :voice sound plays; volume: 1 turns it off *Experimental.*
- `Audio.occlusion(volume: Numeric = 0.4, low_pass: Numeric | nil = 1000.0, fade: Numeric = 0.2) -> bool`: how a play_at sound behind one of the level's static bodies is heard; volume: 1, low_pass: nil turns it off *Experimental.*
- `Audio.group(Symbol name, parent: Symbol = :effects, volume: Numeric | nil = nil) -> nil`: makes a mix group of the game's own under another, at volume (1 when nil); again, moves it, and changes its volume if one is given *Experimental.*
- `Audio.effects(Symbol group, low_pass: Numeric | nil = nil, reverb: Numeric = 0.0) -> nil`: muffles a group's sounds (low_pass in Hz) and sends them to its reverb (0 to 1); with neither, takes them off *Experimental.*
- `Audio.set(Integer id, volume: Numeric | nil = nil, pitch: Numeric | nil = nil) -> bool`: changes the volume or pitch of a sound that plays; false once it stopped *Experimental.*
- `Audio.play_at(String | Symbol name, Vector3 position, loop: bool = false, volume: Numeric = 1.0, pitch: Numeric = 1.0, reference_distance: Numeric = 1.0, max_distance: Numeric = 50.0, group: Symbol = :effects, priority: Integer = 128, occlusion: bool = true, velocity: Vector3 = [0, 0, 0]) -> Integer | nil`: plays a sound at a place in the world; returns its id, or nil
- `Audio.listener(Vector3 position, Vector3 forward, Vector3 up, Vector3 velocity = [0, 0, 0]) -> void`: where the ears are (move it with the camera)
- `Audio.move(Integer id, Vector3 position, Vector3 | nil velocity = [0, 0, 0]) -> bool`: moves a sound played with play_at, at that velocity (m/s; still when none is given)
- `Audio.playing?(Integer id) -> bool`: whether that sound still plays
- `Audio.stop(Integer id) -> void`: stops that sound
- `Audio.loudness(Integer | nil id) -> Float`: how loud a sound of the :voice group is as it plays, 0 to 1 (the engine's envelope follower over its own samples, before its volume); 0 for other sounds and once it ended *Experimental.*
- `Audio.stop_sources -> void`: stops every sound played with play or play_at

Every sound played gets an id, flat (`play`) or positional (`play_at`) alike: `stop`, `playing?` and `set` take either. A sound that does not loop is decoded once and each play is a voice of its own; loops and music stream, so a crossfade plays two tracks for its length. A sound plays in a mix group, `:music`, `:effects` (the default) or `:voice`, and is heard at its volume times the player's volume of its group (`Settings.music_volume`, `effects_volume`, `voice_volume`) times the master volume. A game makes groups of its own with `Audio.group(:cave, parent: :effects, volume: 0.8)`, 13 at most, and plays sounds in them by name; a group is heard at its volume times the volumes of the groups over it, and the game's groups end with it. `Audio.effects(group, low_pass:, reverb:)` puts effects on the sounds of a group and of the groups under it: `low_pass` (20 to 20000 Hz) muffles them, and the lowest cutoff over a sound is the one it gets; `reverb` (0 to 1) sends that much of each sound to the reverb of the nearest group that has one. Effects are on sounds, not on the music's tracks. A track that cannot play leaves the one playing as it was. While a sound of the `:voice` group plays, the music and the sounds of the `:music` group duck: they go down to `volume` (0.4 unless `Audio.ducking` says) over `attack` seconds, and come back over `release` seconds once no voice plays. `play_at` sources use stereo panning and inverse-distance volume, fading to silence at `max_distance`. Every frame a ray from the listener to each `play_at` sound in reach looks for the level's static bodies (walls, floors; moving bodies and characters do not count): a sound behind one fades, over `fade` seconds, to `Audio.occlusion`'s `volume` and through its `low_pass`; `occlusion: false` on `play_at` keeps a sound out of it. A `play_at` sound with a `velocity:` (or one `Audio.move` gives it), heard by a listener with a velocity of its own (`Audio.listener`'s fourth argument), plays higher coming nearer and lower going away: the Doppler effect, at 343 m/s for sound and each speed kept below half of it; `Audio.doppler` scales it. `volume` must be 0 or more and `pitch` from 0.01 to 8; `0 < reference_distance < max_distance`. Positions and directions may be arrays or `Vector3`. Update `Audio.listener(position, forward, up)` when the camera moves: nothing follows an object by itself. At most 128 sounds with ids play at once. Past that, a new sound takes the place of the weakest one playing (the lowest `priority`, a whole number from 0 to 255, then the quietest as heard, then the oldest) when its priority is at least that one's: that one stops. Otherwise it does not play and gets `nil`. Old ids are never reused.

## UI

What a script draws over the game this frame: texts, rectangles, pictures, buttons. See [gameplay.md](gameplay.md).

- `UI.text(String str, x: Numeric, y: Numeric, size: Numeric = 24, color: Color | nil = nil, alpha: Integer = 255, align: Symbol = :left, width: Numeric | nil = nil, anchor: Symbol | nil = nil) -> void`: draws a text; width: wraps it at word boundaries; color: the theme's text color unless given
- `UI.rect(x: Numeric, y: Numeric, width: Numeric, height: Numeric, color: Color = :black, alpha: Integer = 255, radius: Numeric = 0, anchor: Symbol | nil = nil) -> void`: draws a rectangle
- `UI.image(String | Symbol name, x: Numeric, y: Numeric, width: Numeric = 0, height: Numeric = 0, alpha: Integer = 255, anchor: Symbol | nil = nil, part: Array[Numeric] = [0, 0, 0, 0], tint: Color = :white) -> void`: draws a picture of images/; part: [x, y, width, height] of it; tint: a color it is multiplied by
- `UI.button(String label, x: Numeric, y: Numeric, width: Numeric, height: Numeric, color: Color | nil = nil, hover_color: Color | nil = nil, text_color: Color | nil = nil, size: Numeric | nil = nil, focused: bool = false, confirm: Array[Array[Symbol]] = [[:enter], [:a]], anchor: Symbol | nil = nil, disabled: bool = false, icon: String | Symbol | nil = nil) -> bool`: draws a button; true the frame it is clicked; disabled: grayed, does nothing; icon: a picture before its label
- `UI.text_field(String value, x: Numeric, y: Numeric, width: Numeric, height: Numeric, max_length: Integer = 64, color: Color | nil = nil, text_color: Color | nil = nil, secret: bool = false, anchor: Symbol | nil = nil) -> String`: a field to type in; returns the new value
- `UI.keyboard_visible? -> bool`: whether a phone's on-screen keyboard covers the bottom of the screen *Experimental.*
- `UI.width -> Integer`: the width of the screen, in the interface's units (UI.scale pixels each)
- `UI.height -> Integer`: the height of the screen, in the interface's units: the reference's height when it is matched
- `UI.pixel_width -> Integer`: the width of the screen, in pixels *Experimental.*
- `UI.pixel_height -> Integer`: the height of the screen, in pixels *Experimental.*
- `UI.scale -> Float`: the pixels a unit of the interface takes now: the reference screen's scale to this one (game.json "ui") times Settings.ui_scale *Experimental.*
- `UI.reference -> [Integer, Integer]`: [width, height] of the screen the interface is laid out for (game.json "ui": its "reference", or the game's window) *Experimental.*
- `UI.mouse_x -> Float`: where the pointer is across the screen, in the interface's units *Experimental.*
- `UI.mouse_y -> Float`: where the pointer is down the screen, in the interface's units *Experimental.*
- `UI.text_width(String str, size: Numeric = 24) -> Integer`: how wide a text is, in units (in the face selected, measured at the pixels it is drawn at)
- `UI.wrap(String str, width: Numeric, size: Numeric = 24) -> Array[String]`: the lines a text wraps to at word boundaries, none wider than width *Experimental.*
- `UI.text_height(String str, size: Numeric = 24, width: Numeric | nil = nil) -> Integer`: how tall a text is drawn, in pixels (wrapped to width, when given) *Experimental.*
- `UI.line_height(Numeric size) -> Integer`: pixels from a line's top to the next one's at a text size *Experimental.*
- `UI.anchored(Symbol anchor, x: Numeric, y: Numeric, width: Numeric = 0, height: Numeric = 0) -> [Integer, Integer]`: the top-left [x, y] of a box placed at an anchor, x and y from it *Experimental.*
- `UI.clip(x: Numeric, y: Numeric, width: Numeric, height: Numeric, anchor: Symbol | nil = nil) { () -> void } -> void`: draws what the block draws only inside a box *Experimental.*
- `UI.toggle(String label, bool on, x: Numeric, y: Numeric, width: Numeric, height: Numeric, focused: bool = false, anchor: Symbol | nil = nil) -> bool`: an on/off row of a settings screen; returns whether it is on
- `UI.choice(String label, Array[String] choices, Integer index, x: Numeric, y: Numeric, width: Numeric, height: Numeric, focused: bool = false, anchor: Symbol | nil = nil) -> Integer`: a row that picks one of choices; returns the index chosen
- `UI.slider(String label, Numeric value, x: Numeric, y: Numeric, width: Numeric, height: Numeric, min: Numeric = 0.0, max: Numeric = 1.0, step: Numeric = 0.05, focused: bool = false, anchor: Symbol | nil = nil) -> Float`: a row with a slider; returns the value
- `UI.font -> String | Array[String] | nil`: the face texts are drawn with: a file name of fonts/, a list of them (a fallback chain; a list of one is its name), or nil for the built-in one
- `UI.font = String | Array[String] | nil name`: draws the next texts with fonts/<name>.ttf or .otf; a list of up to 4 names draws each letter with the first face that has it, then the built-in one (nil: the built-in face)
- `UI.gradient(x: Numeric, y: Numeric, width: Numeric, height: Numeric, top: Color, bottom: Color, alpha: Integer = 255, anchor: Symbol | nil = nil) -> void`: draws a rectangle that goes from the top color to the bottom one *Experimental.*
- `UI.image_size(String | Symbol name) -> [Integer, Integer] | nil`: a picture's [width, height] in its own pixels; nil when there is none *Experimental.*
- `UI.theme -> UI::Theme`: the theme widgets are drawn with now *Experimental.*
- `UI.theme = Hash[Symbol, untyped] | nil changes`: changes the engine's look for every widget (a font it names becomes UI.font); nil goes back to it; raises ArgumentError on a wrong key or value *Experimental.*
- `UI.with_theme(UI::Theme theme) { () -> void } -> void`: draws what the block draws with another theme *Experimental.*
- `UI.shifted(x: Numeric = 0, y: Numeric = 0, alpha: Numeric = 1.0) { () -> void } -> void`: draws what the block draws moved and at a share of its opacity; the pointer follows *Experimental.*
- `UI.draw_button(String label, Hash[Symbol, Numeric] box, UI::ButtonState state, Hash[Symbol, untyped] look = {}) -> void`: draws a button in a box as the theme says, without hearing the mouse or the keys *Experimental.*
- `UI.button_state(Hash[Symbol, Numeric] box, bool focused, disabled: bool = false, id: Symbol | nil = nil) -> UI::ButtonState`: what a button in a box is this frame: focused, under the pointer, held down *Experimental.*

A focused widget (`focused: true`) is worked by Enter or a gamepad's A, and left and right change it. An anchor (`:top_left`, `:top`, `:top_right`, `:left`, `:center`, `:right`, `:bottom_left`, `:bottom`, `:bottom_right`) makes x and y offsets from that point of the screen, and puts the matching corner of what is drawn there: `UI.text("Lives", anchor: :top_right, x: -20, y: 20)`. With an anchor, a text's box is as wide as `width:` or its widest line, and `align:` lays its lines out inside it; an image is placed by the width and height given. A text breaks at its own line breaks, and with `width:` also at spaces; a word longer than a line is cut between letters. Lines are `UI.line_height(size)` apart (1.25 times the size). Inside `UI.clip`, the pointer outside the box is over nothing drawn in it. The built-in face adds glyphs as they are asked for (Greek, Cyrillic, accented letters, `€`); for another face or script, put a `.ttf` or `.otf` in `fonts/` and set `UI.font`. For several scripts, set a fallback chain: `UI.font = [:latin, :cjk, :emoji]`. A missing or unreadable face falls back to the built-in one, with a warning; glyphs no face has show as the built-in face's `?`. Right-to-left text (Arabic, Hebrew) is drawn in its order on screen, line by line, with its brackets mirrored; Arabic letters are not joined (there is no shaping), and only outline (not color) emoji faces are drawn.

## UI::Focus

Which widget of a screen the keyboard and a gamepad work: up and down move it, wrapping. See [gameplay.md](gameplay.md).

- `UI::Focus.new -> UI::Focus`: a focus on the first widget
- `update(Integer count, pad: Integer = 0) -> void`: moves it among count widgets, as up or down was pressed this frame on that gamepad (and the keyboard)
- `at?(Integer index) -> bool`: whether the widget of that index has it: pass it as focused:
- `move_to(Integer index) -> void`: puts it on a widget (one clicked, or the one a screen comes back to)
- `index -> Integer`: the widget that has it
- `reset -> void`: back to the first widget

## UI::ScrollList

Rows in a box that scrolls, drawn only inside it: the wheel, Page Up/Down, a gamepad's bumpers and right stick, and a focus. *Experimental.* See [gameplay.md](gameplay.md).

- `UI::ScrollList.new -> UI::ScrollList`: a list scrolled to its first row *Experimental.*
- `draw(Integer count, x: Numeric, y: Numeric, width: Numeric, height: Numeric, row_height: Numeric, gap: Numeric = 0, focus: UI::Focus | nil = nil, first: Integer = 0, anchor: Symbol | nil = nil, input: bool = true) { (Integer, Hash[Symbol, Integer]) -> void } -> void`: draws count rows in the box, calling the block with each shown row's index and box *Experimental.*
- `offset -> Numeric`: how far it is scrolled, in pixels from its first row's top *Experimental.*
- `offset = Numeric pixels`: scrolls it (kept within its rows when it is drawn) *Experimental.*
- `shown -> Range[Integer]`: the indexes of the rows that show, as a range *Experimental.*
- `scrolls? -> bool`: whether its rows are taller than its box *Experimental.*
- `reset -> void`: back to its first row *Experimental.*

Given a focus (`UI::Focus`), the list scrolls to the row the focus moves to, and Page Up and Page Down move the focus a page; row 0 is the focus' `first`, so rows may come after other widgets. Without one, up and down (arrows, D-pad) scroll a row. The wheel scrolls it while the pointer is over it, and scrolling away from the focused row stays until the focus moves. `input: false` leaves the keys and the wheel to something else that frame. A bar beside its right edge shows how far it is scrolled.

## UI::Theme

How the interface and the menus look: fonts, sizes, colors, buttons, layout, background, logo, footer, transitions, sounds and music. Made of layers over the engine's defaults. *Experimental.*

- `UI::Theme.default -> UI::Theme`: the engine's own look *Experimental.*
- `UI::Theme.read!(Hash[Symbol, untyped] | nil layer) -> Hash[Symbol, untyped]`: a layer read from symbols or from game.json strings; raises ArgumentError on a wrong key or value *Experimental.*
- `UI::Theme.merge(Hash[Symbol, untyped] base, Hash[Symbol, untyped] over) -> Hash[Symbol, untyped]`: two layers in one, the second over the first *Experimental.*
- `[*Symbol path] -> untyped`: a section or a setting: theme[:colors, :button] *Experimental.*
- `color(Symbol name) -> Color | nil`: one of its colors *Experimental.*
- `merged(Hash[Symbol, untyped] layer) -> UI::Theme`: this theme with a layer over it *Experimental.*
- `to_h -> Hash[Symbol, untyped]`: all its settings *Experimental.*

## UI::ButtonState

What a button is this frame: focused, under the pointer, held down, disabled; and the id of the option it is of. *Experimental.*

- `UI::ButtonState.new(focused: bool = false, hover: bool = false, pressed: bool = false, disabled: bool = false, id: Symbol | nil = nil) -> UI::ButtonState`: a state *Experimental.*
- `focused? -> bool`: whether it has the focus *Experimental.*
- `hover? -> bool`: whether the pointer is over it *Experimental.*
- `pressed? -> bool`: whether the pointer holds it down *Experimental.*
- `disabled? -> bool`: whether it cannot be picked *Experimental.*
- `id -> Symbol | nil`: the id of its menu option *Experimental.*
- `name -> Symbol`: the one that shows most: :disabled, :pressed, :hover, :focused or :normal *Experimental.*

## Debug

Developer diagnostics: console messages, frame timings and immediate world drawing.

- `Debug.log(untyped message) -> void`: says something
- `Debug.warn(untyped message) -> void`: warns
- `Debug.error(untyped message) -> void`: reports an error
- `Debug.profiling? -> bool`: whether foreground frame timings are being collected *Experimental.*
- `Debug.profiling = bool enabled`: starts a fresh profile window when enabled; disabling retains the last summary *Experimental.*
- `Debug.stats_visible? -> bool`: whether the developer statistics overlay was requested *Experimental.*
- `Debug.stats_visible = bool visible`: shows frame timings and FPS; showing also enables profiling, hiding leaves collection running *Experimental.*
- `Debug.frame_stats -> Hash[Symbol, untyped]`: a snapshot with samples, fps and latest/mean/peak timings in milliseconds *Experimental.*
- `Debug.reset_frame_stats -> void`: clears statistics, the scripts' profile and any unfinished measurement *Experimental.*
- `Debug.script_profile(Integer count = 20) -> Array[Hash[Symbol, String | Integer | Float]]`: the scripts' methods the sampling profiler caught most often running their own code: hashes of name, self, total, self_percent and total_percent *Experimental.*
- `Debug.export_script_profile -> String`: writes the whole scripts' profile as tab-separated columns to script_profile.tsv in the game's data folder; returns the file's path *Experimental.*
- `Debug.line(Vector3 from, Vector3 to, color: Color = :green, alpha: Integer = 255, depth_test: bool = true) -> bool`: draws a world-space line for this frame; returns whether it was accepted *Experimental.*
- `Debug.box(Vector3 center, Vector3 size, color: Color = :green, alpha: Integer = 255, depth_test: bool = true) -> bool`: draws an axis-aligned wire box with full nonnegative size for this frame; returns whether it was accepted *Experimental.*
- `Debug.label(untyped text, at: Vector3, color: Color = :white, alpha: Integer = 255, size: Integer = 16) -> bool`: draws text centered where the main camera shows the world point `at` for this frame; returns whether it was accepted *Experimental.*

Profiling is off by default. The overlay uses the shared game HUD, including editor Play and recordings; it is hidden when profiling is off. Timings use the real steady clock, independently of Time.scale or fixed-step replay. The most recent 120 completed frames form the window; reads during a frame expose only completed frames. FPS uses completion-to-completion intervals, and is zero until an interval is available. `latest`, `mean` and `peak` contain `frame_ms`, `interval_ms`, `has_interval`, and `input_ms`, `network_ms`, `http_ms`, `update_ms`, `physics_ms`, `presentation_ms`, `scene_ms`. Input includes pending script startup; network and HTTP include their callbacks; update includes animations, scripts, particles, loading UI and physics. Physics includes fixed-update callbacks, body synchronization and contacts, so its time overlaps update. Presentation includes uploads, drawing and any frame-cap or synchronization waits. Scene includes request polling, synchronous reads and activation; the asynchronous worker is not instrumented, while offline fixed-step waits count as scene wall time. These are foreground wall times, not CPU utilization. Where the graphics card can time its work (desktop OpenGL 3.3), the overlay also shows its time over the prepare, scene and post passes, read a frame or more late so the game never waits for the card; it shows none on OpenGL ES. While profiling, a sampling profiler reads the scripts' call stack about every millisecond of Ruby running and the overlay lists the five methods with the most self samples; time in the engine's own functions counts for the Ruby method that called them. Phase times are inclusive and must not be summed as a partition. Overlay timings and captures are nondeterministic; leave the overlay off for reproducible pictures. Debug.line and Debug.box use world coordinates and live for one frame: submit from ready/update/fixed_update. At most 4096 commands are accepted per frame; later calls return false. They are local-only and return false without a presenter, while still validating arguments. depth_test: false draws through objects. Shape frames use unjittered rendering and reset temporal history; normal temporal sampling resumes afterwards. Native windows allocate one cached color/depth target only when shapes are requested; editor, scaled and recorded views reuse their target. If that extra native target is unavailable, the game still draws with a warning and the shapes are omitted. Debug.label places its text after the frame's scripts, as the main camera then sees the point, over the game's interface and under the overlay; at most 256 labels a frame, of at most 256 bytes and a size of 1 to 256 pixels. Nothing is shown for a point behind the camera or without a main camera.

## Game

The game itself. See [gameplay.md#a-loading-screen](gameplay.md#a-loading-screen).

- `Game.load_scene(String | Symbol name) -> bool | nil`: switches to scenes/<name>.glb ("main" is scene.glb); Ruby globals survive. In a network game only the host may, and every machine changes level with it (false elsewhere)
- `Game.load_scene_async(String | Symbol name) -> bool | nil`: queues a background scene read; the old world survives failed reads and cancellation. In a network game, as load_scene (an unknown name raises) *Experimental.*
- `Game.loading? -> bool`: a scene request is queued or being read *Experimental.*
- `Game.loading_scene -> String | nil`: the requested scene name, or nil *Experimental.*
- `Game.loading_error -> String | nil`: the last asynchronous read error, or nil; cleared by the next load request *Experimental.*
- `Game.cancel_scene_load -> bool`: cancels a pending asynchronous request; returns whether one was pending (false, cancelling nothing, during a change of level with the others) *Experimental.*
- `Game.on_loading ?{ (Float) -> void } -> void`: draws the loading screen each frame with unscaled delta; no block restores the default *Experimental.*
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

File reading, glTF parsing and, for a game shown in a window, picture decoding happen on the
worker. The scene read is then made ready over the next loading frames while the old one is still
shown: its physics bodies are made in batches of about 4 ms a frame, its textures and meshes go to
the graphics card about 8 MiB a frame, and the swap happens once all of it is ready. Every script
of the new scene starts in that one frame, so a behaviour's first update sees the complete level.
Creating the scene's objects, its water and its navigation bake still take one frame each. The old
and incoming scene coexist in memory until the swap. Fixed-step offline runs (including
recordings and scripted replays) wait for the reader before advancing, then make the scene ready
all at once after exactly one loading frame, so completion depends neither on thread scheduling
nor on the real clock. Networked runs and dedicated-server rooms keep the background reader and
the spread activation even in fixed-step mode; a dedicated server's rooms share their loop. Closing the process waits for an active parser to return.

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
- `Settings.ui_scale -> Float`: how much bigger than the game made it the interface is drawn (0.5 to 2); UI.scale includes it *Experimental.*
- `Settings.ui_scale = Numeric scale`: changes it *Experimental.*
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
- `Settings.music_volume -> Float`: the player's volume of the music group, 0 to 1 *Experimental.*
- `Settings.music_volume = Numeric volume`: changes it *Experimental.*
- `Settings.effects_volume -> Float`: the player's volume of the effects group, 0 to 1 *Experimental.*
- `Settings.effects_volume = Numeric volume`: changes it *Experimental.*
- `Settings.voice_volume -> Float`: the player's volume of the voice group, 0 to 1 *Experimental.*
- `Settings.voice_volume = Numeric volume`: changes it *Experimental.*
- `Settings.save -> bool`: keeps them for the next runs; false where they last for the run only

Until the player chooses, everything is what `game.json` says. `Settings.save` keeps them only in a built game a player plays: in the editor's Play, `redgem run`, a server's rooms and tests they last for the run, and `save` returns false.

## GameMenu

A behaviour: title screen, pause menu, end screen, settings screen with its controls page, and the quest journal; arrows or the D-pad move the focus, Enter or A picks. See [gameplay.md#pause-and-the-flow-between-screens](gameplay.md#pause-and-the-flow-between-screens).

- `GameMenu.instance -> GameMenu | nil`: the menu of the scene, or nil
- `show(Symbol screen) -> void`: opens a screen: :title, :pause, :over, :settings, :journal or one of the game's
- `show_settings -> void`: opens the settings screen (window, resolution, quality, volume...; its Controls button opens the controls page)
- `resume -> void`: closes the menu and goes on
- `restart -> void`: starts the scene again
- `finish(String message) -> void`: opens the end screen with a message
- `open? -> bool`: whether the menu is open
- `screen -> Symbol | nil`: the screen open
- `screens -> Hash[Symbol, GameMenu::Screen]`: the screens, by name (:title, :pause, :over, :settings, :controls, :journal and the game's own)
- `button_box(Integer index) -> [Integer, Integer, Integer, Integer]`: where the menu's button of that index is: [x, y, width, height]
- `GameMenu.theme -> UI::Theme`: the menus' theme, before any screen's own *Experimental.*
- `GameMenu.theme = Hash[Symbol, untyped] | nil changes`: changes how the menus look (over game.json's menu theme); nil takes it away *Experimental.*
- `theme_of(Symbol name) -> UI::Theme`: the theme a screen is drawn with *Experimental.*
- `on_draw_button { (String, Hash[Symbol, Numeric], UI::ButtonState) -> void } -> void`: draws the menu's buttons with a block instead of the theme's look *Experimental.*
- `on_draw_background { () -> void } -> void`: draws the menu's background with a block instead of the theme's *Experimental.*
- `column_x(Numeric width) -> Numeric`: where a box that wide starts, as the theme aligns the menu *Experimental.*
- `column_text_x -> [Numeric, Symbol]`: where texts aligned with the menu start: [x, align] *Experimental.*
- `margin(Symbol side) -> Numeric`: one of the theme's margins (:top, :right, :bottom, :left), scaled *Experimental.*
- `GameMenu.open? -> bool`: whether any menu is open now; a HUD can hide while one is *Experimental.*

## GameMenu::Screen

A screen of the GameMenu. See [gameplay.md#pause-and-the-flow-between-screens](gameplay.md#pause-and-the-flow-between-screens).

- `GameMenu::Screen.new(heading: String | Proc, options: Array[[String | Proc, Proc]], back: Symbol | nil) -> GameMenu::Screen`: a screen with a heading and options: [[label, action], ...]
- `add_option(String | Proc label, index: Integer = -1, id: Symbol | nil = nil, enabled: Proc | nil = nil) { () -> void } -> void`: adds an option; id: its name for icons and drawn buttons; enabled: a lookup, grayed while false
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
- `option_extras -> Array[Hash[Symbol, untyped]]`: what each option says besides: {id:, enabled:} *Experimental.*
- `theme = Hash[Symbol, untyped] | nil changes`: this screen's own changes over the menu's theme *Experimental.*
- `theme_layer -> Hash[Symbol, untyped]`: what this screen changes of the menu's theme *Experimental.*
- `on_draw_button { (String, Hash[Symbol, Numeric], UI::ButtonState) -> void } -> void`: draws this screen's buttons with a block *Experimental.*
- `on_draw_background { () -> void } -> void`: draws this screen's background with a block *Experimental.*

## Particles

Short-lived sparks, smoke and dust. See [gameplay.md#sparks-dust-and-smoke](gameplay.md#sparks-dust-and-smoke).

- `Particles.burst(Vector3 position, count: Integer = 12, color: Color | Array[Color] = :white, alpha: Integer | Array[Integer] = 255, speed: Numeric = 3.0, spread: Numeric = 180.0, direction: Vector3 = Vector3.up, life: Numeric = 0.8, size: Numeric | Array[Numeric] = 0.12, gravity: Numeric = 1.0, glow: bool = false, lit: bool = false, cast_shadow: bool = false, soft: Numeric = 0.0, image: String | Symbol | nil = nil, frames: [Integer, Integer] = [1, 1], fps: Numeric = 0.0, spin: Numeric = 0.0) -> Integer`: adds particles; returns how many (size, color and alpha may be lists over their life)
- `Particles.emitter(GameObject object, rate: Numeric = 24.0, offset: Vector3 = Vector3.zero, **untyped settings) -> Particles::Emitter`: starts a continuous local effect; returns a Particles::Emitter handle; uses burst settings except count *Experimental.*
- `Particles.count -> Integer`: how many are alive
- `Particles.clear -> void`: removes all particles and stops every emitter

Emitters follow game time and stop on object destruction or scene changes. They are local visual effects, not replicated. Without a presenter, creation returns an inactive handle and no emitter is allocated or simulated. Births follow ordinary movement; movement over 10 meters per update starts them at the destination. Up to 256 emitters fairly share 4096 live particles; missed births are dropped.

## Particles::Emitter

A continuous particle effect attached to an object, returned by Particles.emitter. *Experimental.* See [gameplay.md#sparks-dust-and-smoke](gameplay.md#sparks-dust-and-smoke).

- `active? -> bool`: whether this emitter is still emitting from a live object *Experimental.*
- `stop -> void`: stops new births; existing particles finish their lives *Experimental.*

## Decals

Marks laid on the scene's surfaces: bullet holes, scorches, cracks. *Experimental.* See [gameplay.md#leave-marks-decals](gameplay.md#leave-marks-decals).

- `Decals.place(Vector3 | Hash[Symbol, untyped] point, Vector3 | nil normal = nil, size: Numeric = 0.15, depth: Numeric | nil = nil, image: String | Symbol | nil = nil, color: Color = :black, alpha: Integer = 255, spin: Numeric = 0.0) -> bool`: projects a mark onto the surfaces at a point facing a normal, or at a raycast hit (its point and normal); returns whether it fell on any *Experimental.*
- `Decals.count -> Integer`: how many marks are kept *Experimental.*
- `Decals.limit -> Integer`: how many marks are kept at most (256 unless set) *Experimental.*
- `Decals.limit = Integer count`: keeps at most this many marks, 1 to 4096: past it, placing one drops the oldest *Experimental.*
- `Decals.clear -> void`: removes every mark *Experimental.*

Marks darken what they lie on (their color multiplies it), so they take its light and shade. They stay on the object they fell on as it moves, and go with it when it is destroyed or hidden. Skinned meshes, meshes with shape keys, prefabs, the view model, water, and see-through or cut-out materials take none; a two-sided surface takes them on both sides. Marks are local: never sent to other machines, and a machine that draws nothing keeps none. A scene loaded anew starts without them.

## Scatter

Great numbers of copies of a mesh (grass, trees, rocks), drawn only. See [../levels.md#grass-forests-and-cities](../levels.md#grass-forests-and-cities).

- `Scatter.place(String | GameObject template, count: Integer, area: [Vector3, Vector3], ground: GameObject | nil = nil, seed: Integer = 0, scale: Numeric | Range[Numeric] = 1.0, align: bool = false, layer: String | Symbol | nil = nil, least: Numeric = 0.5, tints: Array[Color] | nil = nil, tint_variation: Numeric = 0.0, fade: Numeric = 0.0, normals_up: Numeric = 0.0, lods: Array[[String, Numeric]] | nil = nil) -> Scatter`: places copies; returns a set (count, remove). layer: and least: (experimental, as ground layers are) keep them to where that ground layer's painted weight is at least least (0.5 unless given; least: without layer: is refused)
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
- `Save.slot -> String`: the selected save slot name; default keeps the existing save file *Experimental.*
- `Save.slot = String | Symbol name`: selects a named slot for this engine; default restores the original store *Experimental.*
- `Save.slots -> Array[String]`: sorted names of persisted slots and slots opened in this run, including default *Experimental.*
- `Save.version -> Integer | nil`: the version of the saved data this game writes, or nil *Experimental.*
- `Save.version = Integer number`: declares that version (1 or more): older slots are migrated, newer ones refused *Experimental.*
- `Save.migrate(from: Integer) { (Hash[String, untyped]) -> Hash[String, untyped] } -> void`: the hook turning a slot of version from into from + 1: gets the values Hash, returns the new one *Experimental.*
- `Save.checkpoint(String | Symbol key = :checkpoint) -> Integer`: keeps this scene's spawned objects whose behaviours persist fields; returns how many *Experimental.*
- `Save.restore_checkpoint(String | Symbol key = :checkpoint) -> Array[GameObject]`: spawns them again in place of the current ones; returns the new copies *Experimental.*

Values are numbers, texts, true/false, `Vector3`, and arrays and hashes of those; hashes come back with string keys. Named slots start on default for every engine and keep their selection across scenes. Selection is local to each engine, including shared-database server rooms. Names are 1–48 lowercase ASCII letters/digits, hyphens or underscores, starting with a letter/digit. At most 64 named stores may be opened per run/local backend and 4096 names listed; invalid names or unavailable backends leave selection unchanged and raise a Ruby error. The existing default file/table is preserved. Named files use their own atomic writes/backups; SQLite uses separate slot tables. Save.update writes to its entry store even if its block selects another slot. Each slot records the Save.version that wrote it: the first use of a slot (or Save.slot =) runs the migration hooks it needs, in order, and writes the result back at once; a slot from a newer game, or one a hook raised on, is left untouched and every use of it raises a RuntimeError saying why (Save.slot = then keeps the previous slot). A slot written before the game declared a version counts as version 1.

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
- `url -> String | nil`: the address the answer came from, once redirects were followed *Experimental.*

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
- `Network.fingerprint -> String | nil`: the fingerprint of the game hosted here, or nil *Experimental.*
- `Network.host_code -> String | nil`: the short code players who join the game hosted here check it with, or nil *Experimental.*
- `Network.leave -> void`: leaves the game
- `Network.online? -> bool`: whether this machine is in a game
- `Network.connecting? -> bool`: whether a join is on its way
- `Network.reconnecting? -> bool`: whether this machine's connection dropped and it is getting back in *Experimental.*
- `Network.reconnect_window -> Float`: seconds the game hosted here holds a dropped player's place (0: none) *Experimental.*
- `Network.reconnect_window = Numeric seconds`: holds a dropped player's place and id that long, up to 300 (0 turns it off) *Experimental.*
- `Network.send_hz -> Integer`: poses a second a NetworkSync sends unless it says otherwise: game.json's network send_hz (20 by default) *Experimental.*
- `Network.host? -> bool`: whether this machine hosts
- `Network.server? -> bool`: whether this machine is a dedicated server
- `Network.id -> Integer`: this machine's peer id
- `Network.peers -> Array[Integer]`: the peers of the game
- `Network.players -> Array[Integer]`: the peers without a dedicated server
- `Network.validate_movement(max_speed: Numeric = 0.0, teleport_distance: Numeric = 0.0, tolerance: Numeric = 0.25, kick_after: Integer = 0, undo: bool = false) -> nil`: on the host: corrects the others' objects that move faster or further than this (no limits: off) *Experimental.*
- `Network.movement_rules -> Hash[Symbol, Numeric | bool]`: the movement validation settings, as a hash *Experimental.*
- `Network.movement_violations(Integer peer) -> Integer`: how many moves of a machine the host did not believe in this game *Experimental.*
- `Network.teleport(GameObject | String object, Vector3 | Array[Numeric] position) -> bool`: on the host (or alone): puts an object kept in step (a GameObject with a NetworkSync, or its key) there on every machine, its owner's included, with no movement violation (a respawn, a spawn point, a level's entry); false on a machine that does not host *Experimental.*
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
- `Network.on_leave { (Integer, String | nil) -> void } -> void`: someone left: |peer, why| (on the host, why it let the machine go; nil when it left by itself, and on the others)
- `Network.on_fail { (String | nil) -> void } -> void`: a join did not work out: |reason|
- `Network.on_reconnecting { (Integer) -> void } -> void`: a connection dropped and its place is held: |peer| *Experimental.*
- `Network.on_reconnected { (Integer) -> void } -> void`: a dropped machine is back in its place: |peer| *Experimental.*
- `Network.on_lost { (String | nil) -> void } -> void`: this machine could not get back in time: |reason| *Experimental.*
- `Network.authenticate { (Integer, untyped, Network::Decision) -> void } -> void`: decides who gets in: |peer, auth, decide|
- `Network.identity(Integer peer) -> untyped`: who a peer was accepted as
- `Network.player_name(Integer peer) -> String | nil`: a peer's name
- `Network.on_level_loading { (String) -> void } -> void`: the host started a change of level, on every machine: |name|; lasts across levels, replaced by the next call *Experimental.*
- `Network.on_level_ready { (String) -> void } -> void`: every machine has the new level, which starts now: |name|; lasts across levels, replaced by the next call *Experimental.*
- `Network.changing_level? -> bool`: the machines are between two levels: loading, or waiting for each other *Experimental.*
- `Network.level_timeout -> Float`: seconds the host, once it has a new level, waits for the others to have it (30); later ones are let go *Experimental.*
- `Network.level_timeout = Numeric seconds`: sets it *Experimental.*
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
- `send_hz -> Float`: poses a second its owner sends: its send_hz setting, else Network.send_hz *Experimental.*
- `send_interval -> Float`: seconds between two of its poses (1 / send_hz) *Experimental.*

## Interactable

A behaviour: something the player can use or talk to; its behaviours hear on_interact(actor), on_focus(actor) and on_blur(actor). *Experimental.* See [interaction.md](interaction.md).

- `Interactable.all -> Array[Interactable]`: every Interactable of the scene *Experimental.*
- `prompt -> String | Symbol | nil`: the I18n key of its prompt, or nil for the engine's "Interact" *Experimental.*
- `prompt = String | Symbol | nil key`: changes the prompt *Experimental.*
- `prompt_text -> String`: the prompt in the language in use *Experimental.*
- `range -> Float`: meters from the actor it can be used from (2 unless said) *Experimental.*
- `range = Numeric meters`: changes the range *Experimental.*
- `priority -> Float`: of those in reach, the highest wins (0 unless said) *Experimental.*
- `priority = Numeric value`: changes the priority *Experimental.*
- `enabled? -> bool`: whether it can be picked *Experimental.*
- `enabled = bool flag`: false: never picked *Experimental.*
- `angle -> Float | nil`: degrees off the actor's forward it may be at; nil for any direction *Experimental.*
- `angle = Numeric | nil degrees`: changes the angle (nil: any direction) *Experimental.*

## Interactor

A behaviour on the player's hero: picks the best Interactable in reach, shows its prompt and uses it with the :interact action or a tap. *Experimental.* See [interaction.md](interaction.md).

- `Interactor.all -> Array[Interactor]`: every Interactor of the scene: the players' heroes *Experimental.*
- `target -> Interactable | nil`: the Interactable in focus, or nil *Experimental.*
- `use -> bool`: uses what is in focus now; false when nothing is *Experimental.*
- `pad -> Integer`: the player whose :interact it reads (0 unless said) *Experimental.*
- `prompt_label(Interactable target) -> String`: what the prompt says for a target: "[E] Open" *Experimental.*

## Dialogue

Conversations written in dialogue files (dialogues/<name>.json): lines, choices, conditions and effects; each player has their own, decided on the host. *Experimental.* See [dialogue.md](dialogue.md).

- `Dialogue.start(String | Symbol | Dialogue::Script name, actor: GameObject, speaker: GameObject | nil = nil) -> Dialogue::Conversation | bool`: starts a conversation of the actor (a player's hero) with the speaker, on that player's machine (true when it went to another); raises on a client *Experimental.*
- `Dialogue.current -> Dialogue::Conversation | nil`: the conversation this machine's player is in, or nil *Experimental.*
- `Dialogue.busy? -> bool`: whether this machine's player is in a conversation, or left one this frame *Experimental.*
- `Dialogue.stop -> bool | nil`: ends the conversation this machine's player is in *Experimental.*
- `Dialogue.on(Symbol event) { (untyped, untyped, untyped) -> void } -> void`: :start, :line and :end |conversation| of this machine's player; :event |name, data, conversation| where effects happen *Experimental.*
- `Dialogue.load(String | Symbol name) -> Dialogue::Script`: the dialogue of that name, read from its file once (raises for one that is not there or not well made) *Experimental.*
- `Dialogue.quests -> untyped`: what reads and moves quest stages (Quests unless a game says) *Experimental.*
- `Dialogue.quests = untyped quests`: what reads and moves quest stages: an object with stage(name) and set_stage(name, stage, conversation) *Experimental.*

## Dialogue::Conversation

One player's way through a dialogue: the line it is at, the choices offered, and how it goes on. *Experimental.* See [dialogue.md#showing-a-conversation](dialogue.md#showing-a-conversation).

- `text -> String | nil`: the line, in the language in use (nil once it is over) *Experimental.*
- `text_key -> String | nil`: the I18n key of the line *Experimental.*
- `speaker_name -> String | nil`: who says it, in the language in use (nil when the node names nobody) *Experimental.*
- `speaker_key -> String | nil`: the I18n key of who says it, or nil *Experimental.*
- `voice -> String | nil`: the sound of the line's voice clip (in sounds/), or nil *Experimental.*
- `speaking? -> bool`: whether the line's voice clip plays; a line with no choices goes on by itself as it ends *Experimental.*
- `choices -> Array[Dialogue::Conversation::Choice]`: the choices offered now: those whose conditions hold *Experimental.*
- `choosing? -> bool`: whether the player answers this line by choosing *Experimental.*
- `continue -> bool`: on from a line with no choices: to what follows, or the end; false when it waits for a choice *Experimental.*
- `choose(Dialogue::Conversation::Choice | Integer choice) -> bool`: takes a choice: its effects happen, and it goes where the choice leads; false for one not offered *Experimental.*
- `stop(quietly: bool = false) -> bool`: ends it where it is (quietly: without telling the host) *Experimental.*
- `over? -> bool`: whether it ended *Experimental.*
- `node -> Hash[String, untyped] | nil`: the node it is at, or nil once it is over *Experimental.*
- `node_id -> String | nil`: the id of the node it is at, or nil *Experimental.*
- `steps -> Integer`: how many lines it has shown (a line it comes back to counts again) *Experimental.*
- `script -> Dialogue::Script`: its dialogue *Experimental.*
- `actor -> GameObject | nil`: the hero of the player in it *Experimental.*
- `speaker -> GameObject | nil`: who talks to that player, or nil *Experimental.*
- `deciding? -> bool`: whether its effects happen on this machine (offline, and on the host) *Experimental.*

## Dialogue::Conversation::Choice

An answer the player may give. *Experimental.* See [dialogue.md#showing-a-conversation](dialogue.md#showing-a-conversation).

- `index -> Integer`: its place among the node's choices (what choose takes) *Experimental.*
- `key -> String`: the I18n key of its text *Experimental.*
- `text -> String`: its text, in the language in use *Experimental.*

## Dialogue::Script

A dialogue file as it was read. *Experimental.* See [dialogue.md](dialogue.md).

- `name -> String`: its name: that of its file in dialogues/, without .json *Experimental.*
- `start -> String | Array[Hash[String, untyped]]`: where it starts: a node's id, or branches *Experimental.*
- `node(String id) -> Hash[String, untyped] | nil`: the node of that id, or nil *Experimental.*
- `node?(String id) -> bool`: whether it has a node of that id *Experimental.*

## GameVariables

The values every player shares, which dialogue conditions read and effects change; owned by the host over the network. *Experimental.* See [dialogue.md#gamevariables](dialogue.md#gamevariables).

- `GameVariables[String | Symbol name] -> untyped`: the value of a variable, or nil when it was never set *Experimental.*
- `GameVariables[String | Symbol name] = untyped value`: sets a variable (nil, true, false, a number or a text); raises on a client *Experimental.*
- `GameVariables.to_h -> Hash[String, untyped]`: every variable that was set, by name (what to save) *Experimental.*
- `GameVariables.replace(Hash[String | Symbol, untyped] values) -> Hash[String, untyped]`: puts these in the place of every variable (a save read back) *Experimental.*
- `GameVariables.on_change { (String, untyped) -> void } -> void`: |name, value| on every change *Experimental.*
- `GameVariables.value?(untyped value) -> bool`: whether a variable can hold a value *Experimental.*
- `GameVariables.name?(untyped name) -> bool`: whether a variable can be called so *Experimental.*

## Talkable

A behaviour: someone the player can talk to; using it (its Interactable, added with the prompt "Talk" when it has none) starts its dialogue. *Experimental.* See [dialogue.md](dialogue.md).

- `dialogue -> String | nil`: the name of its dialogue file *Experimental.*

## Quests

The game's quests, written in quest files (quests/<name>.json): stages with objectives (go to, talk to, deliver, collect, defeat, a check); kept in GameVariables, so the party shares them and the host owns them. *Experimental.* See [quests.md](quests.md).

- `Quests[String | Symbol name] -> Quests::Quest`: the quest of that name, started or not (raises for one with no file) *Experimental.*
- `Quests.load(String | Symbol name) -> Quests::Definition`: the quest file of that name, read once (raises for one that is not there or not well made) *Experimental.*
- `Quests.tracked -> Quests::Quest | nil`: the quest this machine's journal and markers follow: the one tracked while it is under way, else the first one under way *Experimental.*
- `Quests.track(String | Symbol | nil name) -> String | nil`: tracks a quest on this machine (nil: the first one under way) *Experimental.*
- `Quests.any? -> bool`: whether the game has quest files (its menu then has a journal) *Experimental.*
- `Quests.started -> Array[Quests::Quest]`: every quest that started, completed ones too *Experimental.*
- `Quests.active -> Array[Quests::Quest]`: every quest under way: started, and not complete *Experimental.*
- `Quests.completed -> Array[Quests::Quest]`: every quest that is complete *Experimental.*
- `Quests.stage(String | Symbol name) -> String | nil`: the stage a quest is at, or nil while it has not started *Experimental.*
- `Quests.start(String | Symbol name) -> bool`: starts a quest at its first stage (false when it had started); raises on a client *Experimental.*
- `Quests.set_stage(String | Symbol name, String | nil stage, Dialogue::Conversation | nil conversation = nil) -> String | nil`: moves a quest to a stage (nil: not started), its objectives from nothing; raises on a client and for a stage it has not *Experimental.*
- `Quests.reset(String | Symbol name) -> String | nil`: as if the quest never started *Experimental.*
- `Quests.collected(String | Symbol item, Integer count = 1) -> bool`: the players collected items: for the collect objectives (on a client, told to the host) *Experimental.*
- `Quests.delivered(String | Symbol item, to: GameObject | String, count: Integer = 1) -> bool`: the players delivered items to someone: for the deliver objectives *Experimental.*
- `Quests.defeated(GameObject | String who, Integer count = 1) -> bool`: the players defeated someone (an object, or its name): for the defeat objectives *Experimental.*
- `Quests.reached(GameObject | String place) -> bool`: a hero reached a place (an object, or its name): for the go_to objectives (a QuestTracker says it) *Experimental.*
- `Quests.check(String | Symbol name) { (Quests::Quest) -> bool } -> untyped`: a check of the game's own, for the objectives {"check": name}: |quest| true once it is passed (asked every frame on the host) *Experimental.*
- `Quests.on(Symbol event) { (untyped, untyped, untyped) -> void } -> void`: events: :start and :complete |quest|, :stage |quest, stage, from|, :objective |quest, objective|, :change |quest| *Experimental.*
- `Quests.to_h -> Hash[String, Hash[String, untyped]]`: the quests that started, for Save: {"bread" => {"stage" => "bring", "progress" => [2, 0]}} *Experimental.*
- `Quests.replace(Hash[String | Symbol, untyped] | nil saved) -> Hash[String, Hash[String, untyped]]`: puts these in the place of every quest's state (a save read back; a stage the file no longer has is left out); raises on a client *Experimental.*
- `Quests.save(Symbol | String key = :quests) -> untyped`: Save.set(key, Quests.to_h) *Experimental.*
- `Quests.restore(Symbol | String key = :quests) -> Hash[String, Hash[String, untyped]]`: Quests.replace(Save.get(key, {})) *Experimental.*

## Quests::Quest

One quest and where the party is in it (Quests[name] gives one). *Experimental.* See [quests.md#where-the-party-is](quests.md#where-the-party-is).

- `name -> String`: its name: that of its file in quests/, without .json *Experimental.*
- `definition -> Quests::Definition | nil`: its file as read (nil for a quest with no file) *Experimental.*
- `stage -> String | nil`: the stage it is at, or nil while it has not started *Experimental.*
- `started? -> bool`: whether it started *Experimental.*
- `complete? -> bool`: whether it reached a stage that ends it *Experimental.*
- `active? -> bool`: started, and not complete *Experimental.*
- `title -> String | nil`: its title, in the language in use *Experimental.*
- `description -> String | nil`: its description, in the language in use, or nil *Experimental.*
- `stage_text -> String | nil`: the text of the stage it is at, or nil *Experimental.*
- `objectives -> Array[Quests::Objective]`: the objectives of the stage it is at *Experimental.*
- `next_stage -> String | nil`: the stage it goes to once its objectives are done, or nil *Experimental.*

## Quests::Objective

One thing the party does in a stage of a quest. *Experimental.* See [quests.md#objectives](quests.md#objectives).

- `quest -> Quests::Quest`: the quest it is of *Experimental.*
- `index -> Integer`: its place among the stage's objectives *Experimental.*
- `kind -> Symbol`: :go_to, :talk_to, :deliver, :collect, :defeat or :check *Experimental.*
- `target -> String`: what it names: a place, someone, an item or a check *Experimental.*
- `count -> Integer`: how many it takes (1 unless the file says) *Experimental.*
- `progress -> Integer`: how many it has *Experimental.*
- `done? -> bool`: whether it has as many as it takes *Experimental.*
- `text -> String`: its text, in the language in use, with {progress} and {count} filled in *Experimental.*
- `radius -> Float | Integer`: a go_to's meters from its place (2 unless the file says) *Experimental.*
- `recipient -> String | nil`: a deliver's: who the items go to *Experimental.*
- `dialogue -> String | nil`: a talk_to's: the dialogue it must be, or nil *Experimental.*
- `node -> String | nil`: a talk_to's: the node it must reach, or nil *Experimental.*
- `data -> Hash[String, untyped]`: its line in the file *Experimental.*

## Quests::Definition

A quest file as it was read. *Experimental.* See [quests.md](quests.md).

- `name -> String`: its name *Experimental.*
- `title_key -> String`: the I18n key of its title *Experimental.*
- `description_key -> String | nil`: the I18n key of its description, or nil *Experimental.*
- `start -> String`: the stage it starts at *Experimental.*
- `stage(String id) -> Hash[String, untyped] | nil`: the stage of that id (its line in the file), or nil *Experimental.*
- `stage?(String | nil id) -> bool`: whether it has a stage of that id *Experimental.*
- `stage_ids -> Array[String]`: the ids of its stages *Experimental.*
- `end?(String | nil id) -> bool`: whether reaching that stage completes the quest *Experimental.*
- `objectives(String | nil id) -> Array[Hash[String, untyped]]`: the objectives of a stage, as the file has them *Experimental.*

## QuestTracker

A behaviour on the player's hero: tells the quests when it reaches a go_to objective's place, asks the game's checks on the host, and shows where the tracked quest's objectives happen (QuestMarkers). *Experimental.* See [quests.md](quests.md).


## QuestMarkers

Where the tracked quest's objectives happen, on this machine's screen: a marker over each place the camera sees and a pointer at the edge toward one it does not; the game turns them off, restyles them, locates objectives or draws them. *Experimental.* See [quests.md#markers-and-pointers](quests.md#markers-and-pointers).

- `QuestMarkers.enabled -> bool`: whether this machine shows the markers and pointers (true unless the game said) *Experimental.*
- `QuestMarkers.enabled = bool shown`: shows them or not, for the whole run *Experimental.*
- `QuestMarkers.style(**untyped changes) -> Hash[Symbol, untyped]`: changes some of the looks (color:, size:, pointer_size:, margin:, height:, distance:, image:); returns them all *Experimental.*
- `QuestMarkers.reset_style -> Hash[Symbol, untyped]`: the looks back to the engine's *Experimental.*
- `QuestMarkers.locate { (Quests::Objective) -> untyped } -> void`: |objective| where it happens: a GameObject, a Vector3, or nil for the object it names (until the next level) *Experimental.*
- `QuestMarkers.draw { (QuestMarkers::Mark) -> void } -> void`: |mark| draws each mark as the game wants, in the place of the engine's dots (until the next level) *Experimental.*
- `QuestMarkers.marks -> Array[QuestMarkers::Mark]`: what shows now: a mark for each objective under way of the tracked quest that has a place *Experimental.*
- `QuestMarkers.edge_point([Float, Float] direction, Numeric width, Numeric height, Numeric margin = QuestMarkers.style[:margin]) -> [Float, Float]`: where a pointer sits toward a direction from the middle of a screen: on the way to it, margin inside the edge *Experimental.*
- `QuestMarkers.on_screen?([Numeric, Numeric] | nil spot, Numeric width, Numeric height) -> bool`: whether a point (nil: behind the camera) is on a screen of that size *Experimental.*

## QuestMarkers::Mark

One place QuestMarkers shows: a marker on the screen, or a pointer at its edge. *Experimental.* See [quests.md#markers-and-pointers](quests.md#markers-and-pointers).

- `objective -> Quests::Objective`: the objective it shows *Experimental.*
- `x -> Float`: where it is drawn, across (the interface's units) *Experimental.*
- `y -> Float`: where it is drawn, down *Experimental.*
- `pointer -> bool`: true for a pointer at the edge of the screen (the camera does not show the place) *Experimental.*
- `direction -> [Float, Float] | nil`: a pointer's way to its place on the screen, of length 1 (nil for a marker) *Experimental.*
- `meters -> Float`: from the hero to the place *Experimental.*

## DialogueBox

A behaviour: shows the player's conversation at the bottom of the screen, with its choices; keyboard, gamepad and touch go on, choose and leave. *Experimental.* See [dialogue.md#showing-a-conversation](dialogue.md#showing-a-conversation).

- `DialogueBox.letters_shown(Integer letters, Float seconds, Float speed) -> Integer`: how many of a line's letters show after that many seconds at that speed (0: all) *Experimental.*
- `whole? -> bool`: whether the line it shows has all of its letters out (true while it shows none) *Experimental.*
- `pad -> Integer`: the player whose gamepad works it (0 unless said) *Experimental.*
- `letters_per_second -> Float`: how fast a line appears (45 unless said; 0: at once) *Experimental.*

## Interaction

Who uses what: the choice of a target, uses, and the game's handlers; over the network the host decides. *Experimental.* See [interaction.md](interaction.md).

- `Interaction.on(Symbol event) { (GameObject, GameObject) -> void } -> void`: |actor, target| on every use (:interact), and as the focus comes (:focus) and goes (:blur) *Experimental.*
- `Interaction.interact(GameObject actor, GameObject | Interactable target) -> bool`: the actor uses a target (an object or its Interactable) when it is in reach; asked of the host on a client *Experimental.*
- `Interaction.best_for(GameObject actor, Array[Interactable] candidates = Interactable.all, lenient: bool = false) -> Interactable | nil`: the Interactable an actor would use now, or nil (lenient: as the host judges a client's hero) *Experimental.*
- `Interaction.reachable?(GameObject actor, Interactable target, lenient: bool = false) -> bool`: whether an actor can use a target now: enabled, in range, in front, in sight *Experimental.*
- `Interaction.distance(GameObject actor, GameObject | Interactable target) -> Float`: meters from an actor to a target *Experimental.*
- `Interaction.angle_to(GameObject actor, GameObject | Interactable target) -> Float`: degrees between where the actor faces and where the target is, across the ground *Experimental.*

## LineOfSight

Whether one thing can see another: a ray to it hits nothing else first. *Experimental.* See [npcs.md#perception](npcs.md#perception).

- `LineOfSight.clear?(Vector3 from, Vector3 to, GameObject target, ignore: Array[GameObject] = []) -> bool`: whether a ray from a point to a point on the target hits nothing but the target (what is under it, or what carries it) first *Experimental.*
- `LineOfSight.part_of?(GameObject | nil object, GameObject whole) -> bool`: whether an object is the whole, or is under it *Experimental.*

## Perception

A behaviour: what an NPC sees (a cone checked by a ray) and hears (a radius); its behaviours hear on_see, on_lose_sight and on_hear. *Experimental.* See [npcs.md#perception](npcs.md#perception).

- `Perception.noise(at: Vector3, loudness: Numeric = 1.0, source: GameObject | nil = nil) -> Array[GameObject]`: a noise at a point, heard by every Perception near enough (loudness scales how far); the objects that heard it *Experimental.*
- `Perception.all -> Array[Perception]`: every Perception of the scene *Experimental.*
- `sees?(GameObject object) -> bool`: whether an object is in sight now: in range, in the cone, not hidden *Experimental.*
- `in_sight -> Array[GameObject]`: what it sees, as of its last update *Experimental.*
- `sees_any? -> bool`: whether it sees anything it watches *Experimental.*
- `last_seen(GameObject object) -> Vector3 | nil`: where it last saw an object, or nil *Experimental.*
- `watched -> Array[GameObject]`: what it looks for: the objects of its watch tag, or the players' heroes *Experimental.*
- `angle_to(GameObject object) -> Float`: degrees between where it looks and where an object is, across the ground *Experimental.*
- `eye_point -> Vector3`: where it looks from *Experimental.*
- `sight -> Float`: meters it sees (15 unless said) *Experimental.*
- `fov -> Float`: degrees of the whole cone (120 unless said) *Experimental.*
- `eyes -> Float`: meters above its origin it looks from (1.6 unless said) *Experimental.*
- `aim -> Float`: meters above a watched object's origin it looks at (1 unless said) *Experimental.*
- `hearing -> Float`: meters a noise of loudness 1 is heard from (10 unless said) *Experimental.*

## Perception::Noise

A noise someone heard (on_hear). *Experimental.* See [npcs.md#perception](npcs.md#perception).

- `position -> Vector3`: where it was made *Experimental.*
- `loudness -> Float`: how loud: 1 is heard as far as a listener's hearing *Experimental.*
- `source -> GameObject | nil`: the object that made it, or nil *Experimental.*
- `distance -> Float`: meters from the listener *Experimental.*

## Brain

A behaviour: an NPC's state machine, with states, transitions, guards, timers and events. *Experimental.* See [npcs.md#brain](npcs.md#brain).

- `Brain.state(Symbol | String name, **Symbol | Proc hooks) -> void`: declares a state, with its hooks (enter:, update:, exit:; enter_<name> and the like unless said) *Experimental.*
- `Brain.initial(Symbol | String | nil name = nil) -> Symbol | nil`: the state it starts in (the first declared unless said); with a name, sets it *Experimental.*
- `Brain.transition(from: Symbol | Array[Symbol], to: Symbol, **untyped options) -> void`: declares a way from a state (a list, or :any) to another: on: an event, after: seconds, if:/unless: guards *Experimental.*
- `Brain.states -> Hash[Symbol, Hash[Symbol, Symbol | Proc]]`: its states, by name *Experimental.*
- `Brain.transitions -> Array[untyped]`: its transitions, in the order they were declared *Experimental.*
- `state -> Symbol | nil`: the state it is in (nil before its first frame) *Experimental.*
- `previous_state -> Symbol | nil`: the state it was in before *Experimental.*
- `time_in_state -> Float`: seconds of game time in this state *Experimental.*
- `in?(*Symbol | String names) -> bool`: whether it is in one of these states *Experimental.*
- `fire(Symbol | String event, untyped payload = nil) -> bool`: an event: the first transition waiting for it that may be taken is; true when the state changed *Experimental.*
- `go(Symbol | String name) -> bool`: goes to a state now, whatever the transitions say *Experimental.*

## GameClock

The time of day in the game's world, on game time: what schedules follow. *Experimental.* See [npcs.md#daily-schedules](npcs.md#daily-schedules).

- `GameClock.hour -> Float`: the hour of the day, its minutes as a fraction: 0 up to 24 *Experimental.*
- `GameClock.minute -> Integer`: the minute of the hour *Experimental.*
- `GameClock.minute_of_day -> Integer`: whole minutes since midnight: 0 up to 1439 *Experimental.*
- `GameClock.day -> Integer`: the day it is, from 1 *Experimental.*
- `GameClock.to_s -> String`: the time as "HH:MM" *Experimental.*
- `GameClock.set(hour: Integer, minute: Integer = 0, day: Integer = GameClock.day) -> void`: sets the time of day in whole hours and minutes (and the day, from 1) *Experimental.*
- `GameClock.speed -> Float`: game seconds for each second of game time (60 unless said) *Experimental.*
- `GameClock.speed = Numeric value`: changes it (0 stops the clock) *Experimental.*
- `GameClock.state -> Hash[Symbol, Float]`: what to save: { seconds:, speed: } *Experimental.*
- `GameClock.restore(Hash[Symbol | String, Numeric] | nil saved) -> void`: back to a saved state; nil (nothing saved yet) leaves the clock as it is *Experimental.*
- `GameClock.seconds -> Float`: seconds of the world's time since the first day began *Experimental.*

## Schedule

A behaviour: an NPC's day, what it does from what time by the GameClock; its behaviours hear on_schedule(activity). *Experimental.* See [npcs.md#daily-schedules](npcs.md#daily-schedules).

- `current -> Symbol | nil`: the activity it is time for, or nil *Experimental.*
- `plan -> Array[[Integer, Symbol]]`: [[minute of the day, activity], ...] in the order of the day *Experimental.*
- `plan = Hash[String, Symbol | String] | String given`: changes the plan: { "06:00" => :work } or "06:00 work, ..." *Experimental.*
- `Schedule.parse(Hash[String, Symbol | String] | String given) -> Array[[Integer, Symbol]]`: a plan as given, in the order of the day *Experimental.*
- `Schedule.activity_at(Array[[Integer, Symbol]] plan, Integer minute_of_day) -> Symbol | nil`: what a plan says for a minute of the day *Experimental.*
- `Schedule.minute_of(String time) -> Integer`: "06:30" as minutes of the day (390) *Experimental.*

## HeadLook

A behaviour: turns an NPC's head toward the nearest hero in range and in front, or toward its target. *Experimental.* See [npcs.md#head-look-at](npcs.md#head-look-at).

- `target -> GameObject | Vector3 | nil`: what it looks at instead of the heroes, or nil *Experimental.*
- `target = GameObject | Vector3 | nil target`: sets it (nil: the heroes) *Experimental.*
- `nearest_hero -> GameObject | nil`: the nearest hero in range and in front of it, or nil *Experimental.*
- `bone -> GameObject`: the head's bone *Experimental.*
- `range -> Float`: meters to a hero (4 unless said) *Experimental.*
- `angle -> Float`: degrees off its forward a hero may be at, and the most the head turns (70 unless said) *Experimental.*

## VoiceMouth

A behaviour: moves a speaker's mouth with the loudness of its voice, through a shape key (morph:) or a jaw bone (jaw:, angle:). *Experimental.* See [dialogue.md#voice](dialogue.md#voice).

- `follow(Integer | nil handle) -> Integer | nil`: moves the mouth with that sound (a handle of the :voice group) in place of the one it followed; a dialogue line's voice does it for its speaker *Experimental.*
- `openness -> Float`: how open the mouth is now, 0 to 1 *Experimental.*
- `VoiceMouth.openness_at(Numeric loudness, Numeric full) -> Float`: how open a mouth is (0 to 1) for a voice of that loudness, all open from full *Experimental.*

## Dialogue::Voice

The clip of a dialogue line's voice, playing from the speaker's head in the :voice group (made by the conversation shown). *Experimental.* See [dialogue.md#voice](dialogue.md#voice).

- `Dialogue::Voice.play(String | Symbol name, GameObject | nil speaker) -> Dialogue::Voice | nil`: plays a line's clip from the speaker's head (with no place in the world for no speaker); nil when it does not play *Experimental.*
- `handle -> Integer | nil`: the clip's sound handle *Experimental.*
- `playing? -> bool`: whether the clip still plays *Experimental.*
- `follow -> void`: moves the clip to where the speaker's head is now *Experimental.*
- `place -> Vector3`: where the clip plays from: the speaker's head, or the speaker *Experimental.*
- `stop -> bool`: stops the clip *Experimental.*

## Notes

Script errors are logged with a backtrace and only disable the failing behaviour; the game keeps running. Every method takes finite numbers a float can hold: infinity, NaN or a number too large raises an `ArgumentError` instead of reaching the scene. A setting with a range raises outside it and says the range; nothing is quietly clamped, except `Vehicle#drive`'s inputs. Assigning `transform.position` to a physics object teleports it (moving bodies also stop); changing its rotation keeps its velocity. Images may be PNG, JPEG or static WebP (a name without an extension looks for `.webp`, then `.ktx2`, last), at most 8192 pixels a side; animated WebP is not read. Images may also be KTX2 (Basis Universal): decoded to RGBA8, except a scene's textures (glTF's `KHR_texture_basisu`), kept compressed on the graphics card. Missing or unreadable images (scene textures, `UI.image`, particle images) show an opaque magenta-and-black checkerboard, with a warning; a material that never named a texture keeps its plain color, and no path reads outside the game's folder.
