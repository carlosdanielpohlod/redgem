// Exit ordering matters: release a pending reader after the path-warning cache and log's normal
// statics would have been destroyed. The worker must still reject a new link and finish safely.
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <memory>

#include "app/scene_loader.hpp"
#include "core/exit_codes.hpp"
#include "core/safe_path.hpp"

namespace {
constexpr int kArguments = 2;
constexpr auto kReaderWait = std::chrono::seconds(10);

struct Pending {
  std::mutex mutex;
  std::condition_variable changed;
  std::filesystem::path folder;
  std::shared_ptr<redgem::SceneLoad> ticket;
  bool started = false;
  bool released = false;
  bool refused = false;
  ~Pending() {
    if (!refused) std::_Exit(redgem::kExitFailure);
    std::error_code error;
    std::filesystem::remove_all(folder, error);
  }
};
Pending& pending() {
  static Pending value;  // initialized before the loader: its fields outlive the worker
  return value;
}

void release_reader() {
  auto& state = pending();
  const std::lock_guard<std::mutex> hold(state.mutex);
  state.released = true;
  state.changed.notify_all();
}

redgem::LoadResult read_at_exit(const std::filesystem::path& folder, const std::string&) {
  auto& state = pending();
  {
    std::unique_lock<std::mutex> hold(state.mutex);
    state.started = true;
    state.changed.notify_all();
    if (!state.changed.wait_for(hold, kReaderWait, [&] { return state.released; })) return {};
  }
  state.refused = redgem::through_link(folder, "at-exit.glb");
  return {};
}

redgem::SceneLoader& loader() {
  static redgem::SceneLoader reader(read_at_exit);
  return reader;
}

void prepare(const std::filesystem::path& folder) {
  std::filesystem::remove_all(folder);
  std::filesystem::create_directories(folder);
  std::ofstream(folder / "real.glb") << "fixture";
  for (const char* name : {"warmup.glb", "at-exit.glb"})
    std::filesystem::create_symlink(folder / "real.glb", folder / name);
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != kArguments) return redgem::kExitUsage;
  auto& state = pending();
  state.folder = std::filesystem::absolute(argv[1]);
  prepare(state.folder);
  auto& reader = loader();
  if (std::atexit(release_reader) != 0) return redgem::kExitFailure;
  if (!redgem::through_link(state.folder, "warmup.glb")) return redgem::kExitFailure;
  state.ticket = reader.submit(state.folder, "main");
  std::unique_lock<std::mutex> hold(state.mutex);
  return state.changed.wait_for(hold, kReaderWait, [&] { return state.started; }) ? redgem::kExitSuccess
                                                                                  : redgem::kExitFailure;
}
