# Dedicated server

With a dedicated server nobody has to host: the game runs on a machine on the internet, with no
window and no player, and everyone joins it, phones included. A small rented Linux server (a VPS)
is enough. It needs no display and no graphics card.

The server is a program of its own, `redgem-server`. One server holds several **rooms**. Each room
is a match of its own, with the whole game running in it. Players join by the server's address and
get the first room with a free place, or a new room when every room is full. A friend can follow
them into the same room with its code (`Network.room`). A room closes when its match ends: the game
calls `Game.quit`, or everyone left. The next players get a new room, with a new game.

## Run it

```bash
./build/src/redgem-server games/platformer      # listens on port 7777
./build/src/redgem-server games/platformer --port 9000 --max-players 4 --rooms 16
```

| Option | Default | What it is |
|--------|---------|------------|
| `--port` | 7777 | The TCP port players connect to (0 picks a free one) |
| `--max-players` | 8 | Players in one room, up to 15 |
| `--rooms` | 8 | Rooms open at once, up to 64 |

It writes what happens to its output (rooms opened and closed, how many players) and stops
cleanly on Ctrl+C or `SIGTERM`: every room is closed and its players are told.

A room is light (a platformer room takes 1 to 2 MB of memory and a few percent of one core), but
every room runs the whole game, so `--rooms` is what sizes the machine.

## Run it with Docker

`Dockerfile.server` builds an image holding the server and one game, nothing else:

```bash
docker build -f Dockerfile.server --build-arg GAME=games/platformer -t my-game-server .
docker run -d --restart unless-stopped -p 7777:7777 my-game-server --rooms 4
```

`GAME` is the game's folder, relative to the repository. `docker compose up server` does the same
for the platformer (`GAME=path/to/game docker compose up server` for another). The container runs
as an unprivileged user and writes nothing.

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
open on a server, and what the game saves there lasts for the match only. Call `Game.quit` on the
server when a match is over to give its room to the next players.

The server faces the internet. It keeps every limit a hosted game and the relay have (message
sizes, how much each player may send, connections per address, spawned objects), lets go of
machines that stay silent and of players who say nothing to the game for five minutes (a game
left paused counts), and keeps out addresses that guess at room codes. Messages are not encrypted
and players are not authenticated.
