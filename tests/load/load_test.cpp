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

#include <chrono>
#include <cstdio>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include "load/load_play.hpp"
#include "load/load_report.hpp"

namespace {

using redgem::load::BotList;
using redgem::load::cpu_seconds;
using redgem::load::join_one;
using redgem::load::now_seconds;
using redgem::load::RoundResult;
using redgem::load::run_for;

constexpr double kWarmUpSeconds = 1.0;  // everyone hears everyone before anything is counted
constexpr double kRestSeconds = 1.0;    // between rounds: the last room closes
constexpr int kDefaultSeconds = 10;
constexpr int kDefaultPlayers[] = {4, 8, 15};

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
