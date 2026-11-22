---
description: "Multiplayer: hosting and joining, messages, synced objects, relays and rooms."
---

# Multiplayer

Games can be played together over a local network or the internet. One machine **hosts** the game
and the others **join** it; every machine has an id (the host is 1, then 2, 3... as players arrive).
Up to 16 machines play together by default. A game sets its own number, from 2 to 63, in the
`network` section of its `game.json`: `{"network": {"max_players": 32}}` (see
[Room size](#room-size)).

A game that plays online says so in its `game.json` with `"network": true`: its Android app
then asks the phone for the network, which an app without it does not get (a build of a game
that says nothing guesses it from its scripts, and warns; see
[Building and sharing](../shipping.md#icon-version-and-splash-picture)).

There are three ways for the others to get to the host, from the easiest, and a fourth where
nobody hosts:

| Where the players are | How they join | What it takes |
|-----------------------|---------------|---------------|
| On the same network (same Wi-Fi) | The game is found by itself | Nothing |
| Anywhere | A room code like `K7QM3P` | A relay running somewhere on the internet |
| Anywhere | The host's address | The host's router forwarding a port |
| Anywhere | The server's address | The game running on a [dedicated server](../dedicated-server.md) |

## Host and join

```ruby
Network.host                           # on one machine (port 7777; port: 9000 picks another)
Network.join("192.168.0.12")           # on the others: the host's address

Network.on_connect { start_round }     # this machine is in the game
Network.on_fail { |reason| show("Could not join: #{reason}") }  # reason: nil, the host's, or a timeout
Network.on_join { |peer| ... }         # someone else arrived
Network.on_leave { |peer, why| ... }   # someone else left (why: on the host, why it let it go)
Network.on_disconnect { ... }          # the game is gone: the host left or the connection dropped
```

`Network.online?`, `Network.host?`, `Network.id` and `Network.peers` (everyone else's ids) tell
where things stand. A join that hears nothing back for 5 seconds gives up, and `on_fail` gets
`"the game did not answer in time"`. That time, like every wait of the network, is real time: a game
run with no window (a headless test) plays its frames faster than that, and they do not count. `Network.leave` leaves the game; when the host leaves, it ends for everyone.

Every machine in a game runs the same version of the engine's network protocol, and so do the relay
and the dedicated server it meets on. A machine of another version is turned away: `on_fail` gets
the reason (`"this game runs protocol 1, the other side 2"`), and both sides write it to the log.

A host lets 4 machines from one address be joining at the same moment (more are hung up on, and
try again). Players behind one shared address (a campus, some mobile networks) joining together
may need more: `Network.host(waiting_per_address: 16)`, up to 32.

## Games on the same network

A hosted game answers the machines around it, so nobody types an address:

```ruby
Network.host(name: "Ana's game")       # name: the game's title unless said; visible: false hides it

Network.search                         # on the others: start looking
Network.games                          # => [{ name: "Ana's game", address: "192.168.0.12", port: 7777, players: 1 }]
game = Network.games.first
Network.join(game[:address], port: game[:port])
Network.stop_search
```

`Network.games` changes as games come and go, so a menu draws a button for each one every frame.
Every RedGem game looks on the same port, so games see each other's; a game that wants to see
only its own picks another with `Network.discovery_port = 7790` (the same on every copy).

## Over the internet: rooms

Home routers let connections out, not in, so two players at home cannot reach each other
directly. They can both reach a **relay**: a small program on a machine with a public address.
One player opens a room and gets a short code; the others join with the code:

```ruby
Network.relay = "relay.example.com"    # where the relay runs ("host:port" for another port)

Network.host(room: true)               # then, once on_connect fired:
Network.room                           # => "K7QM3P": tell it to the others

Network.join(room: "K7QM3P")             # on the others
```

Everything else is the same in a room: messages, `NetworkSync`, `Network.spawn`. Whoever opened
the room is the host; when they leave, the room closes.

The relay comes with the engine, as a program of its own: `redgem-relay`. It is small, opens no
window and needs no graphics card or display, so it runs on any machine the players can reach (a
small rented server is enough, it only passes messages on):

```bash
./build/src/redgem-relay               # listens on port 7778; --port <number> picks another
./build/src/redgem-relay --fingerprint # shows its fingerprint (see "Encrypted connections")
docker compose up relay                # or with Docker
```

The relay keeps its TLS identity in `./redgem-relay-data` (`--data <folder>` picks another): it
makes one the first time it starts, and prints its fingerprint every time.

One address may hold 24 machines and 8 open rooms on a relay. When many players share one
address (a campus, some mobile networks), raise them: `--per-address <number>` (up to 1024) and
`--rooms-per-address <number>` (up to 256).

The relay also answers STUN Binding requests on the same port number over UDP (open UDP 7778 as
well as TCP to let them through): a machine that asks learns the public address and port its
router gives it. Games do not use this yet (see KNOWN_LIMITATIONS.md); when the UDP port cannot be
had, the relay says so and runs without it. Behind a proxy of UDP (Docker's userland proxy,
rootless Docker) the relay sees the proxy's address and its answers name that one: publish the
port without a proxy (rootful Docker with `"userland-proxy": false`, or host networking).

While nobody talks to it the relay sleeps: it wakes when a machine connects or says something,
and about once a second to let go of machines that went silent.

The relay expects strangers: one address can only open a few rooms and hold a few connections,
guessing room codes gets it kept out for a while, and a machine that goes silent is let go, as is
one that sends far more than a game does (each message counts for 64 bytes on top of its own, so
floods of tiny messages count too).

## By the host's address

Without a relay, players anywhere can still join the host's public address
(`Network.join("203.0.113.7")`) if the host's router forwards the game's port (7777, TCP) to the
host's machine.

A host reached directly, whether it is a player or a dedicated server, lets go of a machine that
sends far more than a game does: more than 256 KB a second over time (and a burst of 2 MB), or
more than 8192 messages a second (and a burst of 16384), however short they are.

## On a dedicated server

A dedicated server runs the game itself, with no player, and everyone joins it
(see [Dedicated server](../dedicated-server.md)):

```ruby
Network.join("play.example.com")                  # any room with a free place
Network.join("play.example.com", room: "K7QM3P")  # the room a friend is in (their Network.room)
```

The server runs the same scripts with `Network.server?` true. It hosts the game (`Network.host?`,
id 1) but plays nobody, so skip "my player" there, and use `Network.players` (everyone else
without the server) where a game shows the other players:

```ruby
Network.on_connect do
  spawn_my_hero unless Network.server?
  Network.players.each { |peer| show_hero_of(peer) }
end
```

To let in only players who logged in, the join carries their token and the server's game decides
(`Network.join(address, auth: { token: token })`, `Network.authenticate`): see
[Online services](online-services.md).

## Encrypted connections

Every connection of a game is encrypted (TLS 1.2 or 1.3), whether it goes to a player hosting, a
relay or a dedicated server: nobody on the way reads or changes what the players say, a login
token in `auth:` included. A game has no way to turn it off, and a machine that talks in the clear
is let go.

Encryption alone does not say *who* answers. A relay or a server proves itself with a certificate
of its own, made the first time it starts and kept in its data folder; its **fingerprint** (a
SHA-256 of that certificate, 64 hexadecimal digits) is printed when it starts, and by
`redgem-relay --fingerprint` or `redgem-server <game> --fingerprint`. A game that knows it pins
it, and refuses any other machine (`on_fail` says so) before a word of the game is sent:

```ruby
Network.join("play.example.com", fingerprint: "3F:A2:C1:...")      # the whole fingerprint
Network.relay = "relay.example.com"
Network.host(room: true, fingerprint: "3F:A2:C1:...")               # the relay's
```

With a room (`room:`), the fingerprint is the relay's or the server's, and only the whole of it is
taken. Or, once for the whole game, in `game.json` (an entry without a `port` is the address's on
every other port; a fingerprint given to the call wins):

```json
{
  "servers": [
    { "address": "play.example.com", "port": 7777, "fingerprint": "3F:A2:C1:..." },
    { "address": "relay.example.com", "fingerprint": "9B:04:7E:..." }
  ]
}
```

A player hosting a game at its own address makes a new identity each time it hosts.
`Network.host_code` is a short form of its fingerprint (`"3FA2C-1B09E"`, its first 10 digits) to
show next to the address, for the others to join with. (A room on a relay has none: its players
reach the relay, and check the relay's fingerprint.)

```ruby
UI.text("Code: #{Network.host_code}", x: 20, y: 20)              # on the host
Network.join("192.168.1.20", fingerprint: "3FA2C-1B09E")          # on the others
```

Without a fingerprint the connection is still encrypted, but whoever answers is believed (the log
says so): fine on a local network, not enough against someone who can redirect the players'
traffic. A machine whose handshake is not over in 3 seconds is let go, without holding up anyone
else.

## Messages

Machines talk in named messages:

```ruby
Network.on(:hit) { |data, from| @health -= data[:damage] }

Network.emit(:hit, { damage: 3, at: transform.position })   # to everyone else
Network.emit(:hit, { damage: 3 }, to: 2)                    # to one machine
Network.emit(:score, 120, to: :host)
```

A message carries `nil`, true/false, numbers, texts, `Vector3`, and arrays and hashes of those
(hash keys arrive as symbols; a key no script of the receiving game uses anywhere arrives as a
text instead, so `data[:damage]` works as written and only keys nobody asked for stay texts).
An array or a hash holds at most 4096 things (in a message: `Save` keeps any number). Messages arrive whole and in the order they were sent. Handlers last
until the next level is loaded, so register them in a behaviour's `start` or a scene script's
`setup`. Names starting with `$` are the engine's own.

## Reconnecting after a drop

A phone switching from Wi-Fi to mobile data, or a router restarting, drops a player's connection
for a moment. The game hosting (directly, or on a dedicated server) can hold that player's place
and id for a while, so that it gets back in as if nothing happened:

```ruby
# on the host (on a dedicated server: in a manager's start)
Network.reconnect_window = 20   # seconds; 0 (the default) turns it off, 300 at most

Network.on_reconnecting { |peer| show_banner("#{peer} lost the connection...") }  # on the host and on that machine
Network.on_reconnected { |peer| hide_banner }   # back, with the same id
Network.on_lost { |reason| go_to_menu(reason) } # on that machine: it could not get back in time
```

The machine whose connection dropped stays in the game (`Network.reconnecting?` is true,
`Network.id` and `Network.peers` stay the same, the others' objects stay theirs, so
`NetworkSync#mine?` is still false for them; nothing can be sent meanwhile) and calls the
host again every second. Its welcome gave it a secret token, which is what gets the place back:
a machine that does not have it is turned away, a token gets a place back once (each return
gives a new one), and once the window is over the place is gone. The window is the one the
player's welcome told: setting another one later counts for those let in afterwards. The others hear the player left
(`on_leave`) only when the window is over; if it gets back first they hear nothing. A player
who leaves (`Network.leave`) says so and holds no place; a host that leaves ends the game at once
for everyone, as before.

What was said while it was away is not replayed. When it is back, the host tells it the spawned
objects as they are now (`Network.spawn`), it starts what it knows of the others' objects over,
and the peers that came and went meanwhile arrive and leave (`on_join`, `on_leave`) on its
machine. Messages of the game (`Network.emit`) sent meanwhile to it are lost, so keep what
matters in state the host can send again (in an `on_reconnected` handler on the host).

## Objects in the same place everywhere

A `NetworkSync` keeps an object where its **owner** has it: the owner's machine moves it as usual
and the others see it follow. Only the owner should read input or decide where it goes (with no
game on, everything is this machine's, so the same script works in single player):

```ruby
class Player < Behaviour
  def update(dt)
    return unless get_component(NetworkSync).mine?
    # ... read the keyboard and move, as in a single-player game
  end
end
```

For objects that are in the level on every machine (a moving platform, a ball), add a `NetworkSync`
to them; the host owns them unless you say otherwise:

```ruby
find!("Lift").add_component(NetworkSync)
find!("Ball").add_component(NetworkSync, owner: 2, key: "ball")   # key: its name on the network
```

A key is one object's: a second `NetworkSync` under a key another object goes by already raises
an error (the behaviour is disabled, the first object keeps the key), and so does `assign` to it.
A key is at most 128 bytes long.

The owner sends where its object is 20 times a second (see [Send rate](#send-rate)), in a few bytes: only what changed since the
last time (an object standing still sends little more than its key). The other machines show it a moment behind,
blended between the last two places they heard of, so it moves smoothly however unevenly those
arrive. The delay follows the network: about 75 milliseconds when places arrive steadily, longer
(up to 350) when they come in bursts. When the next place is late, the object goes on the way it
was going for a fifth of a second, then waits; moved far at once (more than 6 meters), it jumps
there instead of sliding across. Your own objects are never delayed: they are moved on your
machine. Each watcher can tune it per object:

```ruby
add_component(NetworkSync, delay: 0.2)            # always this many seconds behind
add_component(NetworkSync, interpolate: false)    # no delay: chases the newest place, less smoothly
```

### Send rate

A game can send poses more often, for every object, with the `network` section of its
`game.json` (5 to 128 a second; such a section also says the game plays online, as
`"network": true` does):

```json
{ "tick_hz": 128, "network": {"send_hz": 64} }
```

and one object can differ from the game's rate:

```ruby
add_component(NetworkSync, send_hz: 60)   # a player in a shooter: as often as the frames go
add_component(NetworkSync, send_hz: 5)    # a slow lift: a few poses are enough
```

Every machine needs the same rate for an object: the watchers expect its poses that often, and
show it about one and a half intervals behind (75 ms at 20 a second, never less than 50 ms
nor more than 350 ms, which is why 5 a second is the least). An object's `send_hz` that is
not a number from 5 to 128 is warned of: one out of those limits is kept within them, and one
that is not a number sends at the game's rate.
`Network.send_hz` is the game's rate. An owner sends at most one pose a frame, so a rate above
the frame rate is the frame rate. More poses cost bandwidth: about 700 bytes a second per moving
player at 20 a second, about 2.2 KB at 64, to every other machine.

### Room size

A room holds 16 machines unless the game says otherwise. `"network": {"max_players": 32}` lets 32
machines into a room the game hosts or opens on a relay. It may be from 2 to 63. `redgem check`
refuses a number out of those limits, and a game played with one is warned and keeps 16. A
dedicated server takes the same number of players for each of its rooms, unless `--max-players`
says otherwise; the server is one machine more than its players. A newcomer to a full room is turned away. A relay and a dedicated server let one address
connect 24 machines (`--per-address`): raise it when a whole room plays behind one address.

Traffic grows with the square of the room: each moving player's poses go to every other machine.
At 20 poses a second a room of 32 moving players sends about 32 × 31 × 700 bytes, near 700 KB a
second in all, which a dedicated server or a relay sends out.

What the object's own animation does goes along with its place: play an attack on the owner's
machine (`animation.play(:Attack, loop: false, fade: 0.1)`) and the others see the same clip,
from the same moment of it, as far behind as they show the object, fading in as it does there.
So do its speed, a pause, a clip mixed over it (`animation.mix`) and going back to the level's
clips (`animation.follow_scene`). Only changes are sent, as part of the object's place, and the
whole of it now and then, so a machine that joins in the middle of a fight sees it too. Clips are
known by name: every machine needs the same ones, and a name is at most 64 bytes long. To leave a
character's animation to each machine's own scripts:

```ruby
add_component(NetworkSync, animation: false)
```

For objects created while playing, `Network.spawn` makes the copy on every machine, including the
ones that join later, and removes it when its owner leaves:

```ruby
Network.on_connect { Network.spawn("Hero", position: spawn_point) }   # one hero per player
```

The owner's copy has its physics; the others' copies are only shown, so what a spawned object
touches is decided on its owner's machine and told to the others in a message.

A machine only creates for others what the game itself spawns: templates it has passed to
`Network.spawn`, `prefab = true` objects, and the ones named with
`Network.allow_spawn("Hero", "Bullet")` (worth saying in a scene script's `setup`). And each
machine can only spawn objects of its own: 64 at a time, 512 in the whole game. Past that
`Network.spawn` makes nothing and returns `nil`.

## Changing level together

In a game played together the level is the host's to change: on the host (a player hosting, or
the game on a dedicated server), `Game.load_scene("level2")` changes the level of every machine.
The connection stays up; nobody leaves the game.

```ruby
Game.load_scene("level2")                # on the host: true, and every machine loads level2
Network.on_level_loading { |name| ... }  # on every machine, as the change starts (the old level is still there)
Network.on_level_ready { |name| ... }    # on every machine, once all of them have the level: it starts now
Network.level_timeout = 20               # seconds the host, once it has the level, waits for the rest (30 unless said)
```

What happens, step by step:

1. The host tells every machine the level, and every machine loads it in the background (in a
   network game `Game.load_scene` reads like `Game.load_scene_async`), showing the loading
   screen (`Game.on_loading`).
2. A machine that has the level waits, its new level frozen as while loading: its behaviours do
   not start, nothing moves, nothing it spawns is sent yet. It tells the host it is ready.
3. When every machine is ready, or `Network.level_timeout` seconds passed since the host itself
   had the level, the host lets go of the ones still loading (they hear `on_disconnect`, the
   others `on_leave`) and everyone starts the level at once: `on_level_ready` runs, then the
   level's behaviours start.

The objects of the old level are gone with it on every machine, the ones spawned with
`Network.spawn` included: anything said about them on the way (a place, a spawn) is dropped, so
none of them turns up in the new level. Each machine spawns its player's objects again in the new
level, in the scene script's `setup` or in `on_level_ready`:

```ruby
Network.on_level_ready do |_name|
  Network.spawn("Hero", position: Scene.find!("Start").transform.position) unless Network.server?
end
```

Only the host changes the level: elsewhere `Game.load_scene` changes nothing, warns, and returns
`false`, and a level change that any other machine claims to order is ignored. One change goes on
at a time (another asked meanwhile returns `false`), a name the game has no scene for raises at
once, and `Game.cancel_scene_load` cancels nothing during a change. A machine that joins is told
the level the game plays and loads it (unless it has it already) before it sees what is spawned
there; one that joins during a change takes part in it. A machine that cannot load the level (its
file is broken) leaves the game; when that is the host, the game ends.

`Network.on_level_loading` and `Network.on_level_ready` last across levels, like
`Network.authenticate`: each call replaces the handler before it (unlike `Network.on`, whose
handlers end with the level). `Network.changing_level?` is true from the start of a change until
the level starts.

### What lasts from one level to the next

The Ruby of every machine survives a level: its global variables, its classes and modules and
what they hold. What the game wants to keep (a score, the players' lives, the round) it keeps in
Ruby objects of its own, outside the level's behaviours, which end with the level:

```ruby
# scripts/match.rb: one per machine, for the whole game
module Match
  @scores = Hash.new(0)
  class << self
    attr_reader :scores
  end
end

Network.on(:scored) { |points, from| Match.scores[from] += points }   # (in a level's setup)
```

Every machine keeps its own copy: the host decides (it can send the whole of it with
`Network.emit(:scores, Match.scores)` in `on_level_ready`, for a machine that joined late).

## Movement validation

The machine that owns an object says where it is, so a changed copy of the game could say its
hero runs ten times as fast or stands on the other side of the level. The host (a player's
machine that hosts, or a dedicated server's room) can refuse to believe it: it checks every pose
of the others' objects against how fast and how far one may move, and everyone, the host's own
scripts included, sees an object that went past that where the host believes it is. Its owner is
told and puts it back there (`Network::CORRECT`), and the violation is counted against its
machine, which is let go after `kick_after` of them. The host writes a warning to its log when it
lets a machine go (for this, for a flood, a message that is not the game's, silence, or a place
held for it that ran out), and the host's `on_leave` handlers get why as a second argument (the
others' get nil). It is off unless the game turns it on, in
game.json:

```json
"movement_validation": {
  "max_speed": 9.0,
  "teleport_distance": 4.0,
  "tolerance": 0.25,
  "kick_after": 10,
  "undo": false
}
```

or from a script on the host, at any time (every key may be left out; no limits turns it off):

```ruby
Network.validate_movement(max_speed: 9.0, teleport_distance: 4.0, kick_after: 10) if Network.host?
Network.movement_violations(peer)   # how many moves of that machine the host did not believe
```

`max_speed` is in meters a second; a move too fast goes as far as was allowed (`undo: true` puts
it back where it was). `teleport_distance` is the most one pose (twenty a second) may move an
object: a longer one is never believed. `tolerance` is the share of the limits a move may go past
and still be believed, and half a second of movement at the top speed may arrive at once, so the
network's jitter is never a violation. The poses that were already on their way when a correction
went out count once. Set the limits above anything the game does on purpose (a dash, a launch pad).
To move a player's object further on purpose (a respawn, a spawn point, a level's entry), the
host's game places it:

```ruby
Network.teleport(hero, spawn.transform.position) if Network.host?   # or the key it goes by
```

Every machine has it there at once, and its owner goes on from there. The host checks its moves
from there, so nothing counts against it. The poses its owner sent before it heard are not used
(for two seconds at most, until one comes from within 1.5 m of the spot), with movement
validation on or off, so nobody sees it go back. During a level change the place waits with the
new level's objects. Only the host places: a placing message from any
other machine is ignored, and `Network.teleport` is false on a guest. Alone, it just moves the
object. (The host's own objects are never checked.)
A machine's objects past 4096 that it has not moved for ten seconds are forgotten; while none are,
the poses of its new ones are not used. In a room on a relay the relay passes poses on and checks nothing.

## Deciding things once

Two machines must not both decide the same thing. The simple rule: each machine decides for the
objects it owns and tells the others. A coin picked up by a player is announced by that player's
machine:

```ruby
def on_trigger_enter(other)
  return unless other.get_component(NetworkSync)&.mine?

  Network.emit(:coin, name)
  collect
end

# in a manager's start
Network.on(:coin) { |coin_name, _from| Scene.find(coin_name)&.get_component(Coin)&.collect }
```

The platformer demo plays together this way: Esc > Multiplayer hosts or joins a game, and
`games/platformer/scripts/co_op.rb` is the whole of its networking.

What a player uses (a door, a lever, someone to talk to, through an `Interactor`) is the one
exception: the host decides it. A client's use is asked of the host, which checks the hero's
range and line of sight again before `on_interact` runs there (see
[Interaction](interaction.md#over-the-network)). Conversations follow it: each player has their
own, the host follows it, and its effects on `GameVariables` happen there (see
[Dialogue](dialogue.md#over-the-network)).

## Testing it

The engine's own multiplayer test runs two copies of the game at once, each with its own test
script (`tests/system_pairs/`), and two copies against the game on a dedicated server
(`tests/system_servers/`). For a quick try on one machine, open the game twice, host in one
and join `127.0.0.1` in the other.

## What it does not do

There is no matchmaking or public list of games: players are on the same network, share a room
code, or know the host's or the server's address. No relay or server is run for you: each game
runs its own. Every message passes through the host (or the relay), over TCP: a packet lost on
the way holds up the ones after it. Nothing rewinds time to judge a shot where its shooter saw the
target (lag compensation), so the shooter's machine should decide its own hits, as in "Deciding
things once": fine for co-op and casual games, not for competitive shooters. Connections are
encrypted, but who answers is checked only when the game gives its fingerprint (see "Encrypted
connections"). Players are not authenticated unless the game hosting asks for it
([Online services](online-services.md)), in a room on a relay too. A room on a relay holds no
place for a player whose connection dropped (see "Reconnecting after a drop"), and when the
player hosting a game drops, the game ends: nobody else takes over as host.
