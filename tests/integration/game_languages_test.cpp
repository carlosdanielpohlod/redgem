// Which of a game's languages its scripts start in, and whose text wins when the game and the
// engine both have one: a language asked for with its region ("pt-BR"), in any case, finds the
// game's file that serves it best, and the engine's translation into the player's language is
// never hidden by the game's English.

#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/string.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "core/config.hpp"
#include "core/translations.hpp"
#include "scene/scene.hpp"
#include "script/game_locales.hpp"
#include "script/script_context.hpp"
#include "script/script_host.hpp"
#include "test_support.hpp"

using namespace redgem;

namespace {

namespace fs = std::filesystem;

// A language no real translation is in, which the engine is given a translation into here.
constexpr const char* kEngineLanguage = "zz";

std::string ruby(ScriptHost& scripts, const std::string& code) {
  const RubyResult result = scripts.eval(code, "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  const mrb_value text = mrb_obj_as_string(scripts.vm().state(), result.value);
  return {RSTRING_PTR(text), static_cast<std::size_t>(RSTRING_LEN(text))};
}

// A game in the temporary folder with a translation file for each of `files` ({name, json}).
fs::path game_with(const std::string& name, std::initializer_list<std::pair<std::string, std::string>> files) {
  const fs::path game = fs::temp_directory_path() / name;
  fs::remove_all(game);
  fs::create_directories(game / config::kLocalesDirName);
  fs::create_directories(game / config::kScriptsDirName);
  for (const auto& [file, json] : files) std::ofstream(game / config::kLocalesDirName / file) << json;
  return game;
}

// The language its scripts start in, for a user who asked for `asked`.
std::string starting_locale(const fs::path& game, const char* asked) {
  translations::use_language(asked);
  Scene scene;
  ScriptContext context;
  context.scene = &scene;
  ScriptHost scripts(context, game / config::kScriptsDirName);
  const std::string locale = ruby(scripts, "I18n.locale");
  translations::use_language("en");
  return locale;
}

// The engine, given a translation of one of its texts into kEngineLanguage (for as long as the
// program runs: nobody else asks for that language).
void give_the_engine_a_translation() {
  static const std::string json = R"({"game.menu.quit": "Zz quit"})";
  const EmbeddedFile file{reinterpret_cast<const unsigned char*>(json.data()), static_cast<int>(json.size())};
  translations::add_file(std::string(kEngineLanguage) + ".json", file);
}

}  // namespace

TEST_CASE("a game starts in its file that best serves the language asked for, region and case aside") {
  const fs::path game =
      game_with("redgem-game-regions-test", {{"en.json", "{}"}, {"pt_BR.json", "{}"}, {"pt.json", "{}"}});
  CHECK(starting_locale(game, "pt-BR") == "pt_BR");
  CHECK(starting_locale(game, "PT_br") == "pt_BR");
  CHECK(starting_locale(game, "pt_PT") == "pt");  // the language alone, before another region of it
  CHECK(starting_locale(game, "de_DE") == "en");
  fs::remove(game / config::kLocalesDirName / "pt.json");
  CHECK(starting_locale(game, "pt") == "pt_BR");  // another region of the same language
  fs::remove_all(game);
}

TEST_CASE("a game's text in a region falls back to the same language without it") {
  const fs::path game =
      game_with("redgem-game-region-texts-test", {{"en.json", R"({"hi": "Hi"})"}, {"pt.json", R"({"hi": "Oi"})"}});
  translations::use_language("pt_BR");
  Scene scene;
  ScriptContext context;
  context.scene = &scene;
  ScriptHost scripts(context, game / config::kScriptsDirName);
  translations::use_language("en");
  CHECK(ruby(scripts, "I18n.locale") == "pt");
  ruby(scripts, "I18n.locale = :pt_BR");
  CHECK(ruby(scripts, "I18n.t('hi')") == "Oi");
  fs::remove_all(game);
}

TEST_CASE("the engine's translation into the player's language beats a game's English of the same text") {
  give_the_engine_a_translation();
  const fs::path game = game_with("redgem-game-overrides-test", {{"en.json", R"({"game.menu.quit": "Leave"})"}});
  Scene scene;
  ScriptContext context;
  context.scene = &scene;
  ScriptHost scripts(context, game / config::kScriptsDirName);
  CHECK(ruby(scripts, "I18n.t('game.menu.quit')") == "Leave");  // in English, the game's own
  ruby(scripts, std::string("I18n.locale = :") + kEngineLanguage);
  CHECK(ruby(scripts, "I18n.t('game.menu.quit')") == "Zz quit");
  fs::remove_all(game);
}

TEST_CASE("a translation file whose name is not plain letters is left out with a warning, whatever it spells") {
  test::LogCapture log;
  const fs::path game = game_with("redgem-game-odd-names-test", {{"en.json", "{}"}});
  std::ofstream(game / config::kLocalesDirName / fs::u8path(u8"中文.json")) << "{}";
  CHECK(read_game_locales(game).languages == std::vector<std::string>{"en"});
  CHECK(log.contains("a translation file is named for its language"));
  fs::remove_all(game);
}

TEST_CASE("a GameMenu made before I18n.locale changes shows its labels in the language it changed to") {
  test::LogCapture log;  // (the fixture's broken files are warned of)
  Scene scene;
  ScriptContext context;
  context.scene = &scene;
  ScriptHost scripts(context, test::fixture("games/translated/scripts"));
  ruby(scripts, "$menu = GameMenu.new");
  CHECK(ruby(scripts, "$menu.screens[:pause].options.first.first") == "Resume");
  ruby(scripts, "I18n.locale = :pt");
  CHECK(ruby(scripts, "$menu.screens[:pause].options.first.first") == "Continuar");
}
