#pragma once

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// What users read, kept out of the code in translation files: one per language, a JSON object of
// keys (editor.menu.file) and the texts they stand for (File). A text that has values in
// it names them, "Created {game}", so that a translation may put them where its language wants.
namespace redgem {

// The language every text is written in first, and the one a missing translation falls back to.
inline constexpr const char* kFallbackLanguage = "en";

// A value put into a text in the place of its placeholder: {"game", "Racer"} fills "{game}".
struct TextValue {
  std::string name;
  std::string value;
};
using TextValues = std::vector<TextValue>;

// The texts of one language, by key.
using TextCatalog = std::map<std::string, std::string, std::less<>>;

// The texts of translation files, by language and key.
class Texts {
 public:
  // Adds the texts of a translation file in `language` ("en", "pt_BR"): a JSON object whose
  // values are texts, or lists of lines (a long text, one line each). Nothing is added from a file
  // that is anything else, and `error` says why.
  bool add(std::string_view language, std::string_view json, std::string& error);

  // The languages some file gave texts in, by their language_key (see core/language_names.hpp).
  std::vector<std::string> languages() const;
  // The text of `key` in `language` (a language_key, which costs nothing to look up with: "pt_br"),
  // else in that language without its region ("pt"); nullptr when neither has it. It stays where
  // it is until texts are added again.
  const std::string* find(std::string_view language, std::string_view key) const;
  // The same, else in English.
  const std::string* find_or_english(std::string_view language, std::string_view key) const;
  // Every key `language` (a language_key) has, in order.
  std::vector<std::string> keys(std::string_view language) const;

 private:
  // The text of `key` in `language` alone.
  const std::string* find_in(std::string_view language, std::string_view key) const;

  std::map<std::string, TextCatalog, std::less<>> languages_;
};

// `text` with each of its {name} placeholders replaced by the value of that name. A placeholder
// without a value stays as it is, so that a missing one shows.
std::string fill_placeholders(std::string_view text, const TextValues& values);

}  // namespace redgem
