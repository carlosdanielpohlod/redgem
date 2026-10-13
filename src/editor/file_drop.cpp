#include "editor/file_drop.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>

#include "core/config.hpp"
#include "core/safe_path.hpp"

namespace redgem {
namespace {

constexpr const char* kModelExtension = ".glb";
constexpr const char* kSoundExtensions[] = {".wav", ".ogg", ".mp3", ".flac"};
constexpr const char* kNotForAGame = ": not a .glb model or a sound (.wav, .ogg, .mp3, .flac)";
constexpr const char* kAlreadyThere = ": the game already has a file of that name";
constexpr const char* kNotCopied = ": could not be copied";

std::string lowered(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char letter) { return static_cast<char>(std::tolower(letter)); });
  return text;
}

const char* folder_of(DroppedKind kind) {
  return kind == DroppedKind::Model ? config::kModelsDirName : config::kSoundsDirName;
}

// Copies the file into `folder`; why not, when it does not.
const char* copy_into(const std::filesystem::path& folder, const std::filesystem::path& file) {
  std::error_code error;
  std::filesystem::create_directories(folder, error);
  const std::filesystem::path target = folder / file.filename();
  if (std::filesystem::exists(target, error)) return kAlreadyThere;
  return std::filesystem::copy_file(file, target, error) ? nullptr : kNotCopied;
}

void add_one(const std::filesystem::path& game_dir, const std::filesystem::path& file, DropReport& report) {
  const std::string name = printable(file.filename().string());
  const std::optional<DroppedKind> kind = kind_of_dropped(file);
  if (!kind) return report.refused.push_back(name + kNotForAGame);
  if (const char* problem = copy_into(game_dir / folder_of(*kind), file)) {
    return report.refused.push_back(name + problem);
  }
  (*kind == DroppedKind::Model ? report.models : report.sounds).push_back(name);
}

}  // namespace

std::optional<DroppedKind> kind_of_dropped(const std::filesystem::path& file) {
  const std::string extension = lowered(file.extension().string());
  if (extension == kModelExtension) return DroppedKind::Model;
  const bool sound = std::any_of(std::begin(kSoundExtensions), std::end(kSoundExtensions),
                                 [&extension](const char* known) { return extension == known; });
  return sound ? std::optional<DroppedKind>(DroppedKind::Sound) : std::nullopt;
}

DropReport add_dropped_files(const std::filesystem::path& game_dir, const std::vector<std::filesystem::path>& files) {
  DropReport report;
  for (const std::filesystem::path& file : files) add_one(game_dir, file, report);
  return report;
}

}  // namespace redgem
