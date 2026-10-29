#include "storage/save_slots.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace redgem {
namespace {
bool letter_or_digit(char value) { return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9'); }
bool name_character(char value) { return letter_or_digit(value) || value == '-' || value == '_'; }
}  // namespace
bool valid_save_slot(const std::string& name) {
  const bool sized = !name.empty() && name.size() <= kMaxSaveSlotName;
  return sized && letter_or_digit(name.front()) && std::all_of(name.begin(), name.end(), name_character);
}
void require_named_save_slot(const std::string& name) {
  if (!valid_save_slot(name) || name == kDefaultSaveSlot) throw std::invalid_argument("invalid named save slot");
}
std::filesystem::path save_slots_folder(const std::filesystem::path& file) {
  auto folder = file;
  folder += ".slots";
  return folder;
}
std::filesystem::path save_slot_file(const std::filesystem::path& file, const std::string& name) {
  require_named_save_slot(name);
  return save_slots_folder(file) / (std::string(kSaveSlotPrefix) + name + kSaveSlotExtension);
}
void SaveSlots::use_default(SaveStore& store) {
  if (default_ != nullptr && default_ != &store) throw std::logic_error("save slots cannot change their backend");
  default_ = &store;
}
SaveStore& SaveSlots::select(const std::string& name) {
  if (!valid_save_slot(name)) throw std::invalid_argument("invalid save slot name");
  if (default_ == nullptr) throw std::runtime_error("save slots have no default store");
  if (name == kDefaultSaveSlot) return *default_;
  const auto found = opened_.find(name);
  if (found != opened_.end()) return *found->second;
  return open_named(name);
}
SaveStore& SaveSlots::open_named(const std::string& name) {
  if (opened_.size() == kMaxOpenSaveSlots) throw std::runtime_error("too many save slots opened in this run");
  auto made = default_->open_slot(name);
  if (made == nullptr) throw std::runtime_error("this save backend does not support named slots");
  return *opened_.emplace(name, std::move(made)).first->second;
}
std::vector<std::string> SaveSlots::names() {
  if (default_ == nullptr) throw std::runtime_error("save slots have no default store");
  const auto stored = default_->slot_names();
  std::set<std::string> names{kDefaultSaveSlot};
  for (const auto& name : stored)
    if (valid_save_slot(name)) names.insert(name);
  for (const auto& entry : opened_) names.insert(entry.first);
  if (names.size() > kMaxListedSaveSlots) throw std::runtime_error("too many save slots to list");
  return {names.begin(), names.end()};
}
}  // namespace redgem
