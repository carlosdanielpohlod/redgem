#include "testing/test_saves.hpp"

#include <system_error>

#include "app/save_location.hpp"
#include "core/platform.hpp"

namespace redgem::testing {
namespace {

constexpr const char* kTestSavesFolder = "test-saves";  // beside the players' saves, in the user's own folder

}  // namespace

std::filesystem::path test_save_file_for(const std::filesystem::path& game_dir) {
  // (In the user's own folder, not in one every user of the machine can write to and guess.)
  return platform::user_data_dir() / kTestSavesFolder / save_file_for(game_dir).filename();
}

std::filesystem::path fresh_test_save_file(const std::filesystem::path& game_dir) {
  const std::filesystem::path file = test_save_file_for(game_dir);
  std::error_code ignored;
  std::filesystem::remove(file, ignored);
  return file;
}

}  // namespace redgem::testing
