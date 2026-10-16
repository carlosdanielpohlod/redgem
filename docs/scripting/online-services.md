# Online services

What a game played online needs besides the game itself: talking to web services (`HTTP`),
letting in only the players who logged in (connection approval) and keeping their progress
(`Save` on a [dedicated server](../dedicated-server.md), or a database behind a web service).
No service is built into the engine and none is required: below are two ways to do it, a hosted
service (Supabase) and a backend of your own.

The player's password only ever goes from their device to the service, over HTTPS. The game
connection carries the token the service gave, then gameplay.

## HTTP

```ruby
HTTP.get("https://api.example.com/news") { |response| @news = response.json if response.ok? }

HTTP.post("https://api.example.com/scores", json: { player: id, points: 120 },
          headers: { "Authorization" => "Bearer #{token}" }) do |response|
  next Debug.warn("scores: #{response.error || response.status}") unless response.ok?

  show_ranking(response.json[:top])
end
```

- `HTTP.get`, `post`, `put`, `patch`, `delete(url, json:, body:, headers:, timeout:)`. `json:` sends
  a hash or an array as JSON (with its `Content-Type`); `body:` sends a text as it is.
- The block gets the `response` a moment later, on the game's own thread: the frame never waits.
  `ok?` (a 2xx status), `status` (0 when no answer came), `headers` (names in lower case), `body`,
  `json` (nil when the body is not JSON) and `error` (why no answer came).
- `response.json` gives hash keys as symbols when your scripts use that symbol, as strings
  otherwise (as messages do).
- Addresses are `https://`, checked against the certificates the system trusts, or `http://` to
  this machine or the local network only (a backend you run beside the game).
- An answer is at most 8 MB; 16 requests may be on their way at once; each gives up after 15
  seconds unless `timeout:` says otherwise (at most 120). Redirects are not followed.
- When a level is loaded, the blocks of requests still on their way are not called (the requests
  still go out). When the game ends (or Play is stopped in the editor) they are given up.

## Letting players in

The joining machine sends a hash with its join, usually the token it got when logging in. The
game that hosts (on a dedicated server, the server's game) decides:

```ruby
# on the device, after logging in
Network.join("play.example.com", auth: { token: session_token })

# on the server
Network.authenticate do |peer, auth, decide|
  HTTP.get("https://auth.example.com/user",
           headers: { "Authorization" => "Bearer #{auth[:token]}" }) do |response|
    next decide.deny("not logged in") unless response.ok?

    decide.accept(identity: response.json[:id], name: response.json[:name])
  end
end

Network.on_join { |peer| greet(Network.identity(peer), Network.player_name(peer)) }
```

- The block gets the newcomer's id, what its join carried (a hash, empty when nothing) and the
  decision to make: `decide.accept(identity:, name:)` or `decide.deny(reason)`, now or later.
- Until then the newcomer waits: `on_join` fires once it is accepted. One not decided on within 10
  seconds is turned away. A player turned away gets `on_fail { |reason| }` with the reason.
- `Network.identity(peer)` and `Network.player_name(peer)` say who a player is, on the machine
  that accepted it, until after its `on_leave`.
- Without `authenticate` everyone is let in, as before. `Network.authenticate` with no block turns
  it off. The handler stays when a level is loaded.
- What a join carries is at most 4 KB once written as JSON. It is untrusted: check it with the
  service that issued it.
- It works with a player hosting too, and in a room on a relay, where the game of whoever opened the
  room decides: the relay decides nothing, it passes each join on and the decision back (a relay
  older than 0.3.0 lets everyone in).

## Saving on a server

`Save` on a dedicated server is written to the server's data folder (`--data`, see
[Dedicated server](../dedicated-server.md)), shared by all its rooms, and survives restarts.
`Save` keeps numbers, texts, true/false, `Vector3`, and arrays and hashes of those (hashes come
back with string keys), so each player's data can be kept under their identity:

```ruby
Network.on_leave do |peer|
  players = Save["players"] || {}
  players[Network.identity(peer)] = { "level" => @level, "coins" => @coins }
  Save["players"] = players
end
```

The file is rewritten whole on every change (never half-written): right for hundreds or a few
thousand players. Past that, or for queries such as a ranking, send the data to a backend with
`HTTP`.

**Secrets of the server** (the key a backend lets only the server write with) go in the server's
own save before it starts, in `<data folder>/save.json`: `{ "backend_key": "..." }`. The server's
game reads them with `Save["backend_key"]`; players' machines never see a server's saves.

## With a hosted service: Supabase

[Supabase](https://supabase.com) has a free tier with accounts and a database reached over HTTPS.

1. Create a project. Its settings give the project's address (`https://<ref>.supabase.co`), the
   public **anon** key (for the game) and the secret **service_role** key (for the server only).
2. In the SQL editor, create a table for each player's progress that anyone may read:

```sql
create table progress (player_id uuid primary key references auth.users, name text, coins int not null default 0);
alter table progress enable row level security;
create policy "anyone reads the ranking" on progress for select using (true);
```

On the device (`SUPABASE` is the project's address, `ANON_KEY` its anon key):

```ruby
# Sign up once (Supabase may ask the player to confirm their email first)
HTTP.post("#{SUPABASE}/auth/v1/signup", json: { email: email, password: password },
          headers: { "apikey" => ANON_KEY }) { |response| ... }

# Log in: the answer holds the session token
HTTP.post("#{SUPABASE}/auth/v1/token?grant_type=password", json: { email: email, password: password },
          headers: { "apikey" => ANON_KEY }) do |response|
  next show_error("could not log in (#{response.error || response.status})") unless response.ok?

  Network.join(SERVER, auth: { token: response.json[:access_token] })
end

# The ranking
HTTP.get("#{SUPABASE}/rest/v1/progress?select=name,coins&order=coins.desc&limit=10",
         headers: { "apikey" => ANON_KEY }) { |response| @ranking = response.json if response.ok? }
```

On the server, the token is checked with Supabase, and each player's coins (kept in the
server's save, as above) are written with the service_role key, kept in the server's save too:

```ruby
Network.authenticate do |_peer, auth, decide|
  HTTP.get("#{SUPABASE}/auth/v1/user",
           headers: { "apikey" => ANON_KEY, "Authorization" => "Bearer #{auth[:token]}" }) do |response|
    next decide.deny("not logged in") unless response.ok?

    decide.accept(identity: response.json[:id], name: response.json[:email])
  end
end

Network.on_leave do |peer|
  key = Save["supabase_service_key"]
  coins = Save["players"][Network.identity(peer)]["coins"]
  HTTP.post("#{SUPABASE}/rest/v1/progress",
            json: { player_id: Network.identity(peer), name: Network.player_name(peer), coins: coins },
            headers: { "apikey" => key, "Authorization" => "Bearer #{key}",
                       "Prefer" => "resolution=merge-duplicates" }) { |response| ... }
end
```

## With a backend of your own

A backend of your own needs only a few calls. The essentials, in Ruby with Sinatra and SQLite (a
`players` table with `id`, `name`, `salt`, `digest` and `coins`, and a `sessions` table that ties
tokens to players; signing up inserts a player with a random salt and the digest of its password):

```ruby
DB = SQLite3::Database.new("accounts.sqlite3", results_as_hash: true)

def digest(password, salt) = OpenSSL::KDF.pbkdf2_hmac(password, salt: salt, iterations: 100_000,
                                                                 length: 32, hash: "sha256").unpack1("H*")

post "/login" do
  name, password = JSON.parse(request.body.read).values_at("name", "password")
  player = DB.get_first_row("SELECT * FROM players WHERE name = ?", [name.to_s])
  correct = player && Rack::Utils.secure_compare(player["digest"], digest(password.to_s, player["salt"]))
  halt 401, { error: "wrong name or password" }.to_json unless correct
  token = SecureRandom.hex(32)
  DB.execute("INSERT INTO sessions (token, player_id) VALUES (?, ?)", [token, player["id"]])
  { token: token, id: player["id"], name: player["name"] }.to_json
end

get "/me" do   # who a token is: what the game server asks
  token = request.env["HTTP_AUTHORIZATION"].to_s.delete_prefix("Bearer ").dup.force_encoding("UTF-8")
  player = DB.get_first_row("SELECT players.* FROM sessions JOIN players ON players.id = player_id " \
                            "WHERE token = ?", [token])
  halt 401, { error: "not logged in" }.to_json unless player
  { id: player["id"], name: player["name"] }.to_json
end

post "/progress" do   # only the game server, which holds the key
  halt 403 unless Rack::Utils.secure_compare(request.env["HTTP_X_SERVER_KEY"].to_s, ENV.fetch("SERVER_KEY"))
  player, coins = JSON.parse(request.body.read).values_at("player", "coins")
  DB.execute("UPDATE players SET coins = coins + ? WHERE id = ?", [coins.to_i, player.to_i])
  halt 204
end
```

| Call | Sends | Answers |
|------|-------|---------|
| `POST /login` | `{name, password}` | 200 `{token, id, name}`, 401 `{error}` |
| `GET /me` | `Authorization: Bearer <token>` | 200 `{id, name}`, 401 |
| `POST /progress` | `{player, coins}` and `X-Server-Key` | 204, 403 without the key |

Sinatra listens on this machine only (`-o 0.0.0.0` lets the local network in). On the internet,
put it behind HTTPS (a reverse proxy in front of it): games reach other machines over `https://`
only.

### The platformer with it

The platformer demo logs in when its `ACCOUNTS` constant is set (`scripts/main_scene.rb`), next to
`SERVER`:

```ruby
SERVER = "127.0.0.1".freeze
ACCOUNTS = "http://127.0.0.1:4567".freeze
```

The multiplayer menu then offers **Log in** (a name, then a password) before **Play online**,
which joins with the token. The server checks it with `GET /me`, lets the player in as
`user-<id>`, and when the player leaves keeps their coins in its save and, given the backend's
key, in the backend too:

```bash
mkdir -p /srv/platformer-data      # once: the backend's key, in the server's own save
echo '{ "backend_key": "some-long-secret" }' > /srv/platformer-data/save.json
redgem-server games/platformer --data /srv/platformer-data
```

With `ACCOUNTS` empty (as it ships), players join as guests, each under an id their machine keeps,
and the server keeps their coins all the same. See `scripts/accounts.rb` and
`scripts/server_gate.rb`.
