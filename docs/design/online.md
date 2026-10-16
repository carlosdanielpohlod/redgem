# Design: online games

Status: implemented (see [Online services](../scripting/online-services.md) for how a game uses it,
and "As built" at the end for where the implementation differs). What RedGem needs so a game can
be played online with accounts and saved data, built the way established engines do it, and
generic: the engine provides the pieces, the game chooses its services.

## What other engines do

| Need | Common practice |
|------|-----------------|
| Talking to web services | A generic, asynchronous HTTP client with HTTPS in the scripting API (Godot `HTTPRequest`, Unity `UnityWebRequest`, Unreal's HTTP module), available on every platform. |
| Accounts and login | None ships its own account system. Games log in through a service (Unity Authentication, PlayFab, Nakama, Firebase, Supabase, Epic Online Services) or the developer's own backend, over HTTPS, and get a session token. |
| Letting a player into a server | *Connection approval*: the client sends a payload when it connects (usually that token), the server decides whether to accept it (Unity Netcode `ConnectionApprovalCallback`, Godot `SceneMultiplayer.auth_callback`). |
| Saved data on a server | The ordinary save API writes to the server's disk (Godot `user://`, Unity `persistentDataPath`). Anything bigger goes to a database behind a backend or service, reached over HTTP. |
| Databases | Never opened by the game itself. A backend owns the database; the game server calls the backend. |
| Encrypting the game connection | Offered by the transport (Godot ENet with DTLS, Unity Transport with TLS), usually optional. |

## The design

```
 phone / PC                          dedicated server                 services
 ┌──────────────┐  1 log in (HTTPS)                                  ┌──────────────┐
 │  game        │ ─────────────────────────────────────────────────▶ │ auth service │
 │  (Ruby)      │ ◀───────────────── session token ───────────────── │ or backend   │
 │              │                   ┌───────────────────┐            │              │
 │              │ 2 join + token ─▶ │ redgem-server     │ 3 verify ─▶│              │
 │              │ ◀── approved ──── │ (Ruby: the game)  │ ◀──────────│              │
 │              │ 4 play ◀───────▶  │                   │ 5 save ──▶ │ database     │
 └──────────────┘                   └───────────────────┘   (HTTPS)  └──────────────┘
```

The password never travels over the game connection: it goes from the device to the service
over HTTPS. The game connection carries only the token, then gameplay.

### 1. `HTTP`: a web client for every game

Asynchronous (the frame never waits), HTTPS by default, on Linux, Windows, macOS, Android and
the dedicated server. The answer arrives in a block on the game's own thread, like every other
callback.

```ruby
HTTP.post("https://api.mygame.com/scores", json: { player: id, points: 120 },
          headers: { "Authorization" => "Bearer #{token}" }) do |response|
  next Debug.warn("scores: #{response.error}") unless response.ok?

  show_ranking(response.json[:top])
end

HTTP.get("https://api.mygame.com/news") { |response| @news = response.json }
```

- `HTTP.get/post/put/patch/delete(url, json:, body:, headers:, timeout:)`; the block gets a
  `response` with `ok?`, `status`, `headers`, `body`, `json` and `error`.
- Limits: answer size (8 MB), requests in flight per game (16), a timeout (15 s default). URLs
  must be `https://`, or `http://` to a local or private address (a backend on the same machine
  or network).
- Certificates are checked against the system's store, or a bundled one on Android.
- Built on the engine's sockets and mbedTLS (Apache 2.0), on a worker thread.

### 2. Connection approval

The joining side sends a payload; the server's game decides. Without a handler on the server
everyone is let in, as today.

```ruby
# on the device, after logging in to the service
Network.join("play.mygame.com", auth: { token: session_token })

# on the server
Network.authenticate do |peer, auth, decide|
  HTTP.get("https://auth.mygame.com/user",
           headers: { "Authorization" => "Bearer #{auth[:token]}" }) do |response|
    next decide.deny("not logged in") unless response.ok?

    decide.accept(identity: response.json[:id], name: response.json[:name])
  end
end

Network.identity(peer)   # => "user-4821" once accepted, on the server
Network.on_join { |peer| greet(Network.identity(peer)) }
```

- `on_join` fires only once a player is accepted; a denied player gets `on_fail` with the reason.
- Approval has a deadline (10 s): a server that never decides denies.
- Works the same with a player hosting, but is meant for dedicated servers.

### 3. Saving on a server

`Save` keeps working everywhere. On a dedicated server it is written to the server's data
folder (`--data <dir>`, default next to the game), so it survives restarts. Data per player is
kept under the player's identity:

```ruby
Network.on_leave do |peer|
  Save.update("players", {}) do |players|      # no other room's change gets lost in between
    players.merge(Network.identity(peer) => { level: @level, coins: @coins })
  end
end
```

One row per key in a database (SQLite), each written as it changes. For queries such as a
ranking, the game sends its data to a backend with `HTTP`.

### 4. Guides, not built-in services

The manual gets two worked examples, both with the platformer:

- **A hosted service** (Supabase, which has a free tier): log in, keep each player's progress,
  a ranking.
- **Your own backend** in a few lines of Sinatra with SQLite, for those who want it all on their
  own machine.

No service is built into the engine and none is required.

## Later

- **Encrypted game connections** (TLS over the same mbedTLS), optional, for games that carry
  anything sensitive over the game connection.

## Security

- Passwords never reach the game server or the game connection.
- Every HTTP answer and every `auth` payload is untrusted input: size limits, types checked,
  timeouts.
- A server's game decides who plays; tokens are checked by the service that issued them.
- Scripts still cannot read or write files on the player's device; only `HTTP` reaches out.

## As built

- **HTTP** lives in `src/net/http_*` (Mbed TLS 3.6, one thread per request), its Ruby side in
  `ruby/engine/http.rb`. `http://` is allowed when the address a name leads to is this machine's
  or a private network's, checked once connected (nothing is sent otherwise). Redirects are not
  followed. On Android the certificates are not bundled: the engine reads the system's own
  (`/apex/com.android.conscrypt/cacerts`, then `/system/etc/security/cacerts`), which every app
  can read and which stays up to date. A level loaded while requests are on their way drops their
  blocks (the requests still go out); a game that ends gives them up.
- **Connection approval**: the name is `Network.player_name(peer)`; `on_fail` gets the reason. The
  hello carries `auth` under a new key, and the host answers with two new kinds of message ("the
  game is deciding", "refused, and why"); no version field was needed. A client of 0.2.0 joins as
  before (it carries nothing); against a host that decides, it ignores "deciding", so it gives up
  after its own 5 seconds if the game takes longer, and a refusal reaches it as a plain failure.
  In a room on a relay the game of whoever opened it decides, and the relay decides nothing: the
  opener tells it whether its game decides (a new kind of message), the relay holds the newcomer
  with the id it would have and hands the opener what its hello carried, and passes the decision
  back (two more).
- **Saving on a server**: the data folder defaults to `<game>-data` beside the game's folder. It
  holds a SQLite database (`save.sqlite3`, `src/server/save_database.*`): one row per key, each
  change its own transaction, used by the rooms' threads one call at a time. Saved arrays and hashes are no longer limited to 4096
  things (that limit is a message's); only their nesting is.
  `Save` now keeps arrays, hashes and `Vector3` as well (the example above needs a hash);
  hashes come back with string keys. A server's secrets (a backend's key) go in its own save.
- **Guides**: the manual's Online services page; the platformer logs in when `ACCOUNTS` is set
  (`scripts/accounts.rb`, `scripts/server_gate.rb`) and joins as a guest otherwise. The backend's
  essential code is shown in the guide; no example app is kept in the repository.

## On the wire

What the machines of a game (and the relay and the dedicated server between them) say to each
other, since protocol version 1. The code is in `src/net/wire*` and `src/net/pose_*`.

- **Frames.** Every message travels as its length (4 bytes, big-endian) and then its bytes, at
  most 1 MiB. A message is a MessagePack map whose keys are single letters (`k` its kind, `f` from,
  `t` to, `n` a game message's name, `d` its data, and a few more for joining). A game's data is
  MessagePack's nil, booleans, integers, floats, strings (UTF-8), arrays and maps; a `Vector3` is
  `{"$v": [x, y, z]}`. A message to everyone does not say so (no `t`).
- **Version.** Every connection starts with `v`, the protocol's version: in the newcomer's hello,
  and in the welcome or refusal it gets back (and in a game's answer to a LAN search). A host, a
  relay or a server's lobby refuses a hello of another version with "this game runs protocol 1,
  the other side 2" and logs it; the machine turned away says the same, from its side, in its
  `on_fail` and its log. A game of another version found on the local network is not listed (the
  log says why, once). Nothing is kept for older versions; what the first words look like (a frame
  holding a map with `k`, `v` and a reason `y`) is the one thing every version keeps the same.
- **Limits**, the same as with JSON: a message's data nests at most 16 deep, an array or a map
  holds at most 4096 things, names and hash keys are at most 64 bytes, a join's `auth` at most 4096
  bytes written out, a refusal's reason 256. What arrives is untrusted: depth and counts are
  checked while it is read (arrays nested deeper than a message may be are stopped before they are
  built), text must be UTF-8, and bytes, infinity and NaN are no game data. The allowances of a
  relay and a server (bytes a second, messages a poll) are unchanged; since messages are smaller,
  they now hold more of them.
- **Poses** (NetworkSync) are a kind of message of their own, `o`, whose `d` is bytes:

  | part | size | what |
  |---|---|---|
  | flags | 1 byte | which parts follow: where it is, how far it moved, its rotation |
  | key size, key | 1 byte + up to 128 | the object's key (UTF-8) |
  | where it is | 12 bytes | x, y, z as 32-bit floats |
  | or how far it moved | 6 bytes | x, y, z as 16-bit steps of 1/1024 m since the last pose sent |
  | rotation | 4 bytes | "smallest three": the largest part's index (2 bits), the others in 10 bits each |

  A part that did not change since the last pose sent is left out; an object standing still sends
  its key alone, which keeps the others' interpolation on time. Positions are floats, as the engine
  keeps them, so a whole pose is exactly the sender's however far from the middle of the world,
  and a move is counted from the last pose sent, so its precision (half a millimeter) does not
  depend on where the object is either; a power of two times a whole number is exact in a float,
  so the moves add up to the same place on every machine. A rotation comes back within 0.3
  degrees. Every object's pose is sent whole first, again every 40 sends (two seconds), and as
  soon as someone joins; a machine ignores moves of an object until it has its whole pose. The
  receiving side hands whole poses to the scripts as the `$sync` message, as before.
- **Debugging.** With `REDGEM_LOG_MESSAGES=1` in its environment, a game, relay or server writes
  every message it sends and receives to its log as JSON text.

A typical pose (a player running and turning, key `hero-2`) took about 110 to 140 bytes as JSON;
it now takes 34 (30 when it only moves, 24 standing still, 40 when whole). The load test's game
message (a shot: where, how hard, when) went from 143 bytes to 66, framing included.

### Measured

`tests/load` runs bots that join one room of the platformer on `redgem-server` and play as players
do: each runs in circles, its pose sent 20 times a second, and fires a shot (a game message) twice
a second; the server's game moves its copy of each player's hero. It measures the bytes each
player sends and receives, the server's processor time per frame (its process, lobby included, at
60 frames a second) and how long a shot takes from one player to the others (through the server,
which passes messages on once a frame). The CI runs it for 5 seconds with 4, 8 and 15 players
(`ctest -R load.short`, a step of its own in the Linux workflow); these are from GitHub's Linux
runner, one run each, so the processor times are rough (the clock they are read from counts in
10 ms steps).

| players | | sent B/s per player | received B/s per player | bytes per pose | bytes per message | server CPU ms per frame | latency ms p50 / p95 / max |
|---|---|---|---|---|---|---|---|
| 4 | JSON | 3078 | 9234 | 141.0 | 143.2 | 0.30 | 8.6 / 16.2 / 17.2 |
| 4 | MessagePack | 804 | 2423 | 34.1 | 66.0 | 0.47 | 9.7 / 17.2 / 17.3 |
| 8 | JSON | 3065 | 21502 | 141.1 | 142.7 | 0.53 | 8.8 / 16.3 / 17.7 |
| 8 | MessagePack | 802 | 5623 | 34.1 | 66.0 | 0.67 | 9.7 / 16.4 / 17.5 |
| 15 | JSON | 3112 | 43726 | 142.8 | 143.1 | 1.23 | 9.0 / 16.7 / 18.0 |
| 15 | MessagePack | 814 | 11402 | 34.6 | 66.0 | 1.23 | 9.8 / 16.7 / 18.0 |

Each player sends and receives about a quarter of what it did. Latency is the server's frame (a
message waits for the room's next one) and did not change; neither, within what these numbers can
tell, did the server's processor time.
