# Multiplayer

Games can be played together over a local network or the internet. One machine **hosts** the game
and the others **join** it; every machine has an id (the host is 1, then 2, 3... as players arrive).
Up to 16 machines play together.

There are three ways for the others to get to the host, from the easiest:

| Where the players are | How they join | What it takes |
|-----------------------|---------------|---------------|
| On the same network (same Wi-Fi) | The game is found by itself | Nothing |
| Anywhere | A room code like `K7QM3P` | A relay running somewhere on the internet |
| Anywhere | The host's address | The host's router forwarding a port |

## Host and join

```ruby
Network.host                           # on one machine (port 7777; port: 9000 picks another)
Network.join("192.168.0.12")           # on the others: the host's address

Network.on_connect { start_round }     # this machine is in the game
Network.on_fail { show("Could not join") }
Network.on_join { |peer| ... }         # someone else arrived
Network.on_leave { |peer| ... }        # someone else left
Network.on_disconnect { ... }          # the game is gone: the host left or the connection dropped
```

`Network.online?`, `Network.host?`, `Network.id` and `Network.peers` (everyone else's ids) tell
where things stand. `Network.leave` leaves the game; when the host leaves, it ends for everyone.

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

The relay comes with the engine. On any machine the players can reach (a small rented server is
enough, it only passes messages on):

```bash
./build/src/redgem relay               # listens on port 7778; --port picks another
docker compose up relay                # or with Docker
```

The relay expects strangers: one address can only open a few rooms and hold a few connections,
guessing room codes gets it kept out for a while, and a machine that goes silent is let go.

## By the host's address

Without a relay, players anywhere can still join the host's public address
(`Network.join("203.0.113.7")`) if the host's router forwards the game's port (7777, TCP) to the
host's machine.

## Messages

Machines talk in named messages:

```ruby
Network.on(:hit) { |data, from| @health -= data[:damage] }

Network.emit(:hit, { damage: 3, at: transform.position })   # to everyone else
Network.emit(:hit, { damage: 3 }, to: 2)                    # to one machine
Network.emit(:score, 120, to: :host)
```

A message carries `nil`, true/false, numbers, texts, `Vector3`, and arrays and hashes of those
(hash keys arrive as symbols). Messages arrive whole and in the order they were sent. Handlers last
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
machine can only spawn objects of its own.

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
script (`tests/system_pairs/`). For a quick try on one machine, open the game twice, host in one
and join `127.0.0.1` in the other.

## What it does not do

There is no matchmaking or public list of games: players are on the same network, share a room
code, or know the host's address. No relay is run for you. Every message passes through the host
(or the relay), so there is no prediction or lag compensation: fine for co-op and casual
games, not for competitive shooters. Messages are not encrypted, and players are not
authenticated: play with people you trust.
