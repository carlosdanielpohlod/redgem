#include "project/scene_diff.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "core/safe_path.hpp"
#include "core/short_text.hpp"
#include "core/translations.hpp"
#include "project/scene_snapshot.hpp"
#include "project/scene_text.hpp"
#include "scene/scene_ids.hpp"
#include "scene/gltf/gltf_loader.hpp"

namespace redgem::tools {
namespace {

using snapshot::Fields;
using snapshot::Json;
using snapshot::SceneView;
using Index = std::map<std::string, int>;
constexpr std::size_t kMaxLabelBytes = 96;
constexpr std::size_t kMaxValueBytes = 256;
constexpr std::size_t kQuaternionFields = 4;
constexpr int kDiffDecimals = 6;

struct FieldChange {
  std::string field;
  const Json* before = nullptr;
  const Json* after = nullptr;
};
using Changes = std::vector<FieldChange>;

std::string short_text(std::string_view text, std::size_t limit) {
  const auto prefix = utf8_prefix(text, limit);
  return printable(prefix) + (prefix.size() < text.size() ? "..." : "");
}

Index object_index(const SceneDesc& scene) {
  Index ids;
  for (int i = 0; i < static_cast<int>(scene.nodes.size()); ++i) {
    if (!ids.emplace(scene_node_id(scene, i), i).second)
      throw std::invalid_argument("scene has duplicate object identities");
  }
  return ids;
}

std::string header(const char* mark, const SceneDesc& scene, int index) {
  return std::string(mark) + " " + object_id(scene, index) + " " + short_text(scene.nodes[index].name, kMaxLabelBytes);
}

std::set<std::string> field_names(const Json& a, const Json& b) {
  std::set<std::string> names;
  for (const auto& item : a.items()) names.insert(item.key());
  for (const auto& item : b.items()) names.insert(item.key());
  return names;
}

const Json* member(const Json& value, const std::string& name) {
  const auto found = value.find(name);
  return found == value.end() ? nullptr : &*found;
}

bool numeric_vector(const Json& value) {
  return std::all_of(value.begin(), value.end(), [](const Json& component) { return component.is_number(); });
}

void collect_field(const std::string& field, const Json* a, const Json* b, Changes& changes);

void collect_object(const std::string& field, const Json& a, const Json& b, Changes& changes) {
  for (const auto& name : field_names(a, b)) {
    std::string nested = field;
    nested += ".";
    nested += name;
    collect_field(nested, member(a, name), member(b, name), changes);
  }
}

void collect_array(const std::string& field, const Json& a, const Json& b, Changes& changes) {
  for (std::size_t i = 0; i < std::max(a.size(), b.size()); ++i) {
    collect_field(field + "[" + std::to_string(i) + "]", i < a.size() ? &a[i] : nullptr, i < b.size() ? &b[i] : nullptr,
                  changes);
  }
}

enum class Structure { Leaf, Object, Array };

Structure structure_of(const Json* a, const Json* b) {
  if (a == nullptr || b == nullptr) return Structure::Leaf;
  if (a->is_object() && b->is_object()) return Structure::Object;
  if (!a->is_array() || !b->is_array()) return Structure::Leaf;
  const bool numbers = numeric_vector(*a) && numeric_vector(*b);
  return numbers ? Structure::Leaf : Structure::Array;
}

void collect_field(const std::string& field, const Json* a, const Json* b, Changes& changes) {
  if (a != nullptr && b != nullptr && snapshot::same_field(field, *a, *b)) return;
  switch (structure_of(a, b)) {
    case Structure::Object:
      return collect_object(field, *a, *b, changes);
    case Structure::Array:
      return collect_array(field, *a, *b, changes);
    case Structure::Leaf:
      changes.push_back({field, a, b});
  }
}

Changes changes_between(const Fields& before, const Fields& after) {
  std::set<std::string> names;
  for (const auto& item : before) names.insert(item.first);
  for (const auto& item : after) names.insert(item.first);
  Changes changes;
  for (const auto& name : names) {
    const auto a = before.find(name), b = after.find(name);
    collect_field(name, a == before.end() ? nullptr : &a->second, b == after.end() ? nullptr : &b->second, changes);
  }
  return changes;
}

std::string rotation_text(const Json& value) {
  const auto [x, y, z, w] = value.get<std::array<float, kQuaternionFields>>();
  const Quat rotation{x, y, z, w};
  return vector_text(quat_to_euler_degrees(rotation), kDetailDecimals);
}

std::string numeric_array(const Json& value) {
  std::string text = "[";
  for (const auto& component : value) {
    if (text.size() > 1) text += ",";
    text += number_text(component.get<double>(), kDiffDecimals);
  }
  return text + "]";
}

std::string display_value(const std::string& field, const Json& value) {
  if (!snapshot::property_field(field)) {
    if (value.is_number_float()) return number_text(value.get<double>(), kDiffDecimals);
    if (value.is_array() && numeric_vector(value)) return numeric_array(value);
  }
  return value.dump(-1, ' ', false, Json::error_handler_t::replace);
}

std::string field_text(const std::string& field, const Json* value) {
  if (value == nullptr) return tr("project.scene.diff_absent");
  const bool rotation = field == "rotation" && value->is_array() && value->size() == kQuaternionFields;
  if (rotation) return rotation_text(*value);
  return short_text(display_value(field, *value), kMaxValueBytes);
}

class Lines {
 public:
  explicit Lines(std::size_t limit) : limit_(limit) {}
  void add(const std::string& line) {
    if (limit_ != 0 && shown_ == limit_) {
      ++omitted_;
      return;
    }
    text_ += line + "\n";
    ++shown_;
  }
  std::string text() const {
    if (omitted_ == 0) return text_;
    return text_ + tr("project.scene.diff_more", {{"count", std::to_string(omitted_)}}) + "\n";
  }

 private:
  std::size_t limit_;
  std::size_t shown_ = 0;
  std::size_t omitted_ = 0;
  std::string text_;
};

class Difference {
 public:
  Difference(const SceneDesc& before, const SceneDesc& after, std::string name, std::size_t limit)
      : before_(before),
        after_(after),
        before_ids_(object_index(before)),
        after_ids_(object_index(after)),
        before_view_(before),
        after_view_(after),
        name_(std::move(name)),
        lines_(limit) {}

  std::string run() {
    remove_missing();
    add_or_compare();
    const auto summary = tr("project.scene.diff_summary", {{"scene", name_},
                                                           {"added", std::to_string(added_)},
                                                           {"removed", std::to_string(removed_)},
                                                           {"changed", std::to_string(changed_)}});
    const bool unchanged = added_ == 0 && removed_ == 0 && changed_ == 0;
    return summary + "\n" + (unchanged ? std::string(tr("project.scene.diff_none")) + "\n" : lines_.text());
  }

 private:
  void remove_missing() {
    for (const auto& [id, index] : before_ids_) {
      if (after_ids_.count(id) != 0) continue;
      ++removed_;
      lines_.add(header("-", before_, index));
    }
  }
  void add_or_compare() {
    for (const auto& [id, index] : after_ids_) {
      const auto before = before_ids_.find(id);
      if (before != before_ids_.end()) {
        compare(before->second, index);
      } else {
        ++added_;
        lines_.add(header("+", after_, index));
      }
    }
  }
  void compare(int before, int after) {
    const Fields a = before_view_.fields(before), b = after_view_.fields(after);
    const Changes changes = changes_between(a, b);
    if (changes.empty()) return;
    ++changed_;
    lines_.add(header("~", after_, after));
    for (const FieldChange& change : changes) {
      lines_.add("  " + short_text(change.field, kMaxLabelBytes) + ": " + field_text(change.field, change.before) +
                 " -> " + field_text(change.field, change.after));
    }
  }

  const SceneDesc& before_;
  const SceneDesc& after_;
  Index before_ids_, after_ids_;
  SceneView before_view_, after_view_;
  std::string name_;
  Lines lines_;
  std::size_t added_ = 0, removed_ = 0, changed_ = 0;
};

}  // namespace

std::string scene_difference(const SceneDesc& before, const SceneDesc& after, const std::string& name,
                             std::size_t limit) {
  return Difference(before, after, name, limit).run();
}

ToolResult compare_scene_file(const SceneDesc& current, const std::string& name, const std::filesystem::path& baseline,
                              std::size_t limit) {
  try {
    const LoadResult before = load_gltf_scene(baseline.string());
    if (!before.ok) return failed(before.error);
    ToolResult result;
    result.out = scene_difference(before.scene, current, name, limit);
    for (const auto& warning : before.warnings) result.err += "warning: " + printable(warning) + "\n";
    return result;
  } catch (const std::exception& problem) {
    return failed(tr("project.scene.diff_failed", {{"problem", problem.what()}}));
  }
}

}  // namespace redgem::tools
