// A dedicated server under load, for the CI's Benchmarks workflow: M bots join it (filling its K
// rooms one by one, as quick play does) and play for T seconds, sending their poses at the rate a
// game's NetworkSync does, while the server's status port (--status-port) is read every second.
// tests/load/run_server_load.sh starts the server and runs this against it.
//
//   redgem_server_load --port <server's port> --status-port <its status port> --server-pid <pid>
//                      --clients <M> --rooms <K> [--seconds 20] [--out load.json]
//
// It prints a table (Markdown) and writes, with --out, the same results as redgem_bench does
// (tests/bench/bench_results.hpp): the server's frame time (its status's average and worst, one
// sample a second), the bytes a second it receives and sends, its processor time per tick, the
// clients it dropped and the rooms it did not open. tools/bench/check_thresholds.py holds them to
// tests/bench/thresholds.json. It fails only when the status port could not be read.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "bench/bench_results.hpp"
#include "core/config.hpp"
#include "load/load_play.hpp"
#include "load/status_probe.hpp"

namespace {

using redgem::bench::Result;
using redgem::bench::summarize;
using redgem::load::BotList;
using StatusList = std::vector<nlohmann::json>;
using Pointer = nlohmann::json::json_pointer;

constexpr int kDefaultSeconds = 20;
constexpr double kWarmUpSeconds = 2.0;  // everyone hears everyone before anything is counted
// Status samples from the first seconds are left out: its frame times are over the last five
// seconds (server/frame_window.hpp), which still held the rooms' games starting.
constexpr double kSettleSeconds = 5.0;
constexpr std::chrono::seconds kStatusEvery{1};
constexpr double kMillisecondsPerSecond = 1000.0;
constexpr const char* kSuite = "load";

struct Options {
  std::uint16_t port = 0;
  std::uint16_t status_port = 0;
  int server_pid = 0;
  int clients = 0;
  int rooms = 0;
  double seconds = kDefaultSeconds;
  std::string out;
};

// Reads the value of an option into `options`; false when `arg` is none of them.
bool read_option(const std::string& arg, const std::string& value, Options& options) {
  if (arg == "--port")
    options.port = static_cast<std::uint16_t>(std::stoi(value));
  else if (arg == "--status-port")
    options.status_port = static_cast<std::uint16_t>(std::stoi(value));
  else if (arg == "--server-pid")
    options.server_pid = std::stoi(value);
  else if (arg == "--clients")
    options.clients = std::stoi(value);
  else if (arg == "--rooms")
    options.rooms = std::stoi(value);
  else if (arg == "--seconds")
    options.seconds = std::stod(value);
  else if (arg == "--out")
    options.out = value;
  else
    return false;
  return true;
}

Options read_options(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; i += 2) {
    const bool has_value = i + 1 < argc;
    if (!has_value || !read_option(argv[i], argv[i + 1], options))
      throw std::invalid_argument(std::string("unknown option or missing value: ") + argv[i]);
  }
  return options;
}

// Reads the status port every second until `stop`, keeping what it said once the server settled.
class StatusWatch {
 public:
  explicit StatusWatch(std::uint16_t port) : port_(port), started_(redgem::load::now_seconds()) {
    thread_ = std::thread([this] { watch(); });
  }
  StatusList stop() {
    stop_ = true;
    thread_.join();
    return statuses_;
  }

 private:
  void watch() {
    while (!stop_) {
      std::this_thread::sleep_for(kStatusEvery);
      std::string error;
      const auto status = redgem::load::read_status(port_, error);
      if (!status) std::fprintf(stderr, "%s\n", error.c_str());
      if (status && settled()) statuses_.push_back(*status);
    }
  }
  bool settled() const { return redgem::load::now_seconds() - started_ >= kSettleSeconds; }

  std::uint16_t port_;
  double started_;
  std::atomic<bool> stop_{false};
  StatusList statuses_;
  std::thread thread_;
};

// What the server said of `path` (keys, one inside the next) in each status.
std::vector<double> each(const StatusList& statuses, const Pointer& path) {
  std::vector<double> values;
  for (const nlohmann::json& status : statuses) values.push_back(status.value(path, 0.0));
  return values;
}

int most_rooms_open(const StatusList& statuses) {
  int most = 0;
  for (const nlohmann::json& status : statuses) most = std::max(most, status.value(Pointer("/rooms/open"), 0));
  return most;
}

// Whether a bot is in a room and still connected.
bool still_playing(const std::unique_ptr<redgem::load::LoadBot>& bot) { return bot->welcomed() && !bot->gone(); }

int connected(const BotList& bots) { return static_cast<int>(std::count_if(bots.begin(), bots.end(), still_playing)); }

struct Measured {
  StatusList statuses;
  double cpu_ms_per_tick = 0.0;
  int dropped = 0;
};

Measured play(const Options& options) {
  BotList bots;
  while (static_cast<int>(bots.size()) < options.clients && redgem::load::join_one(bots, options.port)) {
  }
  redgem::load::run_for(bots, kWarmUpSeconds);
  const double cpu_before = redgem::load::cpu_seconds(options.server_pid);
  const double start = redgem::load::now_seconds();
  StatusWatch watch(options.status_port);
  redgem::load::run_for(bots, options.seconds);
  Measured measured;
  measured.statuses = watch.stop();
  const double ticks = (redgem::load::now_seconds() - start) / redgem::config::kFixedDeltaSeconds;
  measured.cpu_ms_per_tick =
      (redgem::load::cpu_seconds(options.server_pid) - cpu_before) * kMillisecondsPerSecond / ticks;
  measured.dropped = options.clients - connected(bots);
  return measured;
}

Result one_value(const std::string& name, const std::string& unit, const std::string& what, double value) {
  return {name, unit, what, summarize({value})};
}

std::vector<Result> results(const Measured& measured, const Options& options) {
  const StatusList& statuses = measured.statuses;
  return {
      {"load.server_frame_ms", "ms", "the status's average frame time, a second apart",
       summarize(each(statuses, Pointer("/frame_ms/average")))},
      {"load.server_frame_worst_ms", "ms", "the status's worst frame time of five seconds, a second apart",
       summarize(each(statuses, Pointer("/frame_ms/worst")))},
      {"load.server_bytes_in_per_s", "B/s", "the status's bytes a second received, a second apart",
       summarize(each(statuses, Pointer("/bytes_per_second/in")))},
      {"load.server_bytes_out_per_s", "B/s", "the status's bytes a second sent, a second apart",
       summarize(each(statuses, Pointer("/bytes_per_second/out")))},
      one_value("load.server_cpu_ms_per_tick", "ms", "the server's processor time per tick", measured.cpu_ms_per_tick),
      one_value("load.dropped_clients", "clients", "clients that could not join or were let go", measured.dropped),
      one_value("load.rooms_missing", "rooms", "rooms asked for that never opened",
                std::max(0, options.rooms - most_rooms_open(statuses)))};
}

int report(const std::vector<Result>& measured, const Options& options) {
  std::fputs(redgem::bench::results_table(measured).c_str(), stdout);
  std::string error;
  if (options.out.empty() || redgem::bench::write_results(options.out, kSuite, measured, error)) return 0;
  std::fprintf(stderr, "%s\n", error.c_str());
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = read_options(argc, argv);
    const Measured measured = play(options);
    if (measured.statuses.empty()) throw std::runtime_error("the status port never answered");
    return report(results(measured, options), options);
  } catch (const std::exception& error) {
    std::fprintf(stderr, "redgem_server_load: %s\n", error.what());
    return 1;
  }
}
