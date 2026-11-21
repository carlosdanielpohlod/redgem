---
description: "Dialogue: conversations written in dialogue files, with choices, conditions and effects on GameVariables and quests, started from on_interact, and over the network."
---

# Dialogue

The people of a game talk through **dialogue files**: `dialogues/<name>.json` in the game's folder
(subfolders are fine: `dialogues/village/baker.json` is the dialogue `"village/baker"`). A file is a
set of **nodes**, each a line said to the player, that lead to one another; some offer the player
**choices**. Lines and choices show I18n keys, so a conversation is translated like any other text
(see [Texts in several languages](i18n.md)). **Conditions** pick what is offered, and **effects**
change the game's shared values, `GameVariables`, and quest stages.

```json
{
  "start": [
    { "if": [{ "var": "met_baker", "is": true }], "next": "again" },
    { "next": "hello" }
  ],
  "nodes": {
    "hello": {
      "speaker": "baker.name",
      "text": "baker.hello",
      "voice": "baker/hello",
      "effects": [{ "set": "met_baker", "to": true }],
      "next": "offer"
    },
    "again": { "speaker": "baker.name", "text": "baker.again", "next": "offer" },
    "offer": {
      "text": "baker.offer",
      "choices": [
        {
          "text": "baker.buy",
          "if": [{ "var": "gold", "at_least": 5 }],
          "effects": [{ "add": "gold", "by": -5 }, { "quest": "bread", "to": "done" }],
          "next": "thanks"
        },
        { "text": "baker.bye", "end": true }
      ]
    },
    "thanks": { "text": "baker.thanks", "end": true }
  }
}
```

Someone becomes talkable with `Talkable`, which adds an [Interactable](interaction.md) with the
prompt "Talk" (unless it has one) and starts the dialogue from `on_interact`:

```ruby
find!("Baker").add_component(Talkable, dialogue: "baker")
find!("Player").add_component(Interactor)
find!("Player").add_component(DialogueBox)
```

`Talkable` takes `dialogue` and the Interactable's settings (`prompt`, `range`, `priority`,
`angle`); each can also be a Blender property of the object. A game that starts conversations
itself calls `Dialogue.start("baker", actor: hero, speaker: baker)` from its own `on_interact`.

## Nodes

| Key | What it is |
|---|---|
| `text` | The line: an I18n key. Every node has one. |
| `speaker` | Who says it: the I18n key of the name shown. |
| `voice` | A sound in `sounds/` for the line, named as `Audio.play` names one (`"baker/hello"` is `sounds/baker/hello.ogg`). See [Voice](#voice). |
| `effects` | What happens as the conversation reaches the node. |
| `next` | Where it goes on: a node's id, or a list of branches. |
| `choices` | What the player may answer, instead of `next`. |
| `end` | `true`: the conversation ends after this line. |

A node goes on in exactly one way: `next`, `choices` or `"end": true`. `start` works like `next`.

**Branches** are a list of `{ "if": [...], "next": "id" }`: the first whose conditions hold is
taken, and one with no `if` always is. When none holds, the conversation ends.

A **choice** has a `text`, maybe an `if` (it is offered only when its conditions hold) and
`effects` (they happen as it is chosen), and goes on with `next` or `"end": true`. A line whose
choices are all hidden ends the conversation when the player goes on from it.

## Conditions

`if` is a list of conditions, and holds when all of them do. A condition reads a variable or a
quest's stage and compares it with one of `is`, `not`, `at_least` and `at_most`:

```json
{ "var": "gold", "at_least": 5 }
{ "var": "met_baker", "is": true }
{ "quest": "bread", "not": "done" }
```

A variable never set is `null`. `at_least` and `at_most` take a number and never hold for a value
that is not one.

## Effects

| Effect | What it does |
|---|---|
| `{ "set": "met_baker", "to": true }` | Sets a variable (to `null`, `true`, `false`, a number or a text). |
| `{ "add": "gold", "by": -5 }` | Adds to a number (a variable never set counts from 0). |
| `{ "quest": "bread", "to": "done" }` | Moves a quest to a stage. |
| `{ "event": "sold_bread", "data": 5 }` | Tells the game: `Dialogue.on(:event)`. |

```ruby
Dialogue.on(:event) do |name, data, conversation|
  give_bread(conversation.actor) if name == "sold_bread"
end
```

## GameVariables

`GameVariables` are the values every player shares: what was said, what was given.

```ruby
GameVariables[:gold] = 10
GameVariables[:gold]                       # => 10
GameVariables.on_change { |name, value| refresh_hud }
Save.write(:variables, GameVariables.to_h) # and GameVariables.replace(Save.read(:variables) || {})
```

A name is a text of at most 64 bytes, and a value is `nil`, `true`, `false`, a number or a text of
at most 1024 bytes; a game holds at most 1024 of them. They last as levels change, until the game
ends.

## Quests

A quest's conditions and effects read and move the quests of [quest files](quests.md). A quest
with no file is only its stage, the variable `"quest.<name>"`, so a game with no `quests/` folder
can still keep stages. A game with its own quest system sets `Dialogue.quests` to an object with
`stage(name)` and `set_stage(name, stage, conversation)`, and conditions and effects go to it
instead.

## Showing a conversation

`DialogueBox` shows the player's conversation: a box at the bottom of the screen with who speaks
and the line, which appears letter by letter, and the choices under it.

| What | Keyboard | Gamepad | Touch or mouse |
|---|---|---|---|
| Show the whole line, then go on | Enter or E | Y | tap the box |
| Move among the choices | Up and Down | D-pad | |
| Take a choice | Enter or E | Y | tap it |
| Leave the conversation | Backspace | B | the Skip button |

It takes `pad` (whose gamepad works it: 0 unless said) and `letters_per_second` (45 unless said;
0 shows each line at once). While a menu is open the box shows but takes no input, and while the
player talks the Interactor has nothing in focus: the press that ends a conversation uses nothing
else. Space and a gamepad's A do not go on, since they jump. The box does not stop the hero; a
game that wants it still checks `Dialogue.busy?`.

A game with a look of its own draws the conversation itself. `Dialogue.current` is the
conversation this machine's player is in, or `nil`:

```ruby
Dialogue.on(:line) do |conversation|
  @speaker = conversation.speaker_name     # nil when the node has no speaker
  @line = conversation.text                # in the language in use
  @choices = conversation.choices          # each with .index and .text
end
Dialogue.on(:end) { |_conversation| @line = nil }

conversation.continue                      # a line with no choices: on to what follows
conversation.choose(conversation.choices.first)
Dialogue.stop                              # the player leaves it
```

`Dialogue.on(:start)` hears of a new one. What `Dialogue.on` registers lasts until the next level
is loaded, and a level that loads ends every conversation.

## Voice

A line with a `voice` plays its clip as it is shown, in the `:voice` group: the player's voice
volume applies to it, and the music ducks under it (see `Audio.ducking`). The clip plays from the
speaker's head and follows it as it moves: the bone of the speaker's `HeadLook`, or its bone called
`Head`, or the speaker itself when it has neither. A conversation with no speaker hears it with no
place in the world.

The subtitle (the line's text) shows for as long as the clip plays. A line with no choices goes on
by itself when its clip ends; the player can skip it before (`conversation.continue`), which stops
the clip. A line with choices waits for one after its clip. A voice whose sound is not there plays
nothing, and the line waits for the player, as one with no voice does.

```ruby
conversation.speaking?                     # its line's clip plays
```

A speaker with a `VoiceMouth` moves its mouth with the clip's loudness, which the engine follows as
the clip plays (`Audio.loudness(handle)`, 0 to 1): a shape key opens from 0 to 1, or a jaw bone
turns about its own X axis from the pose its clips give it.

```ruby
find!("Baker").add_component(VoiceMouth, morph: "MouthOpen")
find!("Guard").add_component(VoiceMouth, jaw: "Jaw", angle: 18.0)
```

`VoiceMouth` takes `morph` (a shape key), `jaw` (a bone; a rig's prefix, as in `mixamorig:Jaw`, is
fine), `angle` (degrees the jaw turns when the mouth is all open: 15 unless said) and `full` (the
loudness that opens it all the way: 0.25 unless said). It can also follow a clip a game plays
itself: `mouth.follow(Audio.play_at(:hum, position, group: :voice))`.

## Over the network

Each player has a conversation of their own, shown on that player's machine. `on_interact` runs
on the host (see [Interaction](interaction.md#over-the-network)), so `Dialogue.start` is called
there: for another machine's hero, it starts the conversation on that machine, and the host
follows it step by step. The client says every step it takes, and the host takes the same step
on its copy: the effects happen on the host, which then tells every machine of the variables that
changed. A step the host would not take (a choice whose conditions do not hold there) does not
happen, and the client is put back where the host is. `Dialogue.start` raises on a client, and so
does setting a `GameVariables` value there. A newcomer hears of every variable as it joins. A
line's voice plays on the machine of the player in the conversation: the other players do not hear
it.

## Checking dialogues

`redgem check` reads every dialogue file. Errors: a file that is not JSON, a key a node, choice,
branch, condition or effect cannot have (a misspelt one), a node named that is not there, a node
or choice that leads nowhere, a text key with no text in `locales/` (nor among the engine's), and a
quest that is not in `quests/`, when the game has that folder, or a stage that quest does not have.
Warnings include a node that nothing leads to,
a node whose choices all have conditions, branches that all have one, a text a language of the
game lacks, and a voice whose sound is not in `sounds/`.

```text
$ redgem check games/my_game
error dialogues/baker.json: node offer choice 1: there is no node "thank"
warning dialogues/baker.json: node hello: no text for baker.hello in locales/pt.json
check: 1 errors, 1 warnings (12 scripts, 2 scenes)
```

A file the check refuses also raises when a script loads it, saying what is wrong.

The editor's [Story panel](../editor.md#story-panel) steps through a dialogue without Play,
showing each choice's conditions and effects on scratch GameVariables.
