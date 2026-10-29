#pragma once

#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>
#include <utility>

#include "scene/gltf/gltf_loader.hpp"
#include "core/log.hpp"

namespace redgem {

// A detached result slot: abandoning it cancels queued work and discards an active read's result.
class SceneLoad {
 public:
  using Messages = std::vector<std::pair<log::Level, std::string>>;
  struct Result {
    LoadResult loaded;
    Messages messages;
  };
  std::optional<Result> take();
  void wait_until_ready();
  void finish(LoadResult result, Messages messages = {});

 private:
  std::mutex mutex_;
  std::condition_variable ready_;
  std::optional<Result> result_;
};

class SceneLoader {
 public:
  // Runs off thread: return CPU data only, without touching a game, a Ruby VM or the GPU.
  using Reader = std::function<LoadResult(const std::filesystem::path&, const std::string&)>;
  // Includes queued, active and completed slots, so uncollected results cannot grow without bound.
  static constexpr std::size_t kOutstandingLimit = 8;
  explicit SceneLoader(Reader reader);
  ~SceneLoader();
  SceneLoader(const SceneLoader&) = delete;
  SceneLoader& operator=(const SceneLoader&) = delete;
  std::shared_ptr<SceneLoad> submit(const std::filesystem::path& game, const std::string& name);

 private:
  struct Request {
    std::filesystem::path game;
    std::string name;
    std::weak_ptr<SceneLoad> result;
  };
  std::optional<Request> next();
  void run();
  void discard_abandoned();
  void queue_request(const std::filesystem::path& game, const std::string& name,
                     const std::shared_ptr<SceneLoad>& result);
  SceneLoad::Result read(const Request& request);
  Reader reader_;
  std::mutex mutex_;
  std::condition_variable changed_;
  std::deque<Request> queue_;
  std::vector<std::weak_ptr<SceneLoad>> outstanding_;
  bool stopping_ = false;
  std::thread worker_;
};

// One background parser shared by players and dedicated-server rooms in this process.
SceneLoader& shared_scene_loader();
LoadResult read_scene_data(const std::filesystem::path& game, const std::string& name);

}  // namespace redgem
