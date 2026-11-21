---
description: "Quests: quest files with stages and objectives (go to, talk to, deliver, collect, defeat, a check of the game's own), their events, the menu's journal, markers and pointers to the objectives, saving them, and how the party shares them over the network."
---

# Quests

A game's missions are **quest files**: `quests/<name>.json` in the game's folder (subfolders are
fine: `quests/village/bread.json` is the quest `"village/bread"`). A quest is a set of **stages**.
Each stage has **objectives**, what the party does in it, and once they are all done the quest goes
on to the stage's `next`. A stage with `"end": true` completes the quest. Texts are I18n keys, so a
quest is translated like any other text (see [Texts in several languages](i18n.md)).

```json
{
  "title": "bread.title",
  "description": "bread.description",
  "start": "flour",
  "stages": {
    "flour": {
      "text": "bread.flour",
      "objectives": [
        { "go_to": "Mill", "radius": 3, "text": "bread.go_to_mill" },
        { "collect": "flour", "count": 3, "text": "bread.collect" }
      ],
      "next": "bring"
    },
    "bring": {
      "text": "bread.bring",
      "objectives": [
        { "deliver": "flour", "to": "Baker", "count": 3, "text": "bread.deliver" },
        { "talk_to": "Baker", "dialogue": "baker", "node": "thanks", "text": "bread.talk" }
      ],
      "next": "bake"
    },
    "bake": {
      "text": "bread.bake",
      "objectives": [{ "check": "oven_hot", "text": "bread.oven" }],
      "next": "done"
    },
    "done": { "text": "bread.done", "end": true }
  }
}
```

```ruby
Quests.start(:bread)
find!("Player").add_component(QuestTracker)
```

A quest starts with `Quests.start`, or with a dialogue's effect `{ "quest": "bread", "to": "flour" }`.

## Stages

| Key | What it is |
|---|---|
| `text` | What the stage is about: an I18n key. Every stage has one. |
| `objectives` | What the party does in it: a list of 1 to 16 objectives. |
| `next` | The stage it goes to once its objectives are done. |
| `end` | `true`: reaching this stage completes the quest. |

A stage with objectives has a `next`. A stage that ends the quest has neither. A stage with neither
waits: a dialogue's effect or a script (`Quests.set_stage`) moves it on.

## Objectives

Each objective is one of these, with a `text` (an I18n key that may show `{progress}` and
`{count}`: "Flour ({progress}/{count})"):

| Objective | Done when |
|---|---|
| `{ "go_to": "Mill", "radius": 3 }` | A hero with a `QuestTracker` comes within `radius` meters of the object `Mill` (2 unless said). |
| `{ "talk_to": "Baker", "dialogue": "baker", "node": "thanks" }` | A conversation has `Baker` as its speaker. `dialogue` and `node` are optional: that dialogue, at that node. |
| `{ "deliver": "flour", "to": "Baker", "count": 3 }` | The game says so: `Quests.delivered(:flour, to: baker)`. |
| `{ "collect": "flour", "count": 3 }` | The game says so: `Quests.collected(:flour)`. |
| `{ "defeat": "Wolf", "count": 2 }` | The game says so: `Quests.defeated(wolf)`. |
| `{ "check": "oven_hot" }` | The game's check is passed (see below). |

`count` is 1 unless said. Someone or a place is named as the object is; a copy that Blender
numbered counts too, so `"Wolf"` is also `Wolf.001`. An item is a name the game chooses. Only the
objectives of a quest's current stage count: what the players did before does not.

A **check** is the game's own, in Ruby. It is asked every frame on the host (by a `QuestTracker`)
while an objective waits for it, and as its stage starts:

```ruby
Quests.check(:oven_hot) { |quest| GameVariables[:oven] == "hot" }
```

## Where the party is

```ruby
quest = Quests[:bread]
quest.stage                          # => "bring" (nil before it starts)
quest.title                          # in the language in use
quest.stage_text
quest.objectives.map(&:text)         # => ["Bring the flour (1/3)", "Talk to the baker"]
quest.objectives.first.done?
quest.active?                        # started, and not complete
quest.complete?
Quests.active                        # every quest under way
Quests.completed
```

## The journal

The pause menu of a game with quest files has a **Journal** page. It lists the quests under way,
each with the text of the stage it is at and its objectives (a filled box once one is done), then
the quests completed. Its lines scroll and take the focus: up and down (the arrows, a gamepad's
D-pad) move between them, Page Up and Page Down (the bumpers), the mouse wheel and the right stick
scroll. Enter, a gamepad's A, a click or a tap on a line of a quest tracks that quest. Back, Esc or
Start go back to the pause menu.

```ruby
Camera.main.add_component(GameMenu, journal: false)   # no journal: the game has its own
GameMenu.instance.show(:journal)                       # opens it, from a key of the game's
Quests.tracked                                         # the quest the markers follow
Quests.track(:bread)                                   # (nil: the first one under way)
```

The tracked quest is the one picked, while it is under way; else the first one under way. Each
machine tracks its own.

## Markers and pointers

The `QuestTracker` of this machine's hero shows where the tracked quest's objectives happen: a
marker over each place the camera sees, with the meters to it, and a pointer at the edge of the
screen toward each place it does not (one behind the camera is pointed at from the bottom, on its
side). An objective's place is the object it names: a `go_to`'s place, a `talk_to`'s speaker, a
`deliver`'s recipient. An objective that is done, or that has no object of that name in the scene,
shows nothing. Nothing shows while the menu is open, nor on a dedicated server. A `QuestTracker`
that is not on the hero (one for the checks only) is added with `markers: false`.

`QuestMarkers` is **experimental**: it may change in any release.

```ruby
QuestMarkers.enabled = false                    # none at all
QuestMarkers.style(color: :ruby, size: 24, pointer_size: 30, margin: 40, height: 2.0, distance: false)
QuestMarkers.style(image: :quest_marker)        # images/quest_marker.png in the place of the dots
QuestMarkers.reset_style
QuestMarkers.locate { |objective| find("Mill") if objective.kind == :collect }   # an object or a Vector3
QuestMarkers.draw { |mark| UI.text("!", x: mark.x, y: mark.y, align: :center) }  # the game draws each
QuestMarkers.marks                              # => what shows now: QuestMarkers::Mark each
```

| Style | What it is |
|---|---|
| `color` | Of the markers and the pointers. |
| `size` | Units across a marker. |
| `pointer_size` | Units across a pointer. |
| `margin` | Units between a pointer and the edge of the screen. |
| `height` | Meters above its place a marker floats. |
| `distance` | Whether the meters to a place show under its mark. |
| `image` | A picture of `images/` drawn in the place of the dots (`nil`: the dots). |

A `Mark` has the `objective`, `x` and `y` on the screen (in the interface's units), `pointer`
(true at the edge), `direction` (a pointer's way to its place, `[x, y]` of length 1) and `meters`.
`QuestMarkers.edge_point(direction, width, height, margin)` is where a pointer sits.
`enabled` and the style last for the whole run; what `locate` and `draw` register lasts until the
next level is loaded.

## Events

```ruby
Quests.on(:start) { |quest| show_banner(quest.title) }
Quests.on(:stage) { |quest, stage, from| Debug.log("#{quest.name}: #{from} -> #{stage}") }
Quests.on(:objective) { |quest, objective| play_sound(:tick) if objective.done? }
Quests.on(:complete) { |quest| GameVariables[:gold] = GameVariables[:gold].to_i + 10 }
Quests.on(:change) { |quest| refresh_journal }
```

`:change` is heard on every change, also when the quests are put back from a save or a newcomer
hears of them. The others are heard only as a quest moves on. Every machine hears them. What
`Quests.on` and `Quests.check` register lasts until the next level is loaded.

## Saving

A quest's state lives in [GameVariables](dialogue.md#gamevariables): `quest.<name>` is its stage,
and `quest.<name>#<n>` is how far objective `n` of that stage has come. So the quests last as
levels change, and a game that saves its GameVariables saves them too. To keep only the quests:

```ruby
Quests.save                  # Save.set(:quests, Quests.to_h)
Quests.restore               # Quests.replace(Save.get(:quests, {}))
Quests.to_h                  # => { "bread" => { "stage" => "bring", "progress" => [1, 0] } }
```

A saved stage that the quest's file no longer has is left out, and the log says so. A game that
renames a stage moves old saves along with [Save's migrations](gameplay.md#migrate-saves-when-the-game-changes):

```ruby
Save.version = 2
Save.migrate(from: 1) do |data|
  bread = (data["quests"] || {})["bread"]
  bread["stage"] = "bake" if bread && bread["stage"] == "oven"
  data
end
```

## Over the network

The party shares the quests, and the host owns them: their state is GameVariables, which the host
changes and tells every machine of. A newcomer hears of every quest as it joins. On a client,
`Quests.start`, `set_stage` and `replace` raise. What a client's player collects, delivers, defeats
or reaches is told to the host, which counts it. Conversations are followed on the host (see
[Dialogue](dialogue.md#over-the-network)), so a `talk_to` counts there, and so do the checks.
Every machine's journal and markers show the party's quests, as the host told them, and each
player tracks a quest of their own.

A dedicated server has no hero: it needs a `QuestTracker` on any object for its checks.

## Checking quests

`redgem check` reads every quest file. Errors: a file that is not JSON, a key that a quest, stage or
objective cannot have, a stage named that is not there, a stage that leads nowhere once its
objectives are done, an objective that could never be done (a `count` under 1, a `radius` of 0, a
delivery to nobody), a dialogue that is not in `dialogues/`, a node that its dialogue does not
have, an item that is not in `items/` (when the game has that folder: `items/<name>.json`), and a
text key with no text in `locales/`. A dialogue that moves a quest to a stage, or compares one
with a stage, that the quest does not have is an error too. Warnings: a quest with no stage that
ends it, and a text that a language of the game lacks.

```text
$ redgem check games/my_game
error quests/bread.json: stage flour: there is no stage "bring"
error dialogues/baker.json: node thanks: quest bread has no stage "baked"
check: 2 errors, 0 warnings (12 scripts, 2 scenes)
```

A file the check refuses also raises when a script loads it, saying what is wrong.

The editor's [Story panel](../editor.md#story-panel) starts a quest and moves it from stage to
stage without Play.
