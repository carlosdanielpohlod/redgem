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
