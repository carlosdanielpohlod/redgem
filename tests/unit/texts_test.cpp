#include <doctest/doctest.h>

#include <string>

#include "core/platform.hpp"
#include "core/texts.hpp"
#include "core/translations.hpp"

using namespace redgem;

namespace {

constexpr const char* kEnglish = R"({"menu.save": "Save", "menu.quit": "Quit", "hub.created": "Created {game}"})";
constexpr const char* kPortuguese = R"({"menu.save": "Salvar"})";

// Texts in English and, for some keys, in Portuguese.
Texts two_languages() {
  Texts texts;
  std::string error;
  REQUIRE(texts.add("en", kEnglish, error));
  REQUIRE(texts.add("pt", kPortuguese, error));
  return texts;
}

// What a text of `json` (in a language of its own) is under "key"; "missing" when it has none.
std::string added_text(const std::string& json) {
  Texts texts;
  std::string error;
  if (!texts.add("en", json, error)) return "refused: " + error;
  const std::string* text = texts.find("en", "key");
  return text != nullptr ? *text : "missing";
}

}  // namespace

TEST_CASE("a key is looked up in the language asked for") {
  const Texts texts = two_languages();
  CHECK(*texts.find("pt", "menu.save") == "Salvar");
  CHECK(*texts.find("en", "menu.save") == "Save");
}

TEST_CASE("a key a translation lacks is shown in English, and one nobody has is missing") {
  const Texts texts = two_languages();
  CHECK(*texts.find("pt", "menu.quit") == "Quit");
  CHECK(*texts.find("de", "menu.quit") == "Quit");
  CHECK(texts.find("pt", "menu.nothing") == nullptr);
}

TEST_CASE("the languages and keys a set of texts has") {
  const Texts texts = two_languages();
  CHECK(texts.has("pt"));
  CHECK_FALSE(texts.has("de"));
  CHECK(texts.keys("pt") == std::vector<std::string>{"menu.save"});
  CHECK(texts.keys("de").empty());
}

TEST_CASE("a later file of a language adds to it, and replaces what it says again") {
  Texts texts = two_languages();
  std::string error;
  REQUIRE(texts.add("pt", R"({"menu.quit": "Sair", "menu.save": "Gravar"})", error));
  CHECK(*texts.find("pt", "menu.quit") == "Sair");
  CHECK(*texts.find("pt", "menu.save") == "Gravar");
}

TEST_CASE("a long text is a list of lines") {
  CHECK(added_text(R"({"key": ["usage:", "  redgem <game_dir>", ""]})") == "usage:\n  redgem <game_dir>\n");
  CHECK(added_text(R"({"key": []})").empty());
}

TEST_CASE("a file that is no set of texts adds nothing, and says why") {
  CHECK(added_text("{ not json") == "refused: a translation file is a JSON object of keys and texts");
  CHECK(added_text(R"(["key", "text"])") == "refused: a translation file is a JSON object of keys and texts");
  CHECK(added_text(R"({"key": 3})") == "refused: the value of key is neither a text nor a list of lines");
  CHECK(added_text(R"({"key": ["a", 3]})") == "refused: the value of key is neither a text nor a list of lines");
}

TEST_CASE("placeholders are filled by name, wherever a translation puts them") {
  CHECK(fill_placeholders("Created {game}", {{"game", "Racer"}}) == "Created Racer");
  CHECK(fill_placeholders("{count} of {game}", {{"game", "Racer"}, {"count", "3"}}) == "3 of Racer");
  CHECK(fill_placeholders("{a}{a}", {{"a", "x"}}) == "xx");
  CHECK(fill_placeholders("no values", {}) == "no values");
}

TEST_CASE("a placeholder with no value stays, and so do braces that are no placeholder") {
  CHECK(fill_placeholders("Hello {name}", {}) == "Hello {name}");
  CHECK(fill_placeholders("a { b", {{"b", "x"}}) == "a { b");
  CHECK(fill_placeholders("a } b", {}) == "a } b");
  CHECK(fill_placeholders("{x", {{"x", "1"}}) == "{x");
}

TEST_CASE("the engine's texts are in English when no translation has the language asked for") {
  translations::use_language("xx");
  CHECK(translations::language() == "en");
  CHECK(translations::requested_language() == "xx");
  translations::use_language("en");
  CHECK(translations::language() == "en");
}

TEST_CASE("a key no translation file has is shown as itself") {
  CHECK(std::string(tr("nowhere.no.key")) == "nowhere.no.key");
  CHECK(tr("nowhere.no.key", {{"x", "1"}}) == "nowhere.no.key");
}

TEST_CASE("the engine's texts stay where they are from one lookup to the next") {
  const char* first = tr("editor.menu.file");
  CHECK(std::string(first) == "File");
  CHECK(tr("editor.menu.file") == first);
}

TEST_CASE("a locale's name gives its language") {
  CHECK(platform::language_of_locale("pt_BR.UTF-8") == "pt");
  CHECK(platform::language_of_locale("pt-BR") == "pt");
  CHECK(platform::language_of_locale("de@euro") == "de");
  CHECK(platform::language_of_locale("EN") == "en");
  CHECK(platform::language_of_locale("C.UTF-8").empty());
  CHECK(platform::language_of_locale("POSIX").empty());
  CHECK(platform::language_of_locale("").empty());
}
