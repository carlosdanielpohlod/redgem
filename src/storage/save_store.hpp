#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace redgem {

// What a game keeps between runs (Save in Ruby): named values (numbers, texts, true/false, and
// arrays and hashes of them). Whole numbers come back as whole numbers, however they were written
// (a count of 3 is 3, not 3.0). A player's machine keeps them in a file (SaveFile), a dedicated
// server in a database (server/save_database.hpp).
class SaveStore {
 public:
  // What an update makes of a key's value (nullopt: it has none); null removes it.
  using Change = std::function<nlohmann::json(const std::optional<nlohmann::json>& current)>;

  SaveStore() = default;
  virtual ~SaveStore() = default;
  SaveStore(const SaveStore&) = delete;
  SaveStore& operator=(const SaveStore&) = delete;

  virtual std::optional<nlohmann::json> get(const std::string& key) = 0;
  // False when it could not be written. A null value is none: the key is removed.
  virtual bool set(const std::string& key, const nlohmann::json& value) = 0;
  virtual bool remove(const std::string& key) = 0;
  virtual bool clear() = 0;
  // In the order of their names.
  virtual std::vector<std::string> keys() = 0;
  // Reads a key, makes a new value of it and writes that, with nothing else read or written in
  // between: on a dedicated server, no other room's change to the key gets lost. What `change`
  // throws leaves the key as it was. False when it could not be written.
  virtual bool update(const std::string& key, const Change& change) = 0;
  // Where the values are kept, for what is said when they cannot be written.
  virtual std::string where() const = 0;
};

}  // namespace redgem
