# Multiplayer

Games can be played together over a local network or the internet. One machine **hosts** the game
and the others **join** it; every machine has an id (the host is 1, then 2, 3... as players arrive).
Up to 16 machines play together.

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
Network.on_leave { |peer| ... }        # someone else left
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
docker compose up relay                # or with Docker
```

One address may hold 24 machines and 8 open rooms on a relay. When many players share one
address (a campus, some mobile networks), raise them: `--per-address <number>` (up to 1024) and
`--rooms-per-address <number>` (up to 256).

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

The owner sends where its object is 20 times a second, in a few bytes: only what changed since the
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

Loading a level happens on one machine: its spawned objects go away everywhere, and it is shown
the others' again. To change level together, send a message and load it on every machine.

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
things once": fine for co-op and casual games, not for competitive shooters. Messages are not
encrypted. Players are not authenticated
unless the game hosting asks for it ([Online services](online-services.md)), in a room on a relay
too.
