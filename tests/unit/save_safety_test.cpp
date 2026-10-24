// What keeps a player's save through crashes, power cuts and damaged files: flushed writes, the
// backup, and damaged or unreadable saves set aside or left alone, never written over.

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "core/platform.hpp"
#include "storage/durable_file.hpp"
#include "storage/save_file.hpp"
#include "test_support.hpp"

using namespace redgem;

namespace {

constexpr double kBestScore = 1250.0;
constexpr double kOlderScore = 900.0;
constexpr const char* kDamaged = "{ not json";  // what a save cut short by a crash may hold
constexpr const char* kAlsoDamaged = "[1, 2";   // another one, told apart from the first
constexpr int kWrites = 3;
constexpr int kLevel = 2;
constexpr int kFirstNumberedAside = 2;  // .damaged, then .damaged.2
// What sits beside a save: the ones being written, the one before it, and those found damaged.
constexpr const char* kPartial = ".part";
constexpr const char* kBackupPartial = ".bak.part";
constexpr const char* kBackup = ".bak";
constexpr const char* kDamagedAside = ".damaged";
constexpr const char* kSecondDamagedAside = ".damaged.2";
constexpr const char* kBackupDamagedAside = ".bak.damaged";

// A save file in a folder of its own that does not exist yet, removed when the test ends.
struct TempSave {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-save-safety-test";
  std::filesystem::path file = dir / "nested" / "game.json";
  TempSave() { std::filesystem::remove_all(dir); }
  ~TempSave() { std::filesystem::remove_all(dir); }
};

std::filesystem::path beside(const std::filesystem::path& file, const std::string& suffix) {
  return file.string() + suffix;
}

std::string contents_of(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void write_text(const std::filesystem::path& file, const std::string& text) {
  std::filesystem::create_directories(file.parent_path());
  std::ofstream(file, std::ios::binary) << text;
}

double best_in(const std::filesystem::path& file) {
  const auto best = SaveFile(file).get("best");
  REQUIRE(best.has_value());
  return best->get<double>();
}

// A save of `older`, then `best` over it: the backup holds `older`.
void save_twice(const std::filesystem::path& file, double older, double best) {
  SaveFile store(file);
  REQUIRE(store.set("best", older));
  REQUIRE(store.set("best", best));
}

}  // namespace

TEST_CASE("a damaged save file with no backup starts from nothing, and is kept aside") {
  const TempSave temp;
  write_text(temp.file, kDamaged);
  const test::LogCapture log;
  SaveFile store(temp.file);
  CHECK(store.keys().empty());
  CHECK(log.count(log::Level::Error) == 1);
  CHECK(log.contains(temp.file.string()));
  CHECK(log.contains(store.backup_file().string()));
  CHECK(contents_of(beside(temp.file, kDamagedAside)) == kDamaged);
  CHECK(store.set("best", kBestScore));
  CHECK(best_in(temp.file) == doctest::Approx(kBestScore));
  CHECK(contents_of(beside(temp.file, kDamagedAside)) == kDamaged);  // the next save does not write over it
}

TEST_CASE("the save being replaced is kept as the backup, exactly as it was, and written whole") {
  const TempSave temp;
  SaveFile store(temp.file);
  REQUIRE(store.set("best", kOlderScore));
  CHECK_FALSE(std::filesystem::exists(store.backup_file()));  // the first save replaces none
  const std::string older = contents_of(temp.file);
  REQUIRE(store.set("best", kBestScore));
  CHECK(contents_of(store.backup_file()) == older);
  CHECK_FALSE(std::filesystem::exists(beside(temp.file, kBackupPartial)));
  CHECK(best_in(temp.file) == doctest::Approx(kBestScore));
}

TEST_CASE("a damaged save falls back to its backup, says so naming both files, and puts it back in its place") {
  const TempSave temp;
  save_twice(temp.file, kOlderScore, kBestScore);
  write_text(temp.file, kDamaged);
  {
    const test::LogCapture log;
    SaveFile store(temp.file);
    CHECK(store.get("best")->get<double>() == doctest::Approx(kOlderScore));
    CHECK(log.count(log::Level::Error) == 1);
    CHECK(log.contains(temp.file.string() + " is damaged"));
    CHECK(log.contains(store.backup_file().string() + " was loaded instead"));
    CHECK(contents_of(beside(temp.file, kDamagedAside)) == kDamaged);
  }
  // The player quits without saving anything: the next run finds the backup in the save's place.
  const test::LogCapture next_run;
  CHECK(best_in(temp.file) == doctest::Approx(kOlderScore));
  CHECK(next_run.count(log::Level::Error) == 0);
}

TEST_CASE("after a damaged save, the next save does not make the damaged one the backup") {
  const TempSave temp;
  save_twice(temp.file, kOlderScore, kBestScore);
  write_text(temp.file, kDamaged);
  const test::LogCapture log;
  SaveFile store(temp.file);
  REQUIRE(store.set("level", kLevel));
  CHECK(best_in(store.backup_file()) == doctest::Approx(kOlderScore));
  CHECK(SaveFile(temp.file).get("level")->get<int>() == kLevel);
}

TEST_CASE("a missing save with a backup beside it loads the backup and puts it back in its place") {
  const TempSave temp;
  save_twice(temp.file, kOlderScore, kBestScore);
  std::filesystem::remove(temp.file);
  const test::LogCapture log;
  CHECK(best_in(temp.file) == doctest::Approx(kOlderScore));
  CHECK(log.count(log::Level::Warning) == 1);
  CHECK(log.contains(temp.file.string() + " is missing"));
  CHECK(std::filesystem::exists(temp.file));
}

TEST_CASE("when the save and its backup are both damaged, the game starts from nothing and loses neither") {
  const TempSave temp;
  write_text(beside(temp.file, kDamagedAside), kAlsoDamaged);  // one found damaged before
  write_text(temp.file, kDamaged);
  write_text(beside(temp.file, kBackup), kAlsoDamaged);
  const test::LogCapture log;
  SaveFile store(temp.file);
  CHECK(store.keys().empty());
  CHECK(log.count(log::Level::Error) == 1);
  CHECK(log.contains(temp.file.string()));
  CHECK(log.contains(store.backup_file().string()));
  CHECK(log.contains("starts from nothing"));
  // Two saves later the first new save is the backup: both damaged files are still there.
  CHECK(store.set("best", kOlderScore));
  CHECK(store.set("best", kBestScore));
  CHECK(best_in(temp.file) == doctest::Approx(kBestScore));
  CHECK(contents_of(beside(temp.file, kDamagedAside)) == kAlsoDamaged);
  CHECK(contents_of(beside(temp.file, kSecondDamagedAside)) == kDamaged);
  CHECK(contents_of(beside(temp.file, kBackupDamagedAside)) == kAlsoDamaged);
}

TEST_CASE("a save that cannot be read is not damaged: it is left where it is and never written over") {
  const TempSave temp;
  // A folder where the save should be: it cannot be read, as a file another program holds.
  std::filesystem::create_directories(temp.file);
  const test::LogCapture log;
  SaveFile store(temp.file);
  CHECK(store.keys().empty());
  CHECK(log.count(log::Level::Error) == 1);
  CHECK(log.contains(temp.file.string() + " cannot be read"));
  CHECK_FALSE(store.set("best", kBestScore));
  CHECK(store.get("best")->get<double>() == doctest::Approx(kBestScore));  // kept for this run
  CHECK(std::filesystem::is_directory(temp.file));
  CHECK_FALSE(std::filesystem::exists(beside(temp.file, kDamagedAside)));
}

TEST_CASE("a damaged save whose backup cannot be read is not saved over") {
  const TempSave temp;
  write_text(temp.file, kDamaged);
  std::filesystem::create_directories(beside(temp.file, kBackup));
  const test::LogCapture log;
  SaveFile store(temp.file);
  CHECK(store.keys().empty());  // (the save is read on first use)
  CHECK(log.contains(store.backup_file().string() + " cannot be read"));
  CHECK_FALSE(store.set("best", kBestScore));
  CHECK(std::filesystem::is_directory(store.backup_file()));
}

TEST_CASE("a damaged save that cannot be moved aside is never written over") {
  const TempSave temp;
  // Every name a damaged save can be moved to is taken.
  write_text(beside(temp.file, kDamagedAside), kAlsoDamaged);
  for (int number = kFirstNumberedAside; number <= kMostDamagedSavesKept; ++number) {
    write_text(beside(temp.file, std::string(kDamagedAside) + "." + std::to_string(number)), kAlsoDamaged);
  }
  write_text(temp.file, kDamaged);
  const test::LogCapture log;
  SaveFile store(temp.file);
  CHECK(store.keys().empty());
  CHECK(log.contains("could not be moved aside"));
  CHECK_FALSE(store.set("best", kBestScore));
  CHECK(contents_of(temp.file) == kDamaged);
  CHECK(contents_of(beside(temp.file, kDamagedAside)) == kAlsoDamaged);
}

TEST_CASE("a save removed for a fresh start takes its backup with it") {
  const TempSave temp;
  save_twice(temp.file, kOlderScore, kBestScore);
  remove_save(temp.file);
  CHECK(SaveFile(temp.file).keys().empty());
  CHECK_FALSE(std::filesystem::exists(beside(temp.file, kBackup)));
}

TEST_CASE("every save and every backup is flushed to the disk before it takes the old one's place") {
  const TempSave temp;
  int flushes = 0;
  const durable::Flush counting = [&flushes](std::FILE* file) {
    ++flushes;
    return durable::flush_to_disk(file);
  };
  SaveFile store(temp.file, counting);
  for (int write = 0; write < kWrites; ++write) REQUIRE(store.set("best", kBestScore + write));
  // The first save has nothing to back up; each one after it flushes the backup and the save.
  CHECK(flushes == 1 + 2 * (kWrites - 1));
  CHECK(best_in(temp.file) == doctest::Approx(kBestScore + kWrites - 1));
}

TEST_CASE("a save that cannot be flushed fails, and leaves the last good one and its backup as they were") {
  const TempSave temp;
  save_twice(temp.file, kOlderScore, kBestScore);
  SaveFile store(temp.file, [](std::FILE*) { return false; });
  CHECK_FALSE(store.set("best", kBestScore + kOlderScore));
  CHECK_FALSE(std::filesystem::exists(beside(temp.file, kPartial)));
  CHECK_FALSE(std::filesystem::exists(beside(temp.file, kBackupPartial)));
  CHECK(best_in(temp.file) == doctest::Approx(kBestScore));
  CHECK(best_in(store.backup_file()) == doctest::Approx(kOlderScore));
}

TEST_CASE("a file written flushed holds exactly the text, bytes as they are") {
  const TempSave temp;
  std::filesystem::create_directories(temp.dir);
  const std::string text = "{\n  \"best\": 1250\n}\n";
  REQUIRE(durable::write_flushed(temp.dir / "flushed.json", text));
  CHECK(contents_of(temp.dir / "flushed.json") == text);
  CHECK_FALSE(durable::write_flushed(temp.dir / "missing" / "flushed.json", text));
}

TEST_CASE("a file replaced whole leaves nothing beside it, and its folder is flushed to the disk") {
  const TempSave temp;
  std::filesystem::create_directories(temp.dir);
  const std::filesystem::path file = temp.dir / "replaced.json";
  write_text(file, kDamaged);
  REQUIRE(durable::replace_file(file, kAlsoDamaged));
  CHECK(contents_of(file) == kAlsoDamaged);
  CHECK_FALSE(std::filesystem::exists(beside(file, kPartial)));
  CHECK_FALSE(durable::replace_file(temp.dir / "missing" / "replaced.json", kAlsoDamaged));
  CHECK(durable::sync_folder(temp.dir));
  // (Windows has no flush of a folder: there it always succeeds.)
  const bool has_folder_flush = platform::this_desktop() != platform::Desktop::Windows;
  CHECK(durable::sync_folder(temp.dir / "missing") != has_folder_flush);
}
