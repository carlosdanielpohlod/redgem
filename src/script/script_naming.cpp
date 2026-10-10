#include "script/script_naming.hpp"

#include <cctype>

#include "core/config.hpp"

namespace redgem {
namespace {

std::string trim(const std::string& text) {
  std::size_t begin = 0;
  std::size_t end = text.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
  return text.substr(begin, end - begin);
}

}  // namespace

std::string class_name_for_script(const std::string& file_name) {
  std::string base = file_name;
  const std::size_t slash = base.find_last_of('/');
  if (slash != std::string::npos) base = base.substr(slash + 1);
  const std::string extension = config::kRubyFileExtension;
  if (base.size() > extension.size() && base.compare(base.size() - extension.size(), extension.size(), extension) == 0) {
    base.resize(base.size() - extension.size());
  }
  std::string name;
  bool upper_next = true;
  for (const char c : base) {
    if (c == '_' || c == '-' || c == ' ') {
      upper_next = true;
      continue;
    }
    name.push_back(upper_next ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c);
    upper_next = false;
  }
  return name;
}

std::string script_for_class_name(const std::string& class_name) {
  std::string file;
  for (const char c : class_name) {
    const auto letter = static_cast<unsigned char>(c);
    if (std::isupper(letter) && !file.empty()) file.push_back('_');
    file.push_back(static_cast<char>(std::tolower(letter)));
  }
  return file + config::kRubyFileExtension;
}

std::string scene_script_for(const std::string& scene_name) {
  return scene_name + config::kSceneScriptSuffix + config::kRubyFileExtension;
}

std::vector<std::string> split_script_list(const std::string& value) {
  std::vector<std::string> scripts;
  std::size_t start = 0;
  while (start <= value.size()) {
    const std::size_t comma = value.find(',', start);
    const std::size_t end = comma == std::string::npos ? value.size() : comma;
    std::string item = trim(value.substr(start, end - start));
    if (!item.empty()) scripts.push_back(std::move(item));
    if (comma == std::string::npos) break;
    start = comma + 1;
  }
  return scripts;
}

std::string join_script_list(const std::vector<std::string>& scripts) {
  constexpr const char* kSeparator = ", ";
  std::string joined;
  for (const std::string& script : scripts) joined += (joined.empty() ? "" : kSeparator) + script;
  return joined;
}

}  // namespace redgem
