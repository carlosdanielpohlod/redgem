#pragma once
#include "storage/save_store.hpp"
namespace redgem {
// A non-owning view: the backend owns/caches its slot data and must outlive every view.
class SaveView final : public SaveStore {
 public:
  explicit SaveView(SaveStore& store) : store_(store) {}
  std::optional<nlohmann::json> get(const std::string& key) override { return store_.get(key); }
  bool set(const std::string& key, const nlohmann::json& value) override { return store_.set(key, value); }
  bool remove(const std::string& key) override { return store_.remove(key); }
  bool clear() override { return store_.clear(); }
  std::vector<std::string> keys() override { return store_.keys(); }
  bool update(const std::string& key, const Change& change) override { return store_.update(key, change); }
  std::string where() const override { return store_.where(); }

 private:
  SaveStore& store_;
};
}  // namespace redgem
