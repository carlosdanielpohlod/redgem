#pragma once
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "storage/save_store.hpp"
namespace redgem {
inline constexpr std::string_view kSaveSlotPrefix = "slot-";
inline constexpr const char* kSaveSlotExtension = ".json";
inline constexpr const char* kDefaultSaveSlot = "default";
inline constexpr std::size_t kMaxSaveSlotName = 48;
inline constexpr std::size_t kMaxOpenSaveSlots = 64;
inline constexpr std::size_t kMaxListedSaveSlots = 4096;
bool valid_save_slot(const std::string& name);
void require_named_save_slot(const std::string& name);
void remove_named_save_files(const std::filesystem::path& file);
std::filesystem::path save_slot_file(const std::filesystem::path& file, const std::string& name);
std::filesystem::path save_slots_folder(const std::filesystem::path& file);
// One selector per engine, never shared between server rooms. The borrowed default store must
// outlive this object. Strong ownership retains in-memory slots and avoids stale file copies.
class SaveSlots {
 public:
  void use_default(SaveStore& store);
  SaveStore& select(const std::string& name);
  std::vector<std::string> names();

 private:
  SaveStore& open_named(const std::string& name);
  SaveStore* default_ = nullptr;
  std::map<std::string, std::unique_ptr<SaveStore>> opened_;
};
}  // namespace redgem
