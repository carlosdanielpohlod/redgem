#include "app/scene_loader.hpp"

#include <algorithm>
#include <exception>
#include <utility>

#include "app/scene_files.hpp"
#include "core/log.hpp"
#include "core/safe_path.hpp"
#include "core/short_text.hpp"

namespace redgem {
namespace {
constexpr std::size_t kMaxReadMessages = 128;  // plus one notice when further messages are dropped
constexpr std::size_t kMaxReadMessageBytes = 2048;

void capture_message(SceneLoad::Messages& messages, log::Level level, const std::string& text) {
  if (messages.size() > kMaxReadMessages) return;
  if (messages.size() == kMaxReadMessages) {
    messages.emplace_back(log::Level::Warning, "additional scene read diagnostics were dropped");
    return;
  }
  std::string bounded(utf8_prefix(text, kMaxReadMessageBytes));
  if (bounded.size() < text.size()) bounded += "...";
  messages.emplace_back(level, std::move(bounded));
}

LoadResult invoke_reader(const SceneLoader::Reader& reader, const std::filesystem::path& game,
                         const std::string& name) {
  try {
    return reader(game, name);
  } catch (const std::exception& error) {
    return {false, error.what(), {}, {}};
  } catch (...) {
    return {false, "scene reader failed", {}, {}};
  }
}
}  // namespace

std::optional<SceneLoad::Result> SceneLoad::take() {
  const std::lock_guard<std::mutex> hold(mutex_);
  return std::exchange(result_, std::nullopt);
}
void SceneLoad::finish(LoadResult result, Messages messages) {
  {
    const std::lock_guard<std::mutex> hold(mutex_);
    result_ = Result{std::move(result), std::move(messages)};
  }
  ready_.notify_all();
}
void SceneLoad::wait_until_ready() {
  std::unique_lock<std::mutex> hold(mutex_);
  ready_.wait(hold, [this] { return result_.has_value(); });
}

SceneLoader::SceneLoader(Reader reader) : reader_(std::move(reader)), worker_([this] { run(); }) {}
SceneLoader::~SceneLoader() {
  {
    const std::lock_guard<std::mutex> hold(mutex_);
    stopping_ = true;
    queue_.clear();
  }
  changed_.notify_one();
  worker_.join();
}

std::shared_ptr<SceneLoad> SceneLoader::submit(const std::filesystem::path& game, const std::string& name) {
  const std::lock_guard<std::mutex> hold(mutex_);
  discard_abandoned();
  if (outstanding_.size() >= kOutstandingLimit) return {};
  auto result = std::make_shared<SceneLoad>();
  outstanding_.push_back(result);
  queue_request(game, name, result);
  changed_.notify_one();
  return result;
}

void SceneLoader::queue_request(const std::filesystem::path& game, const std::string& name,
                                const std::shared_ptr<SceneLoad>& result) {
  try {
    const auto root = game.empty() ? std::filesystem::current_path() : std::filesystem::absolute(game);
    queue_.push_back({root, name, result});
  } catch (const std::exception& error) {
    result->finish({false, error.what(), {}, {}});
  }
}

void SceneLoader::discard_abandoned() {
  const auto expired_slot = [](const auto& slot) { return slot.expired(); };
  outstanding_.erase(std::remove_if(outstanding_.begin(), outstanding_.end(), expired_slot), outstanding_.end());
  const auto abandoned = [](const Request& request) { return request.result.expired(); };
  queue_.erase(std::remove_if(queue_.begin(), queue_.end(), abandoned), queue_.end());
}

std::optional<SceneLoader::Request> SceneLoader::next() {
  std::unique_lock<std::mutex> hold(mutex_);
  changed_.wait(hold, [this] { return stopping_ || !queue_.empty(); });
  if (stopping_) return std::nullopt;
  Request request = std::move(queue_.front());
  queue_.pop_front();
  return request;
}

SceneLoad::Result SceneLoader::read(const Request& request) {
  SceneLoad::Result result;
  const log::Capture quiet(
      [&](log::Level level, const std::string& text) { capture_message(result.messages, level, text); });
  result.loaded = invoke_reader(reader_, request.game, request.name);
  return result;
}

void SceneLoader::run() {
  while (auto request = next()) {
    if (request->result.expired()) continue;
    auto result = read(*request);
    if (auto slot = request->result.lock()) slot->finish(std::move(result.loaded), std::move(result.messages));
  }
}

LoadResult read_scene_data(const std::filesystem::path& game, const std::string& name) {
  LoadResult result;
  if (!safe_asset_name(name)) {
    result.error = "unsafe scene name";
    return result;
  }
  const auto file = scene_file(game, name);
  if (!file_inside(game, file.lexically_relative(game))) {
    result.error = "scene must be a regular file inside the game without symbolic links";
    return result;
  }
  return load_gltf_scene(file.string());
}

SceneLoader& shared_scene_loader() {
  static SceneLoader loader(read_scene_data);
  return loader;
}
}  // namespace redgem
