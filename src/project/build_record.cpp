#include "project/build_record.hpp"

#include <fstream>

#include "core/files.hpp"
#include "core/translations.hpp"

namespace redgem {

namespace {

constexpr const char* kRecordFile = ".redgem-build";
constexpr const char* kRecordHeading = "# Made by redgem build: the next build into this folder replaces these.";
constexpr char kCommentMark = '#';

bool is_entry(const std::string& line) { return !line.empty() && line.front() != kCommentMark; }

}  // namespace

BuildRecord BuildRecord::of(const std::filesystem::path& out_dir) {
  BuildRecord record(out_dir);
  std::ifstream in(out_dir / kRecordFile);
  for (std::string line; std::getline(in, line);) {
    if (is_entry(line)) record.outputs_.insert(line);
  }
  return record;
}

bool BuildRecord::takes(const std::string& output) const {
  std::error_code fs_error;
  const bool absent = !std::filesystem::exists(out_dir_ / output, fs_error) && !fs_error;
  return absent || outputs_.count(output) != 0;
}

bool BuildRecord::may_write(const std::vector<std::string>& outputs, std::string& error) const {
  for (const std::string& output : outputs) {
    if (takes(output)) continue;
    error = tr("project.build.not_a_build", {{"path", (out_dir_ / output).string()}});
    return false;
  }
  return true;
}

bool BuildRecord::keep(const std::vector<std::string>& outputs, std::string& error) {
  outputs_.insert(outputs.begin(), outputs.end());
  std::string text = std::string(kRecordHeading) + "\n";
  for (const std::string& output : outputs_) text += output + "\n";
  return write_text_file(out_dir_ / kRecordFile, text, error);
}

}  // namespace redgem
