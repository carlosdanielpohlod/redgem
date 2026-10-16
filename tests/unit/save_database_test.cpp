#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "server/save_database.hpp"

using namespace redgem;
using redgem::server::SaveDatabase;

namespace {

constexpr int kRooms = 4;  // threads writing at once, as a server's rooms do
constexpr int kWritesPerRoom = 200;
constexpr double kVolume = 0.35;

// A server's data folder that does not exist yet, removed when the test ends.
struct TempData {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-save-database-test";
  std::filesystem::path database = dir / "data" / "save.sqlite3";
  TempData() { std::filesystem::remove_all(dir); }
  ~TempData() { std::filesystem::remove_all(dir); }
};

std::unique_ptr<SaveDatabase> opened(const std::filesystem::path& file) {
  std::string error;
  auto database = SaveDatabase::open(file, error);
  REQUIRE_MESSAGE(database != nullptr, error);
  return database;
}

}  // namespace

TEST_CASE("a server's saves are written one key at a time, and are there after a restart") {
  const TempData temp;
  auto database = opened(temp.database);
  CHECK(database->set("best", 1250.0));
  CHECK(database->set("players", {{"user-1", {{"coins", 12.0}}}}));
  CHECK(database->set("volume", kVolume));
  // Each change is in the file as soon as it is made: another reader sees it with the store still open.
  auto reader = opened(temp.database);
  CHECK(reader->get("best")->is_number_integer());  // whole numbers come back whole
  CHECK((*reader->get("players"))["user-1"]["coins"] == 12);
  CHECK(database->set("best", 900.0));
  CHECK(*reader->get("best") == 900);

  database.reset();
  reader.reset();
  auto restarted = opened(temp.database);
  CHECK(restarted->keys() == std::vector<std::string>{"best", "players", "volume"});
  CHECK(restarted->get("volume")->get<double>() == doctest::Approx(kVolume));
}

TEST_CASE("saved values are removed one by one, by saving nothing, or all at once") {
  const TempData temp;
  auto database = opened(temp.database);
  database->set("a", 1);
  database->set("b", 2);
  database->set("c", 3);
  CHECK(database->remove("a"));
  CHECK(database->set("b", nullptr));
  CHECK(database->remove("never-there"));
  CHECK(opened(temp.database)->keys() == std::vector<std::string>{"c"});
  CHECK(database->clear());
  CHECK(opened(temp.database)->keys().empty());
  CHECK_FALSE(database->get("c").has_value());
}

TEST_CASE("rooms writing at once from their own threads lose nothing") {
  const TempData temp;
  auto database = opened(temp.database);
  std::vector<std::thread> rooms;
  for (int room = 0; room < kRooms; ++room) {
    rooms.emplace_back([&database, room] {
      for (int write = 0; write < kWritesPerRoom; ++write) {
        const std::string key = "room-" + std::to_string(room) + "-" + std::to_string(write);
        database->set(key, write);
        database->get(key);
        database->set("last", room);
      }
    });
  }
  for (std::thread& room : rooms) room.join();
  const std::vector<std::string> keys = opened(temp.database)->keys();
  CHECK(keys.size() == static_cast<std::size_t>(kRooms * kWritesPerRoom + 1));
  CHECK(*database->get("room-3-199") == 199);
}

TEST_CASE("a database in memory keeps values for this run only, and one that cannot be opened says why") {
  const TempData temp;
  auto memory = opened({});
  CHECK(memory->set("best", 3));
  CHECK(*memory->get("best") == 3);
  CHECK(memory->where().empty());

  // A file where the data folder should be: nothing can be made under it.
  std::filesystem::create_directories(temp.dir);
  std::ofstream(temp.dir / "data") << "in the way";
  std::string error;
  CHECK(SaveDatabase::open(temp.database, error) == nullptr);
  CHECK_FALSE(error.empty());
}
