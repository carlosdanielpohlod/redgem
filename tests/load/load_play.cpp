#include "load/load_play.hpp"

#include <unistd.h>

#include <chrono>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <thread>

namespace redgem::load {

namespace {

constexpr double kJoinTimeoutSeconds = 15.0;  // the first one of a room waits for its game to start
constexpr std::chrono::milliseconds kTickPause{1};
// /proc/<pid>/stat: the processor time a process took (fields 14 and 15 of the line, in clock
// ticks) comes after the name in parentheses and this many fields more.
constexpr int kFieldsBeforeUserTime = 11;

}  // namespace

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

}  // namespace redgem::load
