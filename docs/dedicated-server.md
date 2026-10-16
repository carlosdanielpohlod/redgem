# Dedicated server

With a dedicated server nobody has to host: the game runs on a machine on the internet, with no
window and no player, and everyone joins it, phones included. A small rented Linux server (a VPS)
is enough. It needs no display and no graphics card.

The server is a program of its own, `redgem-server`. One server holds several **rooms**. Each room
is a match of its own, with the whole game running in it. Players join by the server's address and
get the first room with a free place, or a new room when every room is full. A friend can follow
them into the same room with its code (`Network.room`). A room closes when its match ends: the game
calls `Game.quit`, or everyone left. The next players get a new room, with a new game.

Nobody runs a server for you: each game runs its own, on a machine its players can reach, as
below. The same goes for a [relay](scripting/multiplayer.md#over-the-internet-rooms).

## Run it

```bash
./build/src/redgem-server games/platformer      # listens on port 7777
./build/src/redgem-server games/platformer --port 9000 --max-players 4 --rooms 16 --data /srv/platformer
```

| Option | Default | What it is |
|--------|---------|------------|
| `--port` | 7777 | The TCP port players connect to (0 picks a free one) |
| `--max-players` | 8 | Players in one room, up to 15 |
| `--rooms` | 8 | Rooms open at once, up to 64 |
| `--threads` | The machine's cores | Threads the rooms run on, up to 64: each room has one of its own while there are no more rooms than threads |
| `--per-address` | 24 | Machines one address may connect, up to 1024: raise it when many players share one address (a campus, some mobile networks) |
| `--data` | `<game>-data` beside the game's folder | Where the game's `Save` is kept (`save.sqlite3`), for every room and every run |

It writes what happens to its output (rooms opened and closed, how many players) and stops
cleanly on Ctrl+C or `SIGTERM`: every room is closed and its players are told.

A room is light (a platformer room takes 1 to 2 MB of memory and a few percent of one core), but
every room runs the whole game, so `--rooms` is what sizes the machine. Each room runs on a thread
of its own, so the rooms use all the machine's cores, and a room loading a level (when its first
player arrives, or when its game loads another) holds up no other room. Players connect and are
placed in rooms on the server's main thread meanwhile. With more rooms open than `--threads`, the
rooms share the threads there are: a level loading in one then holds up the others on its thread
for that moment.

## Run it with Docker

`Dockerfile.server` builds an image holding the server and one game, nothing else:

```bash
docker build -f Dockerfile.server --build-arg GAME=games/platformer -t my-game-server .
docker run -d --restart unless-stopped -p 7777:7777 -v my-game-data:/data my-game-server --rooms 4
```

`GAME` is the game's folder, relative to the repository. `docker compose up server` does the same
for the platformer (`GAME=path/to/game docker compose up server` for another). The container runs
as an unprivileged user and writes only to `/data`, where the game's saves are kept (a volume, so
that they outlive the container).

## Put it on the internet

1. Rent any small Linux server with a public address, and install Docker on it (or build RedGem
   there, see [Install](install.md)).
2. Copy the repository and your game there, then build and run the image as above.
3. Open the port: allow TCP 7777 in the server's firewall and in the provider's control panel.
4. Point the game at it. In the platformer demo, set `SERVER` in `scripts/main_scene.rb`; in a game
   made from the platformer template, set `SERVER` in `scripts/online.rb`:

```ruby
SERVER = "play.example.com".freeze      # or "203.0.113.7", or "play.example.com:9000"
```

The game's menus then offer **Play online**. In your own game, it is one line:

```ruby
Network.join("play.example.com")                  # any room with a free place
Network.join("play.example.com", room: "K7QM3P")  # the room a friend is in
```

## Make the game work on a server

The server runs the same scripts as the players, with `Network.server?` true. It has no player of
its own, so whatever a script does for "my player" is skipped there:

```ruby
Network.on_connect do
  spawn_my_hero unless Network.server?                # the server plays nobody
  Network.players.each { |peer| show_hero_of(peer) }  # the others who play, never the server
end
```

`Network.host?` is true on the server: it hosts the game and owns the level's own objects, so
what the host decides (scores, a door opening, the end of a match) is decided there. Menus never
open on a server. Call `Game.quit` on the server when a match is over to give its room to the next
players.

`Save` on the server is the server's own: a SQLite database in its data folder (`--data`),
shared by all its rooms and kept across restarts. Each key is a row of its own, written as soon as
it changes, in a transaction (a crash keeps every change made before it, never half of one), and
only what a script asks for is read into memory. Rooms write one at a time. Arrays and hashes
saved there may hold any number of things (only their nesting is limited, to 16 levels). Keep
each player's data under their identity, as
[Online services](scripting/online-services.md#saving-on-a-server) shows; with many players,
a key per player (`Save["player:#{identity}"]`) reads and writes one player instead of all of
them.

A server of version 0.2.0 kept `save.json` in the data folder instead. Whenever the server starts
with a `save.json` there (an older server's, or one written by hand, see
[Online services](scripting/online-services.md#saving-on-a-server)), it moves the file's values
into the database, replacing those of the same names, and keeps the file beside it as
`save.json.migrated`. A server that cannot move it does not start, rather than lose them. A copy of the database for a backup is taken with `sqlite3 save.sqlite3
".backup copy.sqlite3"`, or by copying the folder while the server is stopped.

The server faces the internet. It keeps every limit a hosted game and the relay have (message
sizes, how much each player may send, connections per address, spawned objects), lets go of
machines that stay silent and of players who say nothing to the game for five minutes (a game
left paused counts), and keeps out addresses that guess at room codes. Messages are not encrypted.
Players are let in without asking unless the game decides who plays: see
[Online services](scripting/online-services.md#letting-players-in).
