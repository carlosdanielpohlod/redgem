#include "testing/test_saves.hpp"

#include <system_error>

#include "app/save_location.hpp"
#include "core/platform.hpp"

namespace redgem::testing {
namespace {

constexpr const char* kTestSavesFolder = "test-saves";  // beside the players' saves, in the user's own folder
constexpr const char* kWorkSavesFolder = "saves";       // in a run's own folder
constexpr const char* kSaveExtension = ".json";

std::filesystem::path emptied(const std::filesystem::path& file) {
  std::error_code ignored;
  std::filesystem::remove(file, ignored);
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

}  // namespace redgem::testing
