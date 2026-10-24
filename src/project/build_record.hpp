#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace redgem {

// What builds wrote into an output folder, kept in a file there (<out>/.redgem-build, one name a
// line): the files and folders the next build may replace. Whatever else is in the folder is
// someone else's, and a build never writes over it.
class BuildRecord {
 public:
  // The record of `out_dir`; an empty one where no build wrote anything yet.
  static BuildRecord of(const std::filesystem::path& out_dir);

  // Whether a build may write each of `outputs` (names in `out_dir`): nothing is there yet, or a
  // build made it. False, with `error` naming the first that is someone else's, otherwise.
  bool may_write(const std::vector<std::string>& outputs, std::string& error) const;
  // Adds `outputs` to the record, and writes it.
  bool keep(const std::vector<std::string>& outputs, std::string& error);

 private:
  explicit BuildRecord(std::filesystem::path out_dir) : out_dir_(std::move(out_dir)) {}
  bool takes(const std::string& output) const;

  std::filesystem::path out_dir_;
  std::set<std::string> outputs_;
};

}  // namespace redgem
