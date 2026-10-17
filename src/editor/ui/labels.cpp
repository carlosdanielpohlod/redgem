#include "editor/ui/labels.hpp"

#include <map>
#include <string>
#include <string_view>
#include <tuple>

#include "core/translations.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {

constexpr const char* kAfterIcon = "  ";  // between an icon and its text
constexpr const char* kIdMark = "##";     // what ImGui leaves out of a label: its id follows
constexpr const char* kNone = "";         // no icon, or no id

// What a label is made of: its icon, its text's key and its id (each but the key may be empty).
using Parts = std::tuple<std::string_view, std::string_view, std::string_view>;
struct KeptParts {
  std::string icon;
  std::string key;
  std::string id;
};

Parts parts_of(const KeptParts& kept) { return {kept.icon, kept.key, kept.id}; }
Parts parts_of(const Parts& parts) { return parts; }

// Orders labels by their parts, so that one is found without making a string to look for.
struct ByParts {
  using is_transparent = void;
  template <typename Left, typename Right>
  bool operator()(const Left& left, const Right& right) const {
    return parts_of(left) < parts_of(right);
  }
};

std::string made_label(std::string_view icon, const char* key, std::string_view id) {
  std::string label = icon.empty() ? std::string() : std::string(icon) + kAfterIcon;
  label += tr(key);
  if (!id.empty()) label += std::string(kIdMark) + std::string(id);
  return label;
}

// The label of these parts, made the first time it is asked for.
const char* kept_label(std::string_view icon, const char* key, std::string_view id) {
  static std::map<KeptParts, std::string, ByParts> labels;
  auto label = labels.find(Parts{icon, key, id});
  if (label == labels.end()) {
    KeptParts parts{std::string(icon), key, std::string(id)};
    label = labels.emplace(std::move(parts), made_label(icon, key, id)).first;
  }
  return label->second.c_str();
}

}  // namespace

const char* icon_label(const char* icon, const char* key) { return kept_label(icon, key, kNone); }

const char* label_with_id(const char* key, const char* id) { return kept_label(kNone, key, id); }

const char* icon_label_with_id(const char* icon, const char* key, const char* id) { return kept_label(icon, key, id); }

const char* window_title(const char* key, const char* window) {
  static std::map<std::string, std::string, std::less<>> titles;
  auto title = titles.find(std::string_view(window));
  if (title == titles.end()) title = titles.emplace(window, tr(key) + std::string(window)).first;
  return title->second.c_str();
}

const char* save_label() { return icon_label(ICON_FA_FLOPPY_DISK, "editor.command.save"); }
const char* discard_label() { return tr("editor.command.discard"); }
const char* cancel_label() { return tr("editor.command.cancel"); }
const char* duplicate_label() { return icon_label(ICON_FA_CLONE, "editor.command.duplicate"); }
const char* delete_label() { return icon_label(ICON_FA_TRASH, "editor.command.delete"); }

}  // namespace redgem::ui
