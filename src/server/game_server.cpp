// GameServer: starting and stopping, and the lobby's turn.

#include "server/game_server.hpp"

#include <algorithm>
#include <thread>
#include <variant>

#include "app/scene_files.hpp"
#include "core/files.hpp"
#include "core/log.hpp"
#include "server/save_migration.hpp"

namespace redgem::server {

std::filesystem::path data_dir_beside(const std::filesystem::path& game_dir) {
  const std::filesystem::path game = normalized_folder(game_dir);
  return game.parent_path() / (game.filename().string() + kDataFolderSuffix);
}

bool GameServer::start(std::string& error) {
  auto scene = read_game_scene(settings_.game_dir);
  if (const auto* failed = std::get_if<std::string>(&scene)) {
    error = *failed;
    return false;
  }
  scene_ = std::move(std::get<SceneDesc>(scene));
  return open_saves(error) && lobby_.open(settings_.port, error, settings_.connections_per_address);
}

bool GameServer::open_saves(std::string& error) {
  const bool kept = !settings_.data_dir.empty();
  saves_ = SaveDatabase::open(kept ? settings_.data_dir / kSaveDatabaseName : std::filesystem::path(), error);
  if (saves_ == nullptr) {
    error = "cannot keep the game's saves: " + error;
    return false;
  }
  // (Not moved, the old file would be moved again at the next start, over what changed since.)
  const std::filesystem::path old_save = settings_.data_dir / kSaveFileName;
  if (!kept || move_into(*saves_, old_save)) return true;
  error = "cannot move the old save " + old_save.string() + " into " + saves_->where();
  return false;
}

void GameServer::tick(float seconds) {
  const auto in_rooms = [this](const std::string& address) { return players_from(address); };
  for (net::Arrival& arrival : lobby_.poll(seconds, in_rooms)) place(std::move(arrival));
  forget_closed_rooms();
}

GameServer::~GameServer() { stop(); }

void GameServer::stop() {
  for (auto& thread : threads_) thread->stop();
  threads_.clear();
  rooms_.clear();
  lobby_.close();
}

std::size_t GameServer::player_count() const {
  std::size_t players = 0;
  for (const Room& room : rooms_) players += room.slot->players();
  return players;
}

bool GameServer::visit(const std::string& code, const std::function<void(Engine&)>& visitor) {
  Room* room = find_room(code);
  return room != nullptr && room->slot->visit(visitor);
}

int GameServer::players_from(const std::string& address) const {
  int players = 0;
  for (const Room& room : rooms_) players += room.slot->links_from(address);
  return players;
}

void GameServer::forget_closed_rooms() {
  // (A room closes on its own thread, which lets go of its game: here only its place is freed.)
  rooms_.erase(std::remove_if(rooms_.begin(), rooms_.end(), [](const Room& room) { return room.slot->closed(); }),
               rooms_.end());
}

int processor_count() { return std::max(1, static_cast<int>(std::thread::hardware_concurrency())); }

}  // namespace redgem::server
