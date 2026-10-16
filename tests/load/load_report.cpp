#include "load/load_report.hpp"

#include <algorithm>
#include <cstdio>

#include "core/config.hpp"

namespace redgem::load {

namespace {

constexpr double kMilliseconds = 1e3;
constexpr double kMedian = 0.5;
constexpr double kNinetyFifth = 0.95;
constexpr std::size_t kLineBytes = 256;

struct Totals {
  double sent = 0.0;
  double received = 0.0;
  double pose_bytes = 0.0;
  double poses = 0.0;
  double message_bytes = 0.0;
  double messages = 0.0;
  std::vector<double> latencies;
};

Totals add_up(const RoundResult& round) {
  Totals totals;
  for (const BotCounts& bot : round.bots) {
    totals.sent += static_cast<double>(bot.bytes_sent);
    totals.received += static_cast<double>(bot.bytes_received);
    totals.pose_bytes += static_cast<double>(bot.pose_bytes);
    totals.poses += static_cast<double>(bot.poses_sent);
    totals.message_bytes += static_cast<double>(bot.message_bytes);
    totals.messages += static_cast<double>(bot.messages_sent);
    totals.latencies.insert(totals.latencies.end(), bot.latencies_ms.begin(), bot.latencies_ms.end());
  }
  std::sort(totals.latencies.begin(), totals.latencies.end());
  return totals;
}

// The value `share` of the way through sorted `values` (0 for none).
double percentile(const std::vector<double>& values, double share) {
  if (values.empty()) return 0.0;
  return values[static_cast<std::size_t>(share * static_cast<double>(values.size() - 1))];
}

double average(double total, double count) { return count > 0.0 ? total / count : 0.0; }

bool bot_worked(const BotCounts& bot) { return bot.poses_sent > 0 && bot.poses_heard > 0 && !bot.latencies_ms.empty(); }

}  // namespace

std::string table_heading() {
  return "| players | sent B/s per player | received B/s per player | bytes per pose | bytes per message | "
         "server CPU ms per frame | latency ms p50 / p95 / max |\n"
         "|---|---|---|---|---|---|---|";
}

std::string table_line(const RoundResult& round) {
  const Totals totals = add_up(round);
  const double player_seconds = round.seconds * round.players;
  const double frames = round.seconds / config::kFixedDeltaSeconds;
  char line[kLineBytes];
  std::snprintf(line, sizeof(line), "| %d | %.0f | %.0f | %.1f | %.1f | %.3f | %.1f / %.1f / %.1f |", round.players,
                totals.sent / player_seconds, totals.received / player_seconds,
                average(totals.pose_bytes, totals.poses), average(totals.message_bytes, totals.messages),
                round.server_cpu_seconds * kMilliseconds / frames, percentile(totals.latencies, kMedian),
                percentile(totals.latencies, kNinetyFifth), totals.latencies.empty() ? 0.0 : totals.latencies.back());
  return line;
}

bool round_worked(const RoundResult& round) {
  return static_cast<int>(round.bots.size()) == round.players &&
         std::all_of(round.bots.begin(), round.bots.end(), bot_worked);
}

}  // namespace redgem::load
