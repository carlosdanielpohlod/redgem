#include "testing/test_saves.hpp"

#include <system_error>

#include "app/save_location.hpp"
#include "core/platform.hpp"
#include "storage/save_file.hpp"

namespace redgem::testing {
namespace {

constexpr const char* kTestSavesFolder = "test-saves";  // beside the players' saves, in the user's own folder
constexpr const char* kWorkSavesFolder = "saves";       // in a run's own folder
constexpr const char* kSaveExtension = ".json";
constexpr const char* kSettingsEnding = "-settings.json";  // after the game's and the test's names
constexpr const char* kBindingsEnding = "-bindings.json";
constexpr char kNameJoiner = '-';

// (Its backup too: a missing save is otherwise loaded from it.)
std::filesystem::path emptied(const std::filesystem::path& file) {
  remove_save(file);
  return file;
}

}  // namespace

std::filesystem::path test_save_file_for(const std::filesystem::path& game_dir) {
  // (In the user's own folder, not in one every user of the machine can write to and guess.)
  // Laid out as the players' saves are: a game with an id and one without never share a file.
  return platform::user_data_dir() / kTestSavesFolder / save_file_for(game_dir).lexically_relative(saves_folder());
}

std::filesystem::path fresh_test_save_file(const std::filesystem::path& game_dir) {
  return emptied(test_save_file_for(game_dir));
}

std::filesystem::path fresh_save_file_in(const std::filesystem::path& work_dir,
                                         const std::filesystem::path& test_file) {
  return emptied(work_dir / kWorkSavesFolder / (test_file.stem().string() + kSaveExtension));
}

namespace {

// A file of one test of one game, named as the game's save is with the test's name and `ending`,
// emptied (see fresh_settings_file_for).
std::filesystem::path fresh_test_file(const std::filesystem::path& game_dir, const std::filesystem::path& work_dir,
                                      const std::filesystem::path& test_file, const char* ending) {
  const std::filesystem::path game_save = test_save_file_for(game_dir);
  const std::filesystem::path folder = work_dir.empty() ? game_save.parent_path() : work_dir / kWorkSavesFolder;
  const std::string name = game_save.stem().string() + kNameJoiner + test_file.stem().string() + ending;
  const std::filesystem::path file = folder / name;
  std::error_code error;
  std::filesystem::remove(file, error);
  return file;
}

}  // namespace

std::filesystem::path fresh_settings_file_for(const std::filesystem::path& game_dir,
                                              const std::filesystem::path& work_dir,
                                              const std::filesystem::path& test_file) {
  return fresh_test_file(game_dir, work_dir, test_file, kSettingsEnding);
}

std::filesystem::path fresh_bindings_file_for(const std::filesystem::path& game_dir,
                                              const std::filesystem::path& work_dir,
                                              const std::filesystem::path& test_file) {
  return fresh_test_file(game_dir, work_dir, test_file, kBindingsEnding);
}

}  // namespace redgem::testing
