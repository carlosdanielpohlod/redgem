#include "editor/file_drop.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>

#include "core/config.hpp"
#include "core/image_header.hpp"
#include "core/safe_path.hpp"
#include "scene/gltf/gltf_checks.hpp"
#include "editor/project_files.hpp"

namespace redgem {
namespace {

constexpr const char* kModelExtension = ".glb";
constexpr const char* kSoundExtensions[] = {".wav", ".ogg", ".mp3", ".flac"};
constexpr const char* kNotForAGame =
    ": not a .glb model, a sound (.wav, .ogg, .mp3, .flac) or a picture (.png, .jpg, .jpeg, .webp)";
constexpr const char* kAlreadyThere = ": the game already has a file (or a link) of that name";
constexpr const char* kNotCopied = ": could not be copied";
constexpr const char* kLinkedFolder = ": the game's folder for it is a link, which a game may not use";
constexpr const char* kNotAPicture = ": not a picture the engine can show (a PNG, a JPEG or a still WebP)";
constexpr const char* kTooBig = ": too big for a picture, or unreadable";

std::string lowered(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char letter) { return static_cast<char>(std::tolower(letter)); });
  return text;
}

const char* folder_of(DroppedKind kind) {
  if (kind == DroppedKind::Image) return config::kImagesDirName;
  return kind == DroppedKind::Model ? config::kModelsDirName : config::kSoundsDirName;
}

std::vector<std::string>& list_of(DroppedKind kind, DropReport& report) {
  if (kind == DroppedKind::Image) return report.images;
  return kind == DroppedKind::Model ? report.models : report.sounds;
}

template <std::size_t Count>
bool one_of(const std::string& extension, const char* const (&known)[Count]) {
  return std::any_of(std::begin(known), std::end(known), [&extension](const char* one) { return extension == one; });
}

// Whether something is at `path`, whatever it is: a link counts, even one pointing nowhere.
bool taken(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::symlink_status(path, error).type() != std::filesystem::file_type::not_found;
}

// Copies the file into `folder`, which stays inside the game (it is no link); why not, when it
// does not.
const char* copy_into(const std::filesystem::path& folder, const std::filesystem::path& file) {
  std::error_code error;
  std::filesystem::create_directories(folder, error);
  if (std::filesystem::is_symlink(folder, error)) return kLinkedFolder;
  const std::filesystem::path target = folder / file.filename();
  if (taken(target)) return kAlreadyThere;
  return std::filesystem::copy_file(file, target, error) ? nullptr : kNotCopied;
}

// What keeps a dropped picture out of the game: too big, or not a picture the engine shows.
// nullptr when nothing does.
const char* picture_problem(const std::filesystem::path& file) {
  const std::optional<std::vector<std::uint8_t>> bytes = read_file_up_to(file, gltf_checks::kMaxImageFileBytes);
  if (!bytes) return kTooBig;
  return decodable_image(bytes->data(), bytes->size()) ? nullptr : kNotAPicture;
}

// What keeps a dropped file of `kind` out of the game, before it is copied; nullptr for nothing.
const char* content_problem(DroppedKind kind, const std::filesystem::path& file) {
  return kind == DroppedKind::Image ? picture_problem(file) : nullptr;
}

void add_one(const std::filesystem::path& game_dir, const std::filesystem::path& file, DropReport& report) {
  const std::string name = printable(file.filename().string());
  const std::optional<DroppedKind> kind = kind_of_dropped(file);
  if (!kind) return report.refused.push_back(name + kNotForAGame);
  if (const char* problem = content_problem(*kind, file)) return report.refused.push_back(name + problem);
  if (const char* problem = copy_into(game_dir / folder_of(*kind), file)) {
    return report.refused.push_back(name + problem);
  }
  list_of(*kind, report).push_back(name);
}

}  // namespace

std::optional<DroppedKind> kind_of_dropped(const std::filesystem::path& file) {
  const std::string extension = lowered(file.extension().string());
  if (extension == kModelExtension) return DroppedKind::Model;
  if (is_picture_file(file)) return DroppedKind::Image;
  return one_of(extension, kSoundExtensions) ? std::optional<DroppedKind>(DroppedKind::Sound) : std::nullopt;
}

DropReport add_dropped_files(const std::filesystem::path& game_dir, const std::vector<std::filesystem::path>& files) {
  DropReport report;
  for (const std::filesystem::path& file : files) add_one(game_dir, file, report);
  return report;
}

}  // namespace redgem
