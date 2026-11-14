#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "load/load_bot.hpp"

// What the load tools (load_test.cpp, server_load.cpp) share: bots joining a dedicated server and
// playing on it, and the server's processor time read from /proc (Linux).
namespace redgem::load {

using BotList = std::vector<std::unique_ptr<LoadBot>>;

// Seconds on the steady clock.
double now_seconds();
// The processor time process `pid` took so far, in seconds.
double cpu_seconds(int pid);
// Every bot reads what arrived and says what is due; then a short pause.
void tick_all(BotList& bots);
// Ticks every bot for `seconds`.
void run_for(BotList& bots, double seconds);
// One more bot in, joined once the others are in (a server lets only a few from one address wait
// to be placed at once). False when it could not join.
bool join_one(BotList& bots, std::uint16_t port);

}  // namespace redgem::load
