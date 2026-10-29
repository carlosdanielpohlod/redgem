#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include "core/files.hpp"
#include "storage/save_file.hpp"
#include "storage/save_slots.hpp"
#include "server/save_database.hpp"
using namespace redgem;
namespace {
struct SlotFolder {
  std::filesystem::path root = std::filesystem::temp_directory_path() / "redgem-save-slots-test";
  std::filesystem::path file = root / "save.json";
  SlotFolder() { std::filesystem::remove_all(root); }
  ~SlotFolder() { std::filesystem::remove_all(root); }
};
std::string bytes(const std::filesystem::path& file) {
  std::ifstream input(file, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
}  // namespace
TEST_CASE("save slots use portable bounded names and reject paths") {
  for (const auto* name : {"default", "a", "0", "campaign-1", "profile_2"}) CHECK(valid_save_slot(name));
  for (const auto* name : {"", ".", "..", "../save", "a/b", "a\\b", "a:b", "User", "-a", "a b"})
    CHECK_FALSE(valid_save_slot(name));
  CHECK(valid_save_slot(std::string(kMaxSaveSlotName, 'a')));
  CHECK_FALSE(valid_save_slot(std::string(kMaxSaveSlotName + 1, 'a')));
  CHECK_FALSE(valid_save_slot(std::string("a\0b", 3)));
}
TEST_CASE("named saves leave the legacy default file alone and retain cached values") {
  SlotFolder folder;
  SaveFile file(folder.file);
  REQUIRE(file.set("coins", 7));
  const auto original = bytes(folder.file);
  SaveSlots slots;
  slots.use_default(file);
  auto& first = slots.select("campaign");
  REQUIRE(first.set("coins", 1));
  REQUIRE(slots.select("other").set("coins", 9));
  CHECK(&first == &slots.select("campaign"));
  REQUIRE(first.set("level", 2));
  CHECK(first.get("coins") == 1);
  CHECK(slots.select("other").get("coins") == 9);
  CHECK(slots.select("default").get("coins") == 7);
  CHECK(bytes(folder.file) == original);
  SaveFile reopened(folder.file);
  SaveSlots second;
  second.use_default(reopened);
  CHECK(second.select("campaign").get("coins") == 1);
  CHECK(second.select("campaign").get("level") == 2);
  CHECK(second.names() == std::vector<std::string>{"campaign", "default", "other"});
}
TEST_CASE("named saves recover their own backup and fresh runs preserve damaged evidence") {
  SlotFolder folder;
  SaveFile file(folder.file);
  SaveSlots slots;
  slots.use_default(file);
  auto& saved = slots.select("campaign");
  REQUIRE(saved.set("coins", 1));
  REQUIRE(saved.set("coins", 2));
  const auto path = save_slot_file(folder.file, "campaign");
  std::ofstream(path, std::ios::trunc) << "broken";
  SaveFile reopened(folder.file);
  SaveSlots fresh;
  fresh.use_default(reopened);
  CHECK(fresh.select("campaign").get("coins") == 1);
  CHECK(std::filesystem::exists(path.string() + ".damaged"));
  remove_save(folder.file);
  SaveFile reset(folder.file);
  SaveSlots empty;
  empty.use_default(reset);
  CHECK(empty.names() == std::vector<std::string>{"default"});
  CHECK(std::filesystem::exists(path.string() + ".damaged"));
}
TEST_CASE("in-memory save slots retain data and bound the number opened") {
  SaveFile memory;
  SaveSlots slots;
  slots.use_default(memory);
  for (std::size_t index = 0; index < kMaxOpenSaveSlots; ++index)
    REQUIRE(slots.select("slot-" + std::to_string(index)).set("number", index));
  CHECK_THROWS_AS(slots.select("another"), std::runtime_error);
  CHECK(slots.select("slot-0").get("number") == 0);
  REQUIRE(slots.select("default").set("number", 99));
  CHECK(slots.select("default").get("number") == 99);
  CHECK(slots.names().size() == kMaxOpenSaveSlots + 1);
}
TEST_CASE("database slot selection is isolated per room and updates share the database lock") {
  SlotFolder folder;
  std::string error;
  auto database = server::SaveDatabase::open(folder.root / "save.sqlite", error);
  REQUIRE_MESSAGE(database, error);
  REQUIRE(database->set("coins", 7));
  SaveSlots first, second;
  first.use_default(*database);
  second.use_default(*database);
  auto& a = first.select("campaign");
  auto& b = second.select("campaign");
  auto& other = second.select("other");
  REQUIRE(other.set("coins", 99));
  constexpr int kUpdates = 200;
  const auto increment = [](const auto& value) { return value.value_or(0).template get<int>() + 1; };
  std::thread one([&] {
    for (int i = 0; i < kUpdates; ++i) REQUIRE(a.update("coins", increment));
  });
  std::thread two([&] {
    for (int i = 0; i < kUpdates; ++i) REQUIRE(b.update("coins", increment));
  });
  one.join();
  two.join();
  CHECK(a.get("coins") == kUpdates * 2);
  CHECK(other.get("coins") == 99);
  CHECK(database->get("coins") == 7);
  CHECK_THROWS(a.update("coins", [](const auto&) -> nlohmann::json { throw std::runtime_error("aborted"); }));
  CHECK(a.get("coins") == kUpdates * 2);
  REQUIRE(a.clear());
  CHECK(b.keys().empty());
  CHECK(other.get("coins") == 99);
  CHECK(first.names() == std::vector<std::string>{"campaign", "default", "other"});
}

TEST_CASE("views of a shared memory backend refer to the same named data") {
  SaveFile memory;
  SaveSlots one, two;
  one.use_default(memory);
  two.use_default(memory);
  REQUIRE(one.select("campaign").set("coins", 3));
  CHECK(two.select("campaign").get("coins") == 3);
  REQUIRE(two.select("campaign").set("level", 2));
  CHECK(one.select("campaign").get("level") == 2);
}
TEST_CASE("slot paths preserve Unicode bases and prefix platform-reserved logical names") {
  const auto file = std::filesystem::u8path("ação.json");
  CHECK(save_slots_folder(file) == std::filesystem::u8path("ação.json.slots"));
  CHECK(save_slot_file(file, "con").filename() == "slot-con.json");
}
TEST_CASE("backup-only named saves are listed and recovered") {
  SlotFolder folder;
  SaveFile file(folder.file);
  SaveSlots slots;
  slots.use_default(file);
  REQUIRE(slots.select("campaign").set("coins", 1));
  REQUIRE(slots.select("campaign").set("coins", 2));
  std::filesystem::remove(save_slot_file(folder.file, "campaign"));
  SaveFile reopened(folder.file);
  SaveSlots fresh;
  fresh.use_default(reopened);
  CHECK(fresh.names() == std::vector<std::string>{"campaign", "default"});
  CHECK(fresh.select("campaign").get("coins") == 1);
}
#ifndef _WIN32
TEST_CASE("named saves refuse existing symbolic links without touching their targets") {
  SlotFolder folder;
  std::filesystem::create_directories(folder.root);
  const auto canary = folder.root / "canary.json";
  std::ofstream(canary) << "canary";
  SaveFile file(folder.file);
  std::filesystem::create_directories(save_slots_folder(folder.file));
  std::filesystem::create_symlink(canary, save_slot_file(folder.file, "linked"));
  CHECK_THROWS_AS(file.open_slot("linked"), std::runtime_error);
  std::filesystem::create_symlink(canary, save_backup_of(save_slot_file(folder.file, "backup")));
  CHECK_THROWS_AS(file.open_slot("backup"), std::runtime_error);
  CHECK(bytes(canary) == "canary");
  const auto real_folder = folder.root / "real-slots";
  std::filesystem::rename(save_slots_folder(folder.file), real_folder);
  std::filesystem::create_directory_symlink(real_folder, save_slots_folder(folder.file));
  CHECK_THROWS_AS(file.open_slot("new"), std::runtime_error);
  CHECK_FALSE(std::filesystem::exists(real_folder / "slot-new.json"));
}
#endif

TEST_CASE("database slot views can close while another room updates its slot") {
  std::string error;
  auto database = server::SaveDatabase::open({}, error);
  REQUIRE_MESSAGE(database, error);
  auto writing = database->open_slot("campaign");
  constexpr int kTurns = 200;
  std::thread updater([&] {
    for (int i = 0; i < kTurns; ++i)
      CHECK(writing->update("coins", [](const auto& value) { return value.value_or(0).template get<int>() + 1; }));
  });
  std::thread observer([&] {
    for (int i = 0; i < kTurns; ++i) {
      auto transient = database->open_slot("campaign");
      transient->get("coins");
    }
  });
  updater.join();
  observer.join();
  CHECK(writing->get("coins") == kTurns);
}
