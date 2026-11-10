#include "testing/test_saves.hpp"

#include <fstream>
#include <system_error>

#include "app/game_data_location.hpp"
#include "core/platform.hpp"
#include "storage/save_file.hpp"
#include "storage/save_slots.hpp"

namespace redgem::testing {
namespace {

constexpr const char* kTestSavesFolder = "test-saves";  // in the user's own RedGem folder
constexpr const char* kWorkSavesFolder = "saves";       // in a run's own folder
constexpr const char* kSaveExtension = ".json";
constexpr const char* kSettingsEnding = "-settings.json";  // after the test's name
constexpr const char* kBindingsEnding = "-bindings.json";
constexpr const char* kRunsFolder = "runs";  // in a run's own folder: a word from each test run there
constexpr const char* kRunMarkEnding = ".ran";

// (Its backup too: a missing save is otherwise loaded from it.)
std::filesystem::path emptied(const std::filesystem::path& file) {
  remove_save(file);
  return file;
}

}  // namespace

std::filesystem::path test_data_folder(const std::filesystem::path& game_dir) {
  // (In the user's own folder, not in one every user of the machine can write to and guess.)
  return platform::user_data_dir() / kTestSavesFolder / game_data_subfolder(game_dir);
}

std::filesystem::path test_save_file_for(const std::filesystem::path& game_dir) {
  return game_data_files(test_data_folder(game_dir)).save;
}

std::filesystem::path fresh_test_save_file(const std::filesystem::path& game_dir) {
  return emptied(test_save_file_for(game_dir));
}

namespace {

// The save file of one test run in a folder of its own (see fresh_save_file_in), as it is.
std::filesystem::path save_file_in(const std::filesystem::path& work_dir, const std::filesystem::path& test_file) {
  return work_dir / kWorkSavesFolder / (test_file.stem().string() + kSaveExtension);
}

// A file of one test of one game, named after the test with `ending` (see fresh_settings_file_for),
// as it is.
std::filesystem::path test_file_for(const std::filesystem::path& game_dir, const std::filesystem::path& work_dir,
                                    const std::filesystem::path& test_file, const char* ending) {
  const std::filesystem::path folder = work_dir.empty() ? test_data_folder(game_dir) : work_dir / kWorkSavesFolder;
  return folder / (test_file.stem().string() + ending);
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

namespace {

// A copy of a file, when there is one to copy (a run may have kept nothing in it).
void copy_if_kept(const std::filesystem::path& from, const std::filesystem::path& to) {
  std::error_code error;
  if (!std::filesystem::exists(from, error)) return;
  std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, error);
}

void copy_slot_file(const std::filesystem::path& from, const std::filesystem::path& to) {
  if (!std::filesystem::is_symlink(from)) copy_if_kept(from, to);
}
void copy_slots(const std::filesystem::path& from, const std::filesystem::path& to) {
  SaveFile source(from);
  const auto names = source.slot_names();
  if (names.empty()) return;
  std::filesystem::create_directories(save_slots_folder(to));
  for (const auto& name : names) {
    const auto original = save_slot_file(from, name);
    const auto copied = save_slot_file(to, name);
    copy_slot_file(original, copied);
    copy_slot_file(save_backup_of(original), save_backup_of(copied));
  }
}

std::filesystem::path run_mark(const std::filesystem::path& work_dir, const std::filesystem::path& test_file) {
  return work_dir / kRunsFolder / (test_file.stem().string() + kRunMarkEnding);
}

}  // namespace

RunFiles copied_run_files(const std::filesystem::path& game_dir, const std::filesystem::path& work_dir,
                          const std::filesystem::path& test_file, const std::filesystem::path& earlier_test) {
  const RunFiles own = fresh_run_files(game_dir, work_dir, test_file);
  const RunFiles kept = kept_run_files(game_dir, work_dir, earlier_test);
  copy_if_kept(kept.save, own.save);
  copy_if_kept(save_backup_of(kept.save), save_backup_of(own.save));
  copy_slots(kept.save, own.save);
  copy_if_kept(kept.settings, own.settings);
  copy_if_kept(kept.bindings, own.bindings);
  return own;
}

void mark_run(const std::filesystem::path& work_dir, const std::filesystem::path& test_file) {
  const std::filesystem::path mark = run_mark(work_dir, test_file);
  std::error_code error;
  std::filesystem::create_directories(mark.parent_path(), error);
  std::ofstream(mark) << test_file.string() << '\n';
}

bool ran_in(const std::filesystem::path& work_dir, const std::filesystem::path& test_file) {
  std::error_code error;
  return std::filesystem::exists(run_mark(work_dir, test_file), error);
}

}  // namespace redgem::testing
