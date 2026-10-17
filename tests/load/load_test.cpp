// How a dedicated server holds up under players: bots join one of its rooms, run around and shoot
// as players do, and what it costs is measured (bytes a second each player sends and receives,
// the server's processor time per frame, and how long a game message takes from one player to
// another). For whoever works on the network code: not shipped. tests/load/run_load_test.sh
// starts the server and runs this against it; the ctest entry load.short is a short run of it.
//
//   redgem_load_test --port <server's port> --server-pid <its process> [--seconds 10] [4 8 15]
//
// It prints a table (Markdown) with a line for each number of players, and fails when a round
// did not work as a game would (a bot could not join, or heard nothing).

#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "load/load_bot.hpp"
#include "load/load_report.hpp"

namespace {

using redgem::load::LoadBot;
using redgem::load::RoundResult;
using BotList = std::vector<std::unique_ptr<LoadBot>>;

constexpr double kJoinTimeoutSeconds = 15.0;  // the first one waits for the room's game to start
constexpr double kWarmUpSeconds = 1.0;        // everyone hears everyone before anything is counted
constexpr double kRestSeconds = 1.0;          // between rounds: the last room closes
constexpr std::chrono::milliseconds kTickPause{1};
constexpr int kDefaultSeconds = 10;
constexpr int kDefaultPlayers[] = {4, 8, 15};
// /proc/<pid>/stat: the processor time a process took (fields 14 and 15 of the line, in clock
// ticks) comes after the name in parentheses and this many fields more.
constexpr int kFieldsBeforeUserTime = 11;

double now_seconds() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

double cpu_seconds(int pid) {
  std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
  const std::string line((std::istreambuf_iterator<char>(stat)), std::istreambuf_iterator<char>());
  std::istringstream fields(line.substr(line.rfind(')') + 1));
  std::string field;
  for (int i = 0; i < kFieldsBeforeUserTime; ++i) fields >> field;
  double user = 0.0;
  double system = 0.0;
  fields >> user >> system;
  return (user + system) / static_cast<double>(sysconf(_SC_CLK_TCK));
}

void tick_all(BotList& bots) {
  for (auto& bot : bots) bot->tick(now_seconds());
  std::this_thread::sleep_for(kTickPause);
}

void run_for(BotList& bots, double seconds) {
  for (const double end = now_seconds() + seconds; now_seconds() < end;) tick_all(bots);
}

// One more bot in the room, joined once the others are in (a server lets only a few from one
// address wait to be placed at once). False when it could not join.
bool join_one(BotList& bots, std::uint16_t port) {
  auto bot = std::make_unique<LoadBot>(static_cast<int>(bots.size()) + 1);
  std::string error;
  if (!bot->connect(port, error)) return false;
  bots.push_back(std::move(bot));
  for (const double end = now_seconds() + kJoinTimeoutSeconds; now_seconds() < end;) {
    tick_all(bots);
    if (bots.back()->welcomed()) return true;
  }
  return false;
}

RoundResult play_round(std::uint16_t port, int server_pid, int players, double seconds) {
  BotList bots;
  RoundResult round;
  round.players = players;
  while (static_cast<int>(bots.size()) < players && join_one(bots, port)) {
  }
  run_for(bots, kWarmUpSeconds);
  for (auto& bot : bots) bot->reset_counts();
  const double cpu_before = cpu_seconds(server_pid);
  const double start = now_seconds();
  run_for(bots, seconds);
  round.seconds = now_seconds() - start;
  round.server_cpu_seconds = cpu_seconds(server_pid) - cpu_before;
  for (const auto& bot : bots) round.bots.push_back(bot->counts());
  return round;
}

struct Options {
  std::uint16_t port = 0;
  int server_pid = 0;
  double seconds = kDefaultSeconds;
  std::vector<int> players;
};

Options read_options(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const bool has_value = i + 1 < argc;
    if (arg == "--port" && has_value)
      options.port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
    else if (arg == "--server-pid" && has_value)
      options.server_pid = std::stoi(argv[++i]);
    else if (arg == "--seconds" && has_value)
      options.seconds = std::stod(argv[++i]);
    else
      options.players.push_back(std::stoi(arg));
  }
  if (options.players.empty()) options.players.assign(std::begin(kDefaultPlayers), std::end(kDefaultPlayers));
  return options;
}

}  // namespace

int main(int argc, char** argv) {
  const Options options = read_options(argc, argv);
  std::puts(redgem::load::table_heading().c_str());
  bool worked = true;
  for (const int players : options.players) {
    const RoundResult round = play_round(options.port, options.server_pid, players, options.seconds);
    std::puts(redgem::load::table_line(round).c_str());
    std::fflush(stdout);
    worked = worked && redgem::load::round_worked(round);
    std::this_thread::sleep_for(std::chrono::duration<double>(kRestSeconds));
  }
  return worked ? 0 : 1;
}
