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

std::string without_directory(const std::string& path) {
  const std::size_t slash = path.find_last_of('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string without_ruby_extension(const std::string& file_name) {
  const std::string extension = config::kRubyFileExtension;
  const bool longer = file_name.size() > extension.size();
  const bool ends_with_it =
      longer && file_name.compare(file_name.size() - extension.size(), extension.size(), extension) == 0;
  return ends_with_it ? file_name.substr(0, file_name.size() - extension.size()) : file_name;
}

bool separates_words(char c) { return c == '_' || c == '-' || c == ' '; }

// "camera_follow" -> "CameraFollow".
std::string camel_case(const std::string& words) {
  std::string name;
  bool upper_next = true;
  for (const char c : words) {
    if (!separates_words(c)) {
      name.push_back(upper_next ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c);
    }
    upper_next = separates_words(c);
  }
  return name;
}

}  // namespace

std::string class_name_for_script(const std::string& file_name) {
  return camel_case(without_ruby_extension(without_directory(file_name)));
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
