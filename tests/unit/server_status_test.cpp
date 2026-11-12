// A dedicated server's status: its frame times and traffic, the JSON and log line it makes of them,
// and what its status port answers to what a connection says.

#include <doctest/doctest.h>

#include <csignal>
#include <string>

#include "net/traffic.hpp"
#include "server/frame_window.hpp"
#include "server/server_status.hpp"
#include "server/status_request.hpp"
#include "server/status_signal.hpp"
#include "server/traffic_rate.hpp"

using namespace redgem::server;

namespace {

constexpr double kShortFrame = 0.002;
constexpr double kLongFrame = 0.010;
constexpr double kTick = 1.0 / 60.0;
constexpr double kStart = 100.0;
constexpr std::uint64_t kBytesInASecond = 500;
constexpr std::uint64_t kBytesOutASecond = 2000;
constexpr std::size_t kFirstRoomPlayers = 3;
constexpr std::size_t kSecondRoomPlayers = 2;
constexpr int kMaxRooms = 8;
constexpr int kMaxPlayers = 4;
constexpr double kUptime = 12.5;
constexpr float kNoTime = 0.0f;
constexpr double kMilliseconds = 1000.0;  // in a second
constexpr double kTolerance = 1e-3;
constexpr std::size_t kThreads = 2;
constexpr int kMaxThreads = 4;
constexpr std::size_t kRooms = 2;
constexpr const char* kGet = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
constexpr const char* kDocument = "{\"a\":1}";

// Two rooms: one with short frames, the other with one long frame among them.
ServerStatus two_rooms() {
  ServerStatus status;
  status.engine_version = "1.2.3";
  status.uptime_seconds = kUptime;
  status.max_rooms = kMaxRooms;
  status.max_players = kMaxPlayers;
  status.tick_seconds = kTick;
  status.threads = kThreads;
  status.max_threads = kMaxThreads;
  status.rooms.push_back({kFirstRoomPlayers, {2, 2 * kShortFrame, kShortFrame}});
  status.rooms.push_back({kSecondRoomPlayers, {2, kShortFrame + kLongFrame, kLongFrame}});
  status.traffic = {static_cast<double>(kBytesInASecond), static_cast<double>(kBytesOutASecond)};
  return status;
}

}  // namespace

TEST_CASE("a room's frame window keeps the times of its last frames, the oldest pushed out") {
  FrameWindow window;
  CHECK(window.times().frames == 0);
  CHECK(window.times().average_seconds() == 0.0);
  window.note(kLongFrame);
  for (std::size_t i = 1; i < kFrameWindowFrames; ++i) window.note(kShortFrame);
  FrameTimes full = window.times();
  CHECK(full.frames == kFrameWindowFrames);
  CHECK(full.worst_seconds == doctest::Approx(kLongFrame));
  window.note(kShortFrame);  // the long one leaves the window
  full = window.times();
  CHECK(full.frames == kFrameWindowFrames);
  CHECK(full.worst_seconds == doctest::Approx(kShortFrame));
  CHECK(full.average_seconds() == doctest::Approx(kShortFrame));
}

TEST_CASE("a server's traffic rate is over the samples of the last seconds, one a second at most") {
  TrafficRate rate;
  rate.note(kStart, {0, 0});
  CHECK(rate.per_second().received == 0.0);                                              // (one sample makes no rate)
  rate.note(kStart + kTrafficSampleSeconds / 2.0, {kBytesInASecond, kBytesOutASecond});  // too soon: not kept
  CHECK(rate.per_second().received == 0.0);
  for (std::size_t second = 1; second <= kTrafficSamples * 2; ++second) {
    rate.note(kStart + static_cast<double>(second), {kBytesInASecond * second, kBytesOutASecond * second});
  }
  CHECK(rate.per_second().received == doctest::Approx(static_cast<double>(kBytesInASecond)));
  CHECK(rate.per_second().sent == doctest::Approx(static_cast<double>(kBytesOutASecond)));
}

TEST_CASE("the bytes the connections move are counted from every thread") {
  const redgem::net::TrafficTotals before = redgem::net::traffic_totals();
  redgem::net::count_received(kBytesInASecond);
  redgem::net::count_sent(kBytesOutASecond);
  const redgem::net::TrafficTotals after = redgem::net::traffic_totals();
  CHECK(after.received - before.received >= kBytesInASecond);
  CHECK(after.sent - before.sent >= kBytesOutASecond);
}

TEST_CASE("a server's status is one JSON document of counts and times, in milliseconds") {
  const nlohmann::json json = status_json(two_rooms());
  CHECK(json["engine_version"] == "1.2.3");
  CHECK(json["uptime_seconds"] == kUptime);
  CHECK(json["rooms"]["open"] == kRooms);
  CHECK(json["rooms"]["limit"] == kMaxRooms);
  REQUIRE(json["rooms"]["list"].size() == kRooms);
  CHECK(json["rooms"]["list"][0]["players"] == kFirstRoomPlayers);
  CHECK(json["rooms"]["list"][1]["frame_ms"]["worst"].get<double>() == doctest::Approx(kLongFrame * kMilliseconds));
  CHECK(json["players"]["total"] == kFirstRoomPlayers + kSecondRoomPlayers);
  CHECK(json["players"]["limit_per_room"] == kMaxPlayers);
  // (Three short frames and a long one.)
  const double average = (3 * kShortFrame + kLongFrame) / 4 * kMilliseconds;
  CHECK(json["frame_ms"]["average"].get<double>() == doctest::Approx(average).epsilon(kTolerance));
  CHECK(json["frame_ms"]["worst"].get<double>() == doctest::Approx(kLongFrame * kMilliseconds));
  CHECK(json["frame_ms"]["budget"].get<double>() == doctest::Approx(kTick * kMilliseconds).epsilon(kTolerance));
  CHECK(json["bytes_per_second"]["in"] == kBytesInASecond);
  CHECK(json["bytes_per_second"]["out"] == kBytesOutASecond);
  CHECK(json["threads"]["running"] == kThreads);
  CHECK(json["threads"]["limit"] == kMaxThreads);
}

TEST_CASE("a server's status says nothing of its rooms' codes or players, and fits on one log line") {
  ServerStatus empty;
  const nlohmann::json json = status_json(empty);
  CHECK(json["rooms"]["list"].empty());
  CHECK(json["frame_ms"]["average"] == 0.0);
  const std::string line = status_line(two_rooms());
  CHECK(line.find('\n') == std::string::npos);
  CHECK(line.find("2/8 rooms") != std::string::npos);
  CHECK(line.find("5 players") != std::string::npos);
  CHECK(line.find("10.0 ms worst") != std::string::npos);
  CHECK(line.find("500.0 B/s in") != std::string::npos);
}

TEST_CASE("the status port answers an HTTP request once its head arrived, and a quiet connection plainly") {
  CHECK(status_reply("", kNoTime, false) == StatusReply::Wait);
  CHECK(status_reply("GET / HTTP/1.1\r\n", kNoTime, false) == StatusReply::Wait);
  CHECK(status_reply(kGet, kNoTime, false) == StatusReply::Http);
  CHECK(status_reply("GET /\n\n", kNoTime, false) == StatusReply::Http);
  CHECK(status_reply("", kPlainAnswerAfterSeconds, false) == StatusReply::Plain);
  CHECK(status_reply("", kStatusRequestSeconds - kPlainAnswerAfterSeconds, false) == StatusReply::Plain);
  CHECK(status_reply("", kPlainAnswerAfterSeconds, true) == StatusReply::Drop);  // (a check that the port is open)
}

TEST_CASE("the status port lets a request go unanswered when it is too long, too slow, or cut short") {
  CHECK(status_reply(std::string(kMostStatusRequestBytes + 1, 'x'), kNoTime, false) == StatusReply::Drop);
  CHECK(status_reply("GET / HTTP/1.1\r\n", kStatusRequestSeconds, false) == StatusReply::Drop);
  // A head that comes whole only at the deadline is too late; just before it, it is answered.
  const float just_before = kStatusRequestSeconds - kPlainAnswerAfterSeconds;
  CHECK(status_reply(kGet, just_before, false) == StatusReply::Http);
  CHECK(status_reply(kGet, kStatusRequestSeconds, false) == StatusReply::Drop);
  CHECK(status_reply("GET / HTTP/1.1\r\n", kNoTime, true) == StatusReply::Drop);
}

TEST_CASE("the status port's HTTP answer is the document to a GET, and a refusal to anything else") {
  const std::string ok = http_status_answer(kGet, kDocument);
  CHECK(ok.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
  CHECK(ok.find("Content-Type: application/json\r\n") != std::string::npos);
  CHECK(ok.find("Content-Length: 7\r\n") != std::string::npos);
  CHECK(ok.find("Connection: close\r\n") != std::string::npos);
  CHECK(ok.substr(ok.size() - std::string(kDocument).size()) == kDocument);
  const std::string refused = http_status_answer("POST / HTTP/1.1\r\n\r\n", kDocument);
  CHECK(refused.rfind("HTTP/1.1 405 Method Not Allowed\r\n", 0) == 0);
  CHECK(refused.find(kDocument) == std::string::npos);
  CHECK(plain_status_answer(kDocument) == std::string(kDocument) + "\n");
  const std::string head = http_status_answer("\r\nHEAD / HTTP/1.1\r\n\r\n", kDocument);
  CHECK(head.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
  CHECK(head.find("Content-Length: 7\r\n") != std::string::npos);
  CHECK(head.find(kDocument) == std::string::npos);
  CHECK(http_status_answer("\r\nGET / HTTP/1.1\r\n\r\n", kDocument).rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
}

TEST_CASE("SIGUSR1 asks a server for its status in the log, once each time") {
  StatusSignal signal;
  CHECK_FALSE(signal.taken());
  if (!StatusSignal::supported()) return;
#ifdef SIGUSR1
  std::raise(SIGUSR1);
#endif
  CHECK(signal.taken());
  CHECK_FALSE(signal.taken());
}
