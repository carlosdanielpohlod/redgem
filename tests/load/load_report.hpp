#pragma once

#include <string>
#include <vector>

#include "load/load_bot.hpp"

namespace redgem::load {

// What one round of the load test measured: so many players in one room for so many seconds.
struct RoundResult {
  int players = 0;
  double seconds = 0.0;
  double server_cpu_seconds = 0.0;
  std::vector<BotCounts> bots;
};

// The heading of the table rounds are printed in (Markdown), and a round's line of it.
std::string table_heading();
std::string table_line(const RoundResult& round);
// Whether the round went as a game would: every bot sent and heard poses, and heard shots.
bool round_worked(const RoundResult& round);

}  // namespace redgem::load
