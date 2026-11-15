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
      "voice": "baker/hello.ogg",
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
| `voice` | A sound in `sounds/` for the line. It is kept, but nothing plays it yet. |
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

Until a game has quests of its own, a quest's stage is the variable `"quest.<name>"`. A game with
its own quest system sets `Dialogue.quests` to an object with `stage(name)` and
`set_stage(name, stage, conversation)`, and conditions and effects go to it instead.

## Showing a conversation

`DialogueBox` shows the player's conversation: a box at the bottom of the screen with who speaks
and the line, which appears letter by letter, and the choices under it.

| What | Keyboard | Gamepad | Touch or mouse |
|---|---|---|---|
| Show the whole line, then go on | Enter, Space or E | A or Y | tap the box |
| Move among the choices | Up and Down | D-pad | |
| Take a choice | Enter, Space or E | A or Y | tap it |
| Leave the conversation | Backspace | B | the Skip button |

It takes `pad` (whose gamepad works it: 0 unless said) and `letters_per_second` (45 unless said;
0 shows each line at once). While a menu is open the box shows but takes no input, and while the
player talks the Interactor has nothing in focus: the press that ends a conversation uses nothing
else. The box does not stop the hero; a game that wants it still checks `Dialogue.busy?`.

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

## Over the network

Each player has a conversation of their own, shown on that player's machine. `on_interact` runs
on the host (see [Interaction](interaction.md#over-the-network)), so `Dialogue.start` is called
there: for another machine's hero, it starts the conversation on that machine, and the host
follows it step by step. The client says every step it takes, and the host takes the same step
on its copy: the effects happen on the host, which then tells every machine of the variables that
changed. A step the host would not take (a choice whose conditions do not hold there) does not
happen, and the client is put back where the host is. `Dialogue.start` raises on a client, and so
does setting a `GameVariables` value there. A newcomer hears of every variable as it joins.

## Checking dialogues

`redgem check` reads every dialogue file. Errors: a file that is not JSON, a key a node, choice,
branch, condition or effect cannot have (a misspelt one), a node named that is not there, a node
or choice that leads nowhere, a text key with no text in `locales/` (nor among the engine's), and a
quest that is not in `quests/`, when the game has that folder. Warnings: a node nothing leads to,
a node whose choices all have conditions, branches that all have one, and a text a language of the
game lacks.

```text
$ redgem check games/my_game
error dialogues/baker.json: node offer choice 1: there is no node "thank"
warning dialogues/baker.json: node hello: no text for baker.hello in locales/pt.json
check: 1 errors, 1 warnings (12 scripts, 2 scenes)
```

A file the check refuses also raises when a script loads it, saying what is wrong.
