#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <sqlite3.h>

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

#ifndef _WIN32  // (on Windows a file takes its folder's ACL: see KNOWN_LIMITATIONS.md)
// Whether only a file's owner may read or write it.
bool owners_only(const std::filesystem::path& file) {
  const auto mine = std::filesystem::perms::owner_read | std::filesystem::perms::owner_write;
  return (std::filesystem::status(file).permissions() & std::filesystem::perms::all) == mine;
}
#endif

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

TEST_CASE("an update reads and writes a key with no other room's in between: no change is lost") {
  const TempData temp;
  auto database = opened(temp.database);
  std::vector<std::thread> rooms;
  for (int room = 0; room < kRooms; ++room) {
    rooms.emplace_back([&database] {
      for (int write = 0; write < kWritesPerRoom; ++write) {
        database->update("visits",
                         [](const std::optional<nlohmann::json>& visits) { return visits.value_or(0).get<int>() + 1; });
      }
    });
  }
  for (std::thread& room : rooms) room.join();
  CHECK(*database->get("visits") == kRooms * kWritesPerRoom);
  // A change that fails leaves the key as it was; one that makes nothing removes it.
  CHECK_THROWS(database->update("visits", [](const std::optional<nlohmann::json>&) -> nlohmann::json {
    throw std::runtime_error("the game's block failed");
  }));
  CHECK(*database->get("visits") == kRooms * kWritesPerRoom);
  CHECK(database->update("visits", [](const std::optional<nlohmann::json>&) { return nlohmann::json(); }));
  CHECK_FALSE(database->get("visits").has_value());
}

TEST_CASE("a row that holds no value, written by something else than the server, is none") {
  const TempData temp;
  auto database = opened(temp.database);
  database->set("good", 1);
  sqlite3* other = nullptr;
  REQUIRE(sqlite3_open(temp.database.string().c_str(), &other) == SQLITE_OK);
  CHECK(sqlite3_exec(other, "INSERT INTO saves VALUES ('broken', '{ not json')", nullptr, nullptr, nullptr) ==
        SQLITE_OK);
  sqlite3_close(other);
  CHECK_FALSE(database->get("broken").has_value());
  CHECK(database->keys() == std::vector<std::string>{"good"});
}

TEST_CASE("each read sees the database as it is now, however often the store was read before") {
  const TempData temp;
  auto database = opened(temp.database);
  auto other = opened(temp.database);
  for (int value = 0; value < kWritesPerRoom; ++value) {
    other->set("seen", value);
    CHECK(*database->get("seen") == value);
    CHECK(database->keys() == std::vector<std::string>{"seen"});
  }
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

#ifndef _WIN32
// Players' saves are nobody else's on the machine: the database is its owner's alone, and so are
// the files sqlite keeps beside it.
TEST_CASE("a server's save database is made readable by its owner only") {
  const TempData temp;
  opened(temp.database)->set("best", 1);
  CHECK(owners_only(temp.database));
  std::filesystem::permissions(temp.database, std::filesystem::perms::all, std::filesystem::perm_options::add);
  const std::filesystem::path log = temp.database.string() + "-wal";
  std::ofstream(log, std::ios::binary);  // an empty log, as a run that ended may leave
  std::filesystem::permissions(log, std::filesystem::perms::all, std::filesystem::perm_options::add);
  const auto again = opened(temp.database);  // ones left open to everyone are closed again
  CHECK(owners_only(temp.database));
  REQUIRE(std::filesystem::exists(log));
  CHECK(owners_only(log));
}
#endif

TEST_CASE("a database whose saves table is not the server's does not open as if it could be used") {
  const TempData temp;
  std::filesystem::create_directories(temp.database.parent_path());
  sqlite3* other = nullptr;
  REQUIRE(sqlite3_open(temp.database.string().c_str(), &other) == SQLITE_OK);
  REQUIRE(sqlite3_exec(other, "CREATE TABLE saves (key TEXT)", nullptr, nullptr, nullptr) == SQLITE_OK);
  sqlite3_close(other);
  std::string error;
  CHECK(SaveDatabase::open(temp.database, error) == nullptr);
  CHECK(error.find("no such column") != std::string::npos);
}
