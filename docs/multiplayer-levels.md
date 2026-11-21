---
description: "The multiplayer-levels template: a game of two levels played together, with a lobby, a spawn point per player, a save, an NPC and a quest."
---

# A game of levels played together

The `multiplayer-levels` template is a starting point for a full game: two levels, played alone
or with others, hosted by a player or on a [dedicated server](dedicated-server.md), with someone
to talk to and a quest that goes through both levels. It is built from the editor's primitives
only (no models, sounds or pictures), so you can replace each part with your own.

```bash
redgem new my_game --template multiplayer-levels
redgem my_game
```

## What it plays like

The title screen offers **Play** and **Play together**. Walk with WASD (or a gamepad's left
stick), and press E (or the gamepad's Y) to talk or to use a door.

1. Talk to the guide (the green capsule). Say you will go: the quest "The beacon" starts.
2. Use the door (the purple frame): the whole party goes to the second level.
3. Walk up to the beacon (the yellow pillar).
4. Use the door back, and tell the guide what you found: the quest is complete.

## The parts

Each part is a small script of its own in `scripts/`, with a comment at the top that says what it
does. `main_scene.rb` sets every level up and lists them.

| Script | What it does |
|--------|--------------|
| `main_scene.rb`, `level2_scene.rb` | Set up each level as it loads: the party, the save, the hero's parts, the menus |
| `player.rb` | The hero: walks and jumps, on its own machine only |
| `spawns.rb` | Puts each machine's hero on its own spawn point (`Spawn1`, `Spawn2`...) |
| `party.rb` | Shows the other players' heroes (copies of the `Teammate` prefab) |
| `session.rb` | Plays alone, hosts, joins an address, or joins the dedicated server |
| `lobby_screen.rb` | The menu's "Play together" screen |
| `level_exit.rb` | A door to another level (its `to` property names the level) |
| `progress.rb` | The save: the level the party is on and its quests |
| `guide.rb` | Someone to talk to (the engine's `Talkable`, with `dialogues/guide.json`) |
| `hud.rb` | The controls, who you play with, and the quest's objectives |

The conversation is `dialogues/guide.json` (see [Dialogue](scripting/dialogue.md)), the quest is
`quests/beacon.json` (see [Quests](scripting/quests.md)), and every text players read is in
`locales/en.json`.

## Levels

`scene.glb` is the level called `main`, and `scenes/level2.glb` is `level2`. A door's `on_interact`
calls `Game.load_scene` with its `to`. In a game played together, a use runs on the host (or the
server), and the host's level change takes every machine along (see
[Changing level together](scripting/multiplayer.md#changing-level-together)). Each machine then
sets the new level up again: its hero goes back to its spawn point, and the other players' heroes
are made again.

To add a level, save a new scene as `scenes/level3.glb` with a `Player`, a `Teammate` prefab,
spawn points tagged `spawn`, and a door. Add `scripts/level3_scene.rb`
(`class Level3Scene < MainScene; end`), and point a door's `to` at `level3`.

## Playing together

**Play together** opens the lobby:

- **Host a game**: this machine hosts on port 7777. The others join its address. The HUD shows the
  port and the host's code (`Network.host_code`).
- **Join the address above**: type the host's address in the field, then join it.
- **Play on the server**: joins the game's dedicated server. Set its address in `SERVER`, in
  `scripts/session.rb` (`"play.example.com"` or `"play.example.com:7777"`), and run
  `redgem-server` on the game (see [Dedicated server](dedicated-server.md)).

The host (or the server) decides what counts: uses of the doors and of the guide, the quest's
progress, and the level. Each player sees their own conversation with the guide.

## The save

`Progress` keeps the level the party is on and its quests with `Save`. It saves each time a level
loads and each time a quest changes, so the save lasts across levels and between runs. Play and
Host go on from it: the quests come back, and the party goes to the saved level.

Only the machine that decides writes it: the one playing alone, the host, or the dedicated server
(in its own database). A player who joins someone else's game plays the host's progress and keeps
nothing of it. When that player leaves, their game goes back to their own save. On a dedicated
server, every room shares the server's save. A game with many rooms keys it by room or by player
(see [Online services](scripting/online-services.md)).

## Its tests

The game comes with tests of its own, which `redgem check my_game --run-tests` and the GitHub
workflow every new game gets run (see [Testing your game](testing.md)):

- `tests/quest_test.rb` plays the whole quest alone through both levels, and checks the save at
  each step;
- `tests/resume_test.rb` checks that Play goes back to the saved level and quest;
- `tests/spawns_test.rb` checks which spawn point each player gets.

The engine's own tests also play it with two players, one hosting
(`tests/system_pairs/templates/multiplayer-levels/`), and with two players on a dedicated server
(`tests/system_servers/templates/multiplayer-levels/`). Read them to see how to test your own
game with several machines.
