#include "project/project_json_files.hpp"

#include <algorithm>
#include <optional>
#include <regex>
#include <stdexcept>

#include "core/files.hpp"

namespace redgem::tools::project_files {
namespace {

namespace fs = std::filesystem;
using mcp::Json;

constexpr int kIndent = 2;
const std::regex kFileName("[a-z0-9_-]+(/[a-z0-9_-]+)*");
const std::regex kLanguage("[a-z]{2,3}(-[A-Za-z0-9]{2,8})?");

std::optional<std::string> contents(const fs::path& file) {
  std::error_code missing;
  if (!fs::exists(file, missing)) return std::nullopt;
  std::string error;
  auto text = read_text_file(file, error);
  if (!text) throw std::runtime_error(error);
  return text;
}

void write_text(const fs::path& file, const std::string& text) {
  std::error_code ignored;
  fs::create_directories(file.parent_path(), ignored);
  std::string error;
  if (!write_text_file(file, text, error)) throw std::runtime_error(error);
}

// What the file was before a write: put back, or taken away when there was none.
void put_back(const fs::path& file, const std::optional<std::string>& before) {
  if (before) return write_text(file, *before);
  std::error_code ignored;
  fs::remove(file, ignored);
}

std::vector<std::string> new_ones(const std::vector<std::string>& before, const std::vector<std::string>& after) {
  std::vector<std::string> added;
  for (const auto& line : after) {
    if (std::find(before.begin(), before.end(), line) == before.end()) added.push_back(line);
  }
  return added;
}

std::string lines(const std::vector<std::string>& problems) {
  std::string text;
  for (const auto& problem : problems) text += (text.empty() ? "" : "\n") + problem;
  return text;
}

}  // namespace

Json read_object(const fs::path& file) {
  const auto text = contents(file);
  if (!text) return Json::object();
  Json value = Json::parse(*text, nullptr, false);
  if (!value.is_object()) throw std::invalid_argument(file.filename().string() + " is not a JSON object");
  return value;
}

void write_checked(const fs::path& file, const Json& value, const Problems& problems) {
  const auto before = contents(file);
  const std::vector<std::string> known = problems();
  write_text(file, value.dump(kIndent) + "\n");
  const std::vector<std::string> added = new_ones(known, problems());
  if (added.empty()) return;
  put_back(file, before);
  throw std::invalid_argument("not written, as it would make the game wrong:\n" + lines(added));
}

bool valid_file_name(const std::string& name) { return std::regex_match(name, kFileName); }

bool valid_language(const std::string& language) { return std::regex_match(language, kLanguage); }

}  // namespace redgem::tools::project_files
