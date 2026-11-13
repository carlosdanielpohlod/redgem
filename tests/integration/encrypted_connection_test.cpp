// Game connections over TLS: who answers is checked by its fingerprint, plaintext is refused
// where encryption is wanted, a handshake that hangs costs nobody else anything, and the relay and
// the dedicated server carry encrypted games from end to end.

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <mruby.h>
#include <mruby/string.h>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "net/connection.hpp"
#include "net/network_session.hpp"
#include "net/rooms/relay_server.hpp"
#include "net/secure/fingerprint.hpp"
#include "net/secure/server_pins.hpp"
#include "net/secure/tls_identity.hpp"
#include "net/wire.hpp"
#include "server/game_server.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::net;
using redgem::test::fixture;

namespace {

constexpr const char* kLocalhost = "127.0.0.1";
constexpr int kMaxRounds = 3000;  // of polling, a millisecond apart
constexpr std::chrono::milliseconds kRoundPause{1};
constexpr float kRoundSeconds = 0.001f;
constexpr float kPastHandshakeTimeout = kHandshakeTimeoutSeconds + 0.5f;
constexpr int kSettleRounds = 50;
constexpr std::size_t kTlsRecordHeaderBytes = 5;
constexpr std::uint8_t kTlsHandshakeRecord = 0x16;  // the first byte of a TLS ClientHello's record
constexpr std::size_t kSomeBytes = 4096;
constexpr double kMillisecondsPerSecond = 1000.0;
constexpr const char* kSecret = "a session token nobody else may read";
constexpr const char* kTalk = "talk";

bool until(const std::function<bool()>& done) {
  for (int round = 0; round < kMaxRounds; ++round) {
    if (done()) return true;
    std::this_thread::sleep_for(kRoundPause);
  }
  return false;
}

double milliseconds_since(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() * kMillisecondsPerSecond;
}

std::shared_ptr<const TlsIdentity> new_identity() {
  std::string error;
  auto identity = TlsIdentity::generate(error);
  REQUIRE_MESSAGE(identity != nullptr, error);
  return identity;
}

// Two ends of one connection on this machine: `caller` connected out (checking the other as
// `trust` says), `taker` was connected to (proving itself with `identity`, when it has one).
struct BothEnds {
  Socket listener;
  Connection caller, taker;

  BothEnds(const TlsTrust& trust, const std::shared_ptr<const TlsIdentity>& identity) {
    std::string error;
    listener = Socket::listen_on(0, error);
    REQUIRE_MESSAGE(listener.valid(), error);
    caller = Connection::calling(Socket::connect_to(kLocalhost, listener.local_port(), error), trust);
    Socket taken;
    REQUIRE(until([&] { return listener.accept(taken) == IoStatus::Ok; }));
    taker = Connection::accepted(std::move(taken), kMaxPendingBytes, identity);
  }

  void pump() {
    caller.pump();
    taker.pump();
  }

  // The first message `to` takes, pumping both ends until one comes or `to` closes.
  std::optional<nlohmann::json> heard_by(Connection& to) {
    std::optional<nlohmann::json> heard;
    until([&] {
      pump();
      heard = to.next_message();
      return heard.has_value() || to.closed();
    });
    return heard;
  }
};

nlohmann::json hello_with_secret() {
  nlohmann::json hello = wire::hello();
  hello[wire::kAuth] = {{"token", kSecret}};
  return hello;
}

// A relay that encrypts, and the sessions talking through it.
struct EncryptedMeeting {
  std::shared_ptr<const TlsIdentity> identity = new_identity();
  RelayServer relay;
  std::vector<NetworkSession*> sessions;
  std::vector<std::vector<NetworkEvent>> events;

  EncryptedMeeting() {
    std::string error;
    REQUIRE_MESSAGE(relay.start(0, error, identity), error);
  }

  TlsTrust pinned() const { return TlsTrust::pinned(identity->fingerprint()); }

  void seat(NetworkSession& session) {
    sessions.push_back(&session);
    events.emplace_back();
  }

  void round(float seconds = kRoundSeconds) {
    relay.poll(seconds);
    for (std::size_t i = 0; i < sessions.size(); ++i) {
      for (NetworkEvent& event : sessions[i]->poll(kRoundSeconds)) events[i].push_back(std::move(event));
    }
  }

  bool until(const std::function<bool()>& done) {
    return ::until([&] {
      round();
      return done();
    });
  }

  const NetworkEvent* last(std::size_t seat_index, NetworkEvent::Kind kind) const {
    for (auto event = events[seat_index].rbegin(); event != events[seat_index].rend(); ++event) {
      if (event->kind == kind) return &*event;
    }
    return nullptr;
  }
};

bool online(const NetworkSession& session) { return session.state() == NetworkSession::State::Online; }
bool offline(const NetworkSession& session) { return session.state() == NetworkSession::State::Offline; }

std::string ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  mrb_state* mrb = engine.scripts().vm().state();
  return RSTRING_PTR(mrb_obj_as_string(mrb, result.value));
}

bool online(Engine& engine) { return online(engine.network()); }
bool offline(Engine& engine) { return offline(engine.network()); }

// Players of the sandbox game (a copy of it, whose game.json a test may write) and, when started,
// a dedicated server of it keeping its saves and identity in a folder of its own.
struct EncryptedGame {
  std::filesystem::path root = std::filesystem::temp_directory_path() / "redgem-encrypted-connection-test";
  std::filesystem::path game = root / "game";
  std::filesystem::path data = root / "data";
  std::unique_ptr<server::GameServer> server;
  std::vector<std::unique_ptr<Engine>> machines;

  EncryptedGame() {
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy(fixture("games/sandbox"), game, std::filesystem::copy_options::recursive);
  }
  ~EncryptedGame() {
    machines.clear();
    server.reset();
    std::filesystem::remove_all(root);
  }

  void start_server() {
    server::ServerSettings settings;
    settings.game_dir = game;
    settings.port = 0;
    settings.data_dir = data;
    server = std::make_unique<server::GameServer>(std::move(settings));
    std::string error;
    REQUIRE_MESSAGE(server->start(error), error);
  }

  Engine& add_machine() {
    EngineOptions options;
    options.game_dir = game;
    options.fixed_step = true;
    options.save_file = root / ("save" + std::to_string(machines.size()) + ".json");
    auto created = Engine::create(options);
    if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
    machines.push_back(std::move(std::get<std::unique_ptr<Engine>>(created)));
    Engine& machine = *machines.back();
    ruby(machine, "$reasons = []; Network.on_fail { |reason| $reasons << reason }");
    return machine;
  }

  void round() {
    if (server) server->tick(config::kFixedDeltaSeconds);
    for (auto& machine : machines) machine->frame(config::kFixedDeltaSeconds);
  }

  bool until(const std::function<bool()>& done) {
    return ::until([&] {
      round();
      return done();
    });
  }

  std::string server_port() const { return std::to_string(server->port()); }
};

}  // namespace

TEST_CASE("an encrypted connection carries messages once the other side proved its pinned fingerprint") {
  const auto identity = new_identity();
  const auto started = std::chrono::steady_clock::now();
  BothEnds ends(TlsTrust::pinned(identity->fingerprint()), identity);
  ends.caller.send(hello_with_secret());
  const auto hello = ends.heard_by(ends.taker);
  const double first_message_ms = milliseconds_since(started);
  REQUIRE(hello.has_value());
  CHECK((*hello)[wire::kAuth]["token"] == kSecret);
  // (The side called says the protocol's version first, as a connection's always does.)
  ends.taker.send(wire::hello());
  ends.taker.send(wire::game_message(kTalk, "back", kHostId, kFirstGuestId));
  REQUIRE(ends.heard_by(ends.caller).has_value());
  const auto answer = ends.heard_by(ends.caller);
  REQUIRE(answer.has_value());
  CHECK((*answer)[wire::kName] == kTalk);
  CHECK_FALSE(ends.caller.handshaking());
  CHECK(ends.caller.failure().empty());
  MESSAGE("TLS over loopback: connect, handshake and first message in " << first_message_ms << " ms");
}

TEST_CASE("a hosted game's short code pins it as its whole fingerprint does") {
  const auto identity = new_identity();
  const std::string code = host_code(identity->fingerprint());
  CHECK(code.size() == kHostCodeDigits + 1);
  const auto digits = pin_digits(code);
  REQUIRE(digits.has_value());
  BothEnds ends(TlsTrust::pinned(digits.value_or("")), identity);
  ends.caller.send(wire::hello());
  CHECK(ends.heard_by(ends.taker).has_value());
}

TEST_CASE("a machine with another fingerprint is refused, and nothing of the game reaches it") {
  const auto identity = new_identity();
  const auto impostor = new_identity();
  BothEnds ends(TlsTrust::pinned(identity->fingerprint()), impostor);
  ends.caller.send(hello_with_secret());
  CHECK_FALSE(ends.heard_by(ends.taker).has_value());
  CHECK(ends.caller.closed());
  CHECK(ends.caller.failure().find("not the one this game expects") != std::string::npos);
  CHECK(ends.caller.failure().find(readable_fingerprint(impostor->fingerprint())) != std::string::npos);
  // (Whatever the impostor got, it was never the hello: it is let go without a message.)
  REQUIRE(until([&] {
    ends.pump();
    return ends.taker.closed();
  }));
}

TEST_CASE("a join to a machine that is not the one pinned fails, saying why") {
  EncryptedMeeting meeting;
  NetworkSession opener;
  meeting.seat(opener);
  std::string error;
  REQUIRE(opener.host_room(kLocalhost, meeting.relay.port(), error, TlsTrust::pinned(new_identity()->fingerprint())));
  REQUIRE(meeting.until([&] { return offline(opener); }));
  const NetworkEvent* failed = meeting.last(0, NetworkEvent::Kind::Failed);
  REQUIRE(failed != nullptr);
  CHECK(failed->name.find("not the one this game expects") != std::string::npos);
  CHECK(meeting.relay.room_count() == 0);
}

TEST_CASE("plaintext is refused where encryption is required, either way") {
  const auto identity = new_identity();
  SUBCASE("a plaintext machine calling an encrypted one says nothing to it") {
    BothEnds ends(TlsTrust::plaintext(), identity);
    ends.caller.send(hello_with_secret());
    CHECK_FALSE(ends.heard_by(ends.taker).has_value());
    CHECK(ends.taker.closed());
  }
  SUBCASE("an encrypted machine calling a plaintext one sends it nothing but a TLS handshake") {
    BothEnds ends(TlsTrust::unverified(), nullptr);
    ends.caller.send(hello_with_secret());
    CHECK_FALSE(ends.heard_by(ends.taker).has_value());
    CHECK(ends.taker.closed());
    REQUIRE(until([&] {
      ends.pump();
      return ends.caller.closed();
    }));
  }
  SUBCASE("what goes out first is a TLS record, not a message") {
    std::string error;
    Socket listener = Socket::listen_on(0, error);
    REQUIRE_MESSAGE(listener.valid(), error);
    Connection caller = Connection::calling(Socket::connect_to(kLocalhost, listener.local_port(), error),
                                            TlsTrust::pinned(identity->fingerprint()));
    caller.send(hello_with_secret());
    Socket spy;
    REQUIRE(until([&] { return listener.accept(spy) == IoStatus::Ok; }));
    std::vector<std::uint8_t> seen;
    REQUIRE(until([&] {
      caller.pump();
      spy.read(seen, kSomeBytes);
      return seen.size() >= kTlsRecordHeaderBytes;
    }));
    CHECK(seen.front() == kTlsHandshakeRecord);
    CHECK(std::string(seen.begin(), seen.end()).find(kSecret) == std::string::npos);
  }
}

TEST_CASE("a handshake that hangs is let go in time, and holds up nobody meanwhile") {
  EncryptedMeeting meeting;
  std::string error;
  // Connects, and never says a word of TLS (one byte of a record's header, and nothing more).
  Socket slow = Socket::connect_to(kLocalhost, meeting.relay.port(), error);
  REQUIRE_MESSAGE(slow.valid(), error);
  REQUIRE(until([&] { return slow.connect_status() == IoStatus::Ok; }));
  std::vector<std::uint8_t> one_byte{kTlsHandshakeRecord};
  REQUIRE(until([&] { return slow.write(one_byte) == IoStatus::Ok && one_byte.empty(); }));
  REQUIRE(meeting.until([&] { return meeting.relay.connection_count() == 1; }));

  // Someone else opens a room, encrypted, while that handshake hangs.
  NetworkSession opener;
  meeting.seat(opener);
  REQUIRE(opener.host_room(kLocalhost, meeting.relay.port(), error, meeting.pinned()));
  REQUIRE(meeting.until([&] { return online(opener); }));
  CHECK(meeting.relay.connection_count() == 2);

  meeting.round(kPastHandshakeTimeout);
  for (int round = 0; round < kSettleRounds; ++round) meeting.round();
  CHECK(meeting.relay.connection_count() == 1);
  CHECK(online(opener));
  std::vector<std::uint8_t> ignored;
  CHECK(until([&] { return slow.read(ignored, kSomeBytes) == IoStatus::Closed; }));
}

TEST_CASE("a relay carries an encrypted room from end to end, pinned by its fingerprint") {
  EncryptedMeeting meeting;
  NetworkSession opener, guest;
  meeting.seat(opener);
  meeting.seat(guest);
  std::string error;
  REQUIRE(opener.host_room(kLocalhost, meeting.relay.port(), error, meeting.pinned()));
  REQUIRE(meeting.until([&] { return online(opener); }));
  const auto started = std::chrono::steady_clock::now();
  REQUIRE(guest.join_room(kLocalhost, meeting.relay.port(), opener.room(), error, nullptr, meeting.pinned()));
  REQUIRE(meeting.until([&] { return online(guest) && opener.peers().size() == 1; }));
  const double join_ms = milliseconds_since(started);
  REQUIRE(guest.send(kTalk, kSecret, kHostId));
  REQUIRE(meeting.until([&] { return meeting.last(0, NetworkEvent::Kind::Message) != nullptr; }));
  CHECK(meeting.last(0, NetworkEvent::Kind::Message)->data == kSecret);
  REQUIRE(opener.send(kTalk, "welcome", guest.id()));
  REQUIRE(meeting.until([&] { return meeting.last(1, NetworkEvent::Kind::Message) != nullptr; }));
  CHECK(meeting.last(1, NetworkEvent::Kind::Message)->data == "welcome");
  MESSAGE("joining a room on a relay over loopback, TLS handshake included: " << join_ms << " ms");
}

TEST_CASE("a dedicated server keeps its identity, and players pin it by its fingerprint") {
  EncryptedGame game;
  game.start_server();
  const std::string fingerprint = game.server->fingerprint();
  REQUIRE(fingerprint.size() == kFingerprintDigits);
  CHECK(std::filesystem::exists(game.data / kCertificateFile));
  CHECK(std::filesystem::exists(game.data / kPrivateKeyFile));

  Engine& player = game.add_machine();
  const std::string pinned = ", fingerprint: '" + readable_fingerprint(fingerprint) + "'";
  const auto started = std::chrono::steady_clock::now();
  REQUIRE(ruby(player, "Network.join('127.0.0.1', port: " + game.server_port() + pinned + ")") == "true");
  REQUIRE(game.until([&] { return online(player); }));
  MESSAGE("joining a dedicated server over loopback, TLS handshake included: " << milliseconds_since(started) << " ms");
  CHECK(game.server->player_count() == 1);

  Engine& fooled = game.add_machine();
  const std::string wrong = ", fingerprint: '" + host_code(new_identity()->fingerprint()) + "'";
  REQUIRE(ruby(fooled, "Network.join('127.0.0.1', port: " + game.server_port() + wrong + ")") == "true");
  REQUIRE(game.until([&] { return offline(fooled); }));
  CHECK(ruby(fooled, "$reasons.first.to_s.include?('not the one this game expects')") == "true");

  // The next run of the server is the same server to its players.
  game.machines.clear();
  game.server.reset();
  game.start_server();
  CHECK(game.server->fingerprint() == fingerprint);
}

TEST_CASE("game.json's servers pin a dedicated server for joins that give no fingerprint") {
  EncryptedGame game;
  game.start_server();
  SUBCASE("its own fingerprint: in") {
    std::ofstream(game.game / "game.json") << nlohmann::json{
        {kServersKey, {{{"address", kLocalhost}, {"fingerprint", readable_fingerprint(game.server->fingerprint())}}}}};
    Engine& player = game.add_machine();
    REQUIRE(ruby(player, "Network.join('127.0.0.1', port: " + game.server_port() + ")") == "true");
    CHECK(game.until([&] { return online(player); }));
  }
  SUBCASE("another fingerprint: refused") {
    std::ofstream(game.game / "game.json") << nlohmann::json{
        {kServersKey,
         {{{"address", kLocalhost}, {"port", game.server->port()}, {"fingerprint", new_identity()->fingerprint()}}}}};
    Engine& player = game.add_machine();
    REQUIRE(ruby(player, "Network.join('127.0.0.1', port: " + game.server_port() + ")") == "true");
    REQUIRE(game.until([&] { return offline(player); }));
    CHECK(ruby(player, "$reasons.first.to_s.include?('not the one this game expects')") == "true");
  }
}

TEST_CASE("a player's hosted game shows a code its players join with, and a wrong code is refused") {
  EncryptedGame game;
  Engine& host = game.add_machine();
  REQUIRE(ruby(host, "Network.host(port: 0, visible: false)") == "true");
  const std::string code = ruby(host, "Network.host_code");
  CHECK(code.size() == kHostCodeDigits + 1);
  CHECK(ruby(host, "Network.fingerprint.gsub(':', '').start_with?(Network.host_code.gsub('-', ''))") == "true");
  const std::string port = std::to_string(host.network().port());

  Engine& friendly = game.add_machine();
  REQUIRE(ruby(friendly, "Network.join('127.0.0.1', port: " + port + ", fingerprint: '" + code + "')") == "true");
  CHECK(game.until([&] { return online(friendly); }));

  Engine& misled = game.add_machine();
  const std::string other = host_code(new_identity()->fingerprint());
  REQUIRE(ruby(misled, "Network.join('127.0.0.1', port: " + port + ", fingerprint: '" + other + "')") == "true");
  REQUIRE(game.until([&] { return offline(misled); }));
  CHECK(ruby(misled, "$reasons.first.to_s.include?('not the one this game expects')") == "true");

  // A fingerprint that is none raises, and leaves the game this machine is in alone.
  CHECK_FALSE(friendly.scripts().eval("Network.join('127.0.0.1', port: 1, fingerprint: 'not hex')", "t.rb").ok);
  CHECK_FALSE(friendly.scripts().eval("Network.host(room: true, fingerprint: '3FA2C')", "t.rb").ok);
  // A relay's or a server's identity stays: a room takes its whole fingerprint, never a short code.
  CHECK_FALSE(friendly.scripts().eval("Network.host(room: true, fingerprint: '" + code + "')", "t.rb").ok);
  CHECK_FALSE(friendly.scripts().eval("Network.join(room: 'K7QM3P', fingerprint: '" + code + "')", "t.rb").ok);
  // And a fingerprint is the relay's: a game hosted directly has none to check.
  CHECK_FALSE(friendly.scripts().eval("Network.host(fingerprint: '" + code + "')", "t.rb").ok);
  CHECK(online(friendly));
  ruby(host, "Network.leave");
  CHECK(ruby(host, "[Network.fingerprint, Network.host_code].inspect") == "[nil, nil]");
}

TEST_CASE("fingerprints and host codes are read however a person writes them") {
  const std::string digits(kFingerprintDigits, 'A');
  CHECK(pin_digits(readable_fingerprint(digits)) == digits);
  CHECK(pin_digits("3fa2c-1b09e") == std::optional<std::string>("3FA2C1B09E"));
  CHECK(pin_digits(" 3FA2C 1B09E ") == std::optional<std::string>("3FA2C1B09E"));
  CHECK_FALSE(pin_digits("3FA2C-1B09").has_value());  // neither a code nor a fingerprint
  CHECK_FALSE(pin_digits("3FA2C-1B09G").has_value());
  CHECK_FALSE(pin_digits("").has_value());
  CHECK(host_code("3FA2C1B09E" + std::string(kFingerprintDigits - kHostCodeDigits, '0')) == "3FA2C-1B09E");
  CHECK(TlsTrust::unverified().accepts(digits));
  CHECK_FALSE(TlsTrust::pinned("3FA2C1B09E").accepts(digits));
}

TEST_CASE("game.json's servers are used when they make sense, and the rest are said to be left out") {
  const std::string digits(kFingerprintDigits, 'B');
  std::vector<std::string> warnings;
  const nlohmann::json servers = {
      {{"address", "play.example.com"}, {"port", 7777}, {"fingerprint", readable_fingerprint(digits)}},
      {{"address", "relay.example.com"}, {"fingerprint", digits}},
      {{"address", "short.example.com"}, {"fingerprint", "3FA2C-1B09E"}},  // a host's code is no server's
      {{"address", "port.example.com"}, {"port", "7777"}, {"fingerprint", digits}},
      "play.example.com"};
  const std::vector<ServerPin> pins = read_server_pins(servers, warnings);
  CHECK(pins.size() == 2);
  CHECK(warnings.size() == 3);
  CHECK(pinned_fingerprint(pins, "play.example.com", 7777) == digits);
  CHECK(pinned_fingerprint(pins, "play.example.com", 7778).empty());
  CHECK(pinned_fingerprint(pins, "relay.example.com", 7778) == digits);
  CHECK(pinned_fingerprint(pins, "Play.Example.COM.", 7777) == digits);
  // A pin naming the port wins over one for every port, listed before it or not.
  const std::string other(kFingerprintDigits, 'C');
  const std::vector<ServerPin> both = {{"play.example.com", 0, other}, {"play.example.com", 7777, digits}};
  CHECK(pinned_fingerprint(both, "play.example.com", 7777) == digits);
  CHECK(pinned_fingerprint(both, "play.example.com", 9000) == other);
  CHECK(pinned_fingerprint(pins, "elsewhere.example.com", 7777).empty());
  warnings.clear();
  CHECK(read_server_pins(nlohmann::json::object(), warnings).empty());
  CHECK(warnings.size() == 1);
}

TEST_CASE("a server's identity is made once in its folder, its private key for its owner alone") {
  const std::filesystem::path folder = std::filesystem::temp_directory_path() / "redgem-tls-identity-test";
  std::filesystem::remove_all(folder);
  std::string error;
  const auto first = TlsIdentity::kept_in(folder, error);
  REQUIRE_MESSAGE(first != nullptr, error);
  const auto again = TlsIdentity::kept_in(folder, error);
  REQUIRE_MESSAGE(again != nullptr, error);
  CHECK(again->fingerprint() == first->fingerprint());
#ifndef _WIN32
  const auto others = std::filesystem::perms::group_all | std::filesystem::perms::others_all;
  CHECK((std::filesystem::status(folder / kPrivateKeyFile).permissions() & others) == std::filesystem::perms::none);
#endif
  // Half an identity is none: it is not made again over what is there.
  std::filesystem::remove(folder / kCertificateFile);
  CHECK(TlsIdentity::kept_in(folder, error) == nullptr);
  CHECK_FALSE(error.empty());
  std::filesystem::remove_all(folder);
}
