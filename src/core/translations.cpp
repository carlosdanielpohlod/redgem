#include "core/translations.hpp"

#include <filesystem>
#include <stdexcept>

namespace redgem {
namespace translations {
namespace {

Texts& texts() {
  static Texts all;
  return all;
}

std::string& chosen_language() {
  static std::string language = kFallbackLanguage;
  return language;
}

}  // namespace

void add_file(std::string_view file_name, const EmbeddedFile& file) {
  const std::string language = std::filesystem::path(file_name).stem().string();
  const std::string_view json(reinterpret_cast<const char*>(file.data), file.size);
  std::string error;
  if (!texts().add(language, json, error)) {
    throw std::runtime_error("the translation file " + std::string(file_name) + " is broken: " + error);
  }
}

void use_language(std::string_view language) {
  chosen_language() = texts().has(language) ? std::string(language) : kFallbackLanguage;
}

const std::string& language() { return chosen_language(); }

const Texts& all() { return texts(); }

}  // namespace translations

const char* tr(const char* key) {
  const std::string* text = translations::all().find(translations::language(), key);
  return text != nullptr ? text->c_str() : key;
}

std::string tr(const char* key, const TextValues& values) { return fill_placeholders(tr(key), values); }

}  // namespace redgem
