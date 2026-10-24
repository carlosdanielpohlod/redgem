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

namespace {

// The save file of one test run in a folder of its own (see fresh_save_file_in), as it is.
std::filesystem::path save_file_in(const std::filesystem::path& work_dir, const std::filesystem::path& test_file) {
  return work_dir / kWorkSavesFolder / (test_file.stem().string() + kSaveExtension);
}

// A file of one test of one game, named as the game's save is with the test's name and `ending`
// (see fresh_settings_file_for), as it is.
std::filesystem::path test_file_for(const std::filesystem::path& game_dir, const std::filesystem::path& work_dir,
                                    const std::filesystem::path& test_file, const char* ending) {
  const std::filesystem::path game_save = test_save_file_for(game_dir);
  const std::filesystem::path folder = work_dir.empty() ? game_save.parent_path() : work_dir / kWorkSavesFolder;
  return folder / (game_save.stem().string() + kNameJoiner + test_file.stem().string() + ending);
}

// The same file, emptied.
std::filesystem::path fresh_test_file(const std::filesystem::path& game_dir, const std::filesystem::path& work_dir,
                                      const std::filesystem::path& test_file, const char* ending) {
  const std::filesystem::path file = test_file_for(game_dir, work_dir, test_file, ending);
  std::error_code error;
  std::filesystem::remove(file, error);
  return file;
}

}  // namespace

std::filesystem::path fresh_save_file_in(const std::filesystem::path& work_dir,
                                         const std::filesystem::path& test_file) {
  return emptied(save_file_in(work_dir, test_file));
}

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

RunFiles fresh_run_files(const std::filesystem::path& game_dir, const std::filesystem::path& work_dir,
                         const std::filesystem::path& test_file) {
  const std::filesystem::path save =
      work_dir.empty() ? fresh_test_save_file(game_dir) : fresh_save_file_in(work_dir, test_file);
  return {save, fresh_settings_file_for(game_dir, work_dir, test_file),
          fresh_bindings_file_for(game_dir, work_dir, test_file)};
}

RunFiles kept_run_files(const std::filesystem::path& game_dir, const std::filesystem::path& work_dir,
                        const std::filesystem::path& earlier_test) {
  return {save_file_in(work_dir, earlier_test), test_file_for(game_dir, work_dir, earlier_test, kSettingsEnding),
          test_file_for(game_dir, work_dir, earlier_test, kBindingsEnding)};
}

}  // namespace redgem::testing
