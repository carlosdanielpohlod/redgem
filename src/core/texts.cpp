#include "core/texts.hpp"

#include <algorithm>
#include <optional>

#include <nlohmann/json.hpp>

#include "core/language_names.hpp"

namespace redgem {
namespace {

constexpr char kLineBreak = '\n';  // between the lines of a text written as a list of them
constexpr char kRegionMark = '_';  // where a language_key's region starts ("pt_br")

// Whether a value of a translation file is a list of lines.
bool is_lines(const nlohmann::json& value) {
  return value.is_array() &&
         std::all_of(value.begin(), value.end(), [](const nlohmann::json& line) { return line.is_string(); });
}

// A value of a translation file as the text it stands for; nothing when it is no text.
std::optional<std::string> text_of(const nlohmann::json& value) {
  if (value.is_string()) return value.get<std::string>();
  if (!is_lines(value)) return std::nullopt;
  std::string text;
  for (std::size_t line = 0; line < value.size(); ++line) {
    if (line > 0) text += kLineBreak;
    text += value[line].get_ref<const std::string&>();
  }
  return text;
}

// The texts of a translation file's object by key; nothing when one is no text, and `error` says which.
std::optional<TextCatalog> catalog_of(const nlohmann::json& file, std::string& error) {
  TextCatalog catalog;
  for (const auto& entry : file.items()) {
    std::optional<std::string> text = text_of(entry.value());
    if (!text) {
      error = "the value of " + entry.key() + " is neither a text nor a list of lines";
      return std::nullopt;
    }
    catalog.emplace(entry.key(), std::move(*text));
  }
  return catalog;
}

// The texts of a translation file by key; nothing when it is not one, and `error` says why.
std::optional<TextCatalog> read_catalog(std::string_view json, std::string& error) {
  const nlohmann::json file = nlohmann::json::parse(json.begin(), json.end(), nullptr, false);
  if (file.is_object()) return catalog_of(file, error);
  error = "a translation file is a JSON object of keys and texts";
  return std::nullopt;
}

}  // namespace

bool Texts::add(std::string_view language, std::string_view json, std::string& error) {
  std::optional<TextCatalog> catalog = read_catalog(json, error);
  if (!catalog) return false;
  TextCatalog& texts = languages_[language_key(language)];
  for (auto& [key, text] : *catalog) texts.insert_or_assign(key, std::move(text));
  return true;
}

std::vector<std::string> Texts::languages() const {
  std::vector<std::string> names;
  names.reserve(languages_.size());
  for (const auto& entry : languages_) names.push_back(entry.first);
  return names;
}

const std::string* Texts::find(std::string_view language, std::string_view key) const {
  if (const std::string* text = find_in(language, key)) return text;
  return find_in(language.substr(0, language.find(kRegionMark)), key);
}

const std::string* Texts::find_or_english(std::string_view language, std::string_view key) const {
  if (const std::string* text = find(language, key)) return text;
  return find_in(kFallbackLanguage, key);
}

std::vector<std::string> Texts::keys(std::string_view language) const {
  std::vector<std::string> keys;
  const auto catalog = languages_.find(language);
  if (catalog == languages_.end()) return keys;
  for (const auto& entry : catalog->second) keys.push_back(entry.first);
  return keys;
}

const std::string* Texts::find_in(std::string_view language, std::string_view key) const {
  const auto catalog = languages_.find(language);
  if (catalog == languages_.end()) return nullptr;
  const auto text = catalog->second.find(key);
  return text == catalog->second.end() ? nullptr : &text->second;
}

}  // namespace redgem
