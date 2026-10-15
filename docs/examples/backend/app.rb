# A game's own backend, in a few lines: accounts with passwords, sessions, each player's coins
# and a ranking, in an SQLite file. The manual's Online services page shows how a game (and the
# platformer demo) talks to it.
#
#   bundle install
#   SERVER_KEY=some-long-secret bundle exec ruby app.rb      # http://127.0.0.1:4567
#
# POST /signup   {name, password}      -> 201 {id, name}
# POST /login    {name, password}      -> 200 {token, id, name}
# GET  /me       Bearer <token>        -> 200 {id, name}
# POST /progress {player, coins}       -> 204, only with the game server's X-Server-Key
# GET  /ranking                        -> 200 [{name, coins}, ...]
require "json"
require "openssl"
require "securerandom"
require "sinatra"
require "sqlite3"

DATABASE = SQLite3::Database.new(ENV.fetch("DATABASE", "accounts.sqlite3"), results_as_hash: true)
DATABASE.execute_batch(<<~SQL)
  CREATE TABLE IF NOT EXISTS players (id INTEGER PRIMARY KEY, name TEXT UNIQUE NOT NULL,
                                      salt TEXT NOT NULL, digest TEXT NOT NULL, coins INTEGER NOT NULL DEFAULT 0);
  CREATE TABLE IF NOT EXISTS sessions (token TEXT PRIMARY KEY, player_id INTEGER NOT NULL REFERENCES players(id));
SQL
SERVER_KEY = ENV.fetch("SERVER_KEY")   # what the game server proves itself with
HASH_ROUNDS = 100_000
RANKING_SIZE = 10

helpers do
  def body_json = JSON.parse(request.body.read)
  def answer(status_code, value) = halt(status_code, { "Content-Type" => "application/json" }, value.to_json)
  def digest(password, salt) = OpenSSL::KDF.pbkdf2_hmac(password, salt: salt, iterations: HASH_ROUNDS,
                                                                   length: 32, hash: "sha256").unpack1("H*")

  def player_named(name) = DATABASE.get_first_row("SELECT * FROM players WHERE name = ?", [name])

  def logged_in_player
    # (Headers arrive as bytes: as such SQLite would compare them to no text.)
    token = request.env["HTTP_AUTHORIZATION"].to_s.delete_prefix("Bearer ").dup.force_encoding(Encoding::UTF_8)
    DATABASE.get_first_row("SELECT players.* FROM sessions JOIN players ON players.id = player_id WHERE token = ?",
                           [token]) or answer(401, error: "not logged in")
  end
end

post "/signup" do
  name, password = body_json.values_at("name", "password")
  answer(400, error: "a name and a password, please") if name.to_s.empty? || password.to_s.empty?
  answer(409, error: "that name is taken") if player_named(name)
  salt = SecureRandom.hex(16)
  DATABASE.execute("INSERT INTO players (name, salt, digest) VALUES (?, ?, ?)", [name, salt, digest(password, salt)])
  answer(201, id: DATABASE.last_insert_row_id, name: name)
end

post "/login" do
  name, password = body_json.values_at("name", "password")
  player = player_named(name.to_s)
  correct = player && Rack::Utils.secure_compare(player["digest"], digest(password.to_s, player["salt"]))
  answer(401, error: "wrong name or password") unless correct
  token = SecureRandom.hex(32)
  DATABASE.execute("INSERT INTO sessions (token, player_id) VALUES (?, ?)", [token, player["id"]])
  answer(200, token: token, id: player["id"], name: player["name"])
end

get "/me" do
  player = logged_in_player
  answer(200, id: player["id"], name: player["name"])
end

post "/progress" do
  answer(403, error: "only the game server") unless Rack::Utils.secure_compare(request.env["HTTP_X_SERVER_KEY"].to_s,
                                                                              SERVER_KEY)
  player, coins = body_json.values_at("player", "coins")
  DATABASE.execute("UPDATE players SET coins = coins + ? WHERE id = ?", [coins.to_i, player.to_i])
  halt 204
end

get "/ranking" do
  rows = DATABASE.execute("SELECT name, coins FROM players ORDER BY coins DESC LIMIT ?", [RANKING_SIZE])
  answer(200, rows.map { |row| { name: row["name"], coins: row["coins"] } })
end
