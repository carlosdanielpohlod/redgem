// A game's own translations (<game>/locales/<language>.json): what its scripts show with I18n, in
// which language, with what behind them, and which of its files are left out.

#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/string.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/translations.hpp"
#include "scene/scene.hpp"
#include "script/game_locales.hpp"
#include "script/script_config.hpp"
#include "script/script_context.hpp"
#include "script/script_host.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

// What a line of Ruby run by `scripts` gives, as its to_s.
std::string ruby(ScriptHost& scripts, const std::string& code) {
  const RubyResult result = scripts.eval(code, "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  const mrb_value text = mrb_obj_as_string(scripts.vm().state(), result.value);
  return {RSTRING_PTR(text), static_cast<std::size_t>(RSTRING_LEN(text))};
}

// The scripts of the fixture game whose translations are tested, with no scene running.
struct TranslatedGame {
  test::LogCapture log;  // (the fixture's broken files are warned of)
  Scene scene;
  ScriptContext context;
  ScriptHost scripts{context, fixture("games/translated/scripts")};
};

// A game of its own in the temporary folder, with an English translation file.
std::filesystem::path scratch_game(const std::string& name) {
  const std::filesystem::path game = std::filesystem::temp_directory_path() / name;
  std::filesystem::remove_all(game);
  std::filesystem::create_directories(game / config::kLocalesDirName);
  std::ofstream(game / config::kLocalesDirName / "en.json") << R"({"hello": "Hello"})";
  return game;
}

// The user asks for a language for as long as it lives; English again afterwards.
class AskedLanguage {
 public:
  explicit AskedLanguage(const char* language) { translations::use_language(language); }
  ~AskedLanguage() { translations::use_language("en"); }
  AskedLanguage(const AskedLanguage&) = delete;
  AskedLanguage& operator=(const AskedLanguage&) = delete;
};

}  // namespace

TEST_CASE("a game starts in the language the user asked for when it has texts in it, even if the engine has none") {
  {
    const AskedLanguage portuguese("pt");
    REQUIRE(translations::language() == "en");  // the engine's texts are in English alone
    TranslatedGame game;
    CHECK(ruby(game.scripts, "I18n.locale") == "pt");
    CHECK(ruby(game.scripts, "I18n.t('greeting', name: 'Ana')") == "Olá, Ana!");
    CHECK(ruby(game.scripts, "GameMenu.new.screens[:pause].options.first.first") == "Continuar");
  }
  // A language the game has no texts in (its de.json is broken) starts it in the engine's.
  const AskedLanguage german("de");
  TranslatedGame game;
  CHECK(ruby(game.scripts, "I18n.locale") == "en");
}

TEST_CASE("a game's scripts show its texts, with their placeholders filled") {
  TranslatedGame game;
  CHECK(ruby(game.scripts, "I18n.locale") == "en");
  CHECK(ruby(game.scripts, "I18n.t('greeting', name: 'Ana')") == "Hello, Ana!");
  CHECK(ruby(game.scripts, "I18n.t('farewell', name: 'Ana')") == "Bye,\nAna.");
  CHECK(ruby(game.scripts, "Greeter.greeting('Rui')") == "Hello, Rui!");  // from a script of the game
  CHECK(ruby(game.scripts, "I18n.t('missing.key')") == "missing.key");
  CHECK(game.scripts.error_count() == 0);
}

TEST_CASE("a game's texts follow I18n.locale, with its English and then the engine's behind them") {
  TranslatedGame game;
  ruby(game.scripts, "I18n.locale = :pt");
  CHECK(ruby(game.scripts, "I18n.locale.inspect") == ":pt");
  CHECK(ruby(game.scripts, "I18n.t('greeting', name: 'Ana')") == "Olá, Ana!");
  CHECK(ruby(game.scripts, "I18n.t('only.english')") == "Only in English");
  // The engine's texts are there too, and a game's files may translate them.
  CHECK(ruby(game.scripts, "I18n.t('game.menu.resume')") == "Continuar");
  CHECK(ruby(game.scripts, "I18n.t('game.menu.quit')") == "Quit");
  CHECK(ruby(game.scripts, "GameMenu.new.screens[:pause].options.first.first") == "Continuar");
  ruby(game.scripts, "I18n.locale = :en");
  CHECK(ruby(game.scripts, "I18n.t('game.menu.resume')") == "Resume");
}

TEST_CASE("the files of a game's locales folder that are no translations are left out, each named") {
  TranslatedGame game;
  CHECK(ruby(game.scripts, "I18n.available_locales.inspect") == "[:en, :pt]");
  for (const char* left_out : {"de.json", "fr.json", "es.json", "notes.txt"}) {
    CAPTURE(left_out);
    CHECK(game.log.contains(left_out));
  }
  CHECK(game.log.contains("a translation file is a JSON object of keys and texts"));
  CHECK(game.log.contains("the value of greeting is neither a text nor a list of lines"));
  CHECK(game.log.contains("a translation file is named for its language, such as en.json"));
  // A language whose file was left out shows the game's English.
  ruby(game.scripts, "I18n.locale = :de");
  CHECK(ruby(game.scripts, "I18n.t('greeting', name: 'Ana')") == "Hello, Ana!");
}

TEST_CASE("a translation file larger than the engine reads, or reached through a link, is left out") {
  test::LogCapture log;
  const std::filesystem::path game = scratch_game("redgem-game-locales-test");
  const std::filesystem::path locales = game / config::kLocalesDirName;
  std::ofstream(locales / "pt.json") << R"({"hello": "Olá"})"
                                     << std::string(static_cast<std::size_t>(config::kMaxLocaleFileBytes), ' ');
  std::filesystem::create_symlink(locales / "en.json", locales / "es.json");
  const GameLocales read = read_game_locales(game);
  CHECK(read.languages == std::vector<std::string>{"en"});
  CHECK(log.contains("larger than"));
  CHECK(log.contains("symbolic link"));
  std::filesystem::remove_all(game);
}

TEST_CASE("a game with no locales folder has no texts of its own, and scripts outside a game have none") {
  const std::filesystem::path game = scratch_game("redgem-no-locales-test");
  std::filesystem::remove_all(game / config::kLocalesDirName);
  CHECK(read_game_locales(game).languages.empty());
  std::filesystem::remove_all(game);
  Scene scene;
  ScriptContext context;
  context.scene = &scene;
  // (Scripts run from a folder that is not a game's scripts folder, as a file of tests is: the
  // folder above it is no game's, and is not looked in.)
  ScriptHost scripts(context, fixture("games/translated/locales"));
  CHECK(ruby(scripts, "I18n.available_locales.inspect") == "[]");
}

TEST_CASE("a language's name is letters, digits, '-' and '_', and short") {
  for (const char* name : {"en", "pt", "pt_BR", "zh-Hant"}) CHECK(is_language_name(name));
  for (const char* name : {"", "..", "en.json", "pt/BR", "pt BR", "aaaaaaaaaaaaaaaaaaaaaaaa"}) {
    CAPTURE(name);
    CHECK_FALSE(is_language_name(name));
  }
}
