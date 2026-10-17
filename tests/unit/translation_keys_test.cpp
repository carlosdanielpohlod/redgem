// The translation files and the code agree: every key the code shows has a text in English, every
// English text is shown somewhere, and a translation has no key English lacks. For the engine
// (locales/<part>/, the part being the key's first word) and for each game and template
// (<game>/locales/, read by I18n.t, with the engine's game part behind it).

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>

#include "core/texts.hpp"

using namespace redgem;
namespace fs = std::filesystem;

namespace {

using Keys = std::set<std::string>;

const fs::path kSource = REDGEM_SOURCE_DIR;
constexpr const char* kEnglishFile = "en.json";
constexpr const char* kGamePart = "game";  // the engine's texts a game shows (its menus)
// A string of the C++ code that has the shape of a key: words joined by dots, three or more.
const std::regex kCppKey(R"re("([a-z]+(\.[a-z0-9_]+){2,})")re");
// A key a script looks up.
const std::regex kRubyKey(R"re(I18n\.t\("([^"]+)")re");

std::string read_file(const fs::path& file) {
  std::ifstream in(file, std::ios::binary);
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

// The keys of a translation file (it must be one), as it reads.
Keys read_keys(const fs::path& file) {
  Texts texts;
  std::string error;
  REQUIRE_MESSAGE(texts.add("any", read_file(file), error), (file.string() + ": " + error));
  const std::vector<std::string> keys = texts.keys("any");
  return Keys(keys.begin(), keys.end());
}

// The same, each file read once however often it is asked about.
const Keys& keys_of(const fs::path& file) {
  static std::map<fs::path, Keys> read;
  auto found = read.find(file);
  if (found == read.end()) found = read.emplace(file, read_keys(file)).first;
  return found->second;
}

// What `pattern` finds in the files of `folder` that end in `extension`.
Keys keys_in_code(const fs::path& folder, const std::regex& pattern, const std::string& extension) {
  Keys keys;
  if (!fs::is_directory(folder)) return keys;
  for (const fs::directory_entry& entry : fs::recursive_directory_iterator(folder)) {
    if (entry.path().extension() != extension) continue;
    const std::string code = read_file(entry.path());
    for (std::sregex_iterator found(code.begin(), code.end(), pattern); found != std::sregex_iterator(); ++found) {
      keys.insert((*found)[1].str());
    }
  }
  return keys;
}

// The keys the engine shows: its C++ and its Ruby.
Keys engine_keys() {
  Keys keys = keys_in_code(kSource / "src", kCppKey, ".cpp");
  keys.merge(keys_in_code(kSource / "src", kCppKey, ".hpp"));
  keys.merge(keys_in_code(kSource / "ruby" / "engine", kRubyKey, ".rb"));
  return keys;
}

// The engine's parts: the folders of locales/.
std::vector<std::string> parts() {
  std::vector<std::string> names;
  for (const fs::directory_entry& entry : fs::directory_iterator(kSource / "locales")) {
    names.push_back(entry.path().filename().string());
  }
  return names;
}

std::string part_of(const std::string& key) { return key.substr(0, key.find('.')); }

// Whether the English file of a folder of translations has a key.
bool in_english(const fs::path& locales, const std::string& key) {
  return keys_of(locales / kEnglishFile).count(key) == 1;
}

// Every translation of a folder has only keys its English file has.
void check_translations(const fs::path& locales) {
  const Keys english = keys_of(locales / kEnglishFile);
  for (const fs::directory_entry& entry : fs::directory_iterator(locales)) {
    for (const std::string& key : keys_of(entry.path())) {
      CHECK_MESSAGE(english.count(key) == 1, (entry.path().string() + " has a key English lacks: " + key));
    }
  }
}

// The games and templates: every folder of games/ and templates/ with scripts.
std::vector<fs::path> games() {
  std::vector<fs::path> folders;
  for (const char* kind : {"games", "templates"}) {
    for (const fs::directory_entry& entry : fs::directory_iterator(kSource / kind)) {
      if (fs::is_directory(entry.path() / "scripts")) folders.push_back(entry.path());
    }
  }
  return folders;
}

}  // namespace

TEST_CASE("every key the engine shows has a text in English, in the part its first word names") {
  const fs::path locales = kSource / "locales";
  for (const std::string& key : engine_keys()) {
    const fs::path part = locales / part_of(key);
    if (!fs::is_directory(part)) continue;  // a text with dots in it that is no key (a host name)
    CHECK_MESSAGE(in_english(part, key), ("no text in " + (part / kEnglishFile).string() + " for " + key));
  }
}

TEST_CASE("every text of the engine's English files is shown somewhere, and is in the right part") {
  const Keys used = engine_keys();
  for (const std::string& part : parts()) {
    for (const std::string& key : keys_of(kSource / "locales" / part / kEnglishFile)) {
      CHECK_MESSAGE(used.count(key) == 1, ("nothing shows " + key + " (locales/" + part + ")"));
      CHECK_MESSAGE(part_of(key) == part, (key + " is in locales/" + part + ", not in the part it names"));
    }
  }
}

TEST_CASE("the engine's translations have no key English lacks") {
  for (const std::string& part : parts()) check_translations(kSource / "locales" / part);
}

TEST_CASE("every key a game or a template shows has a text in English, and every text is shown") {
  const Keys engine = keys_of(kSource / "locales" / kGamePart / kEnglishFile);
  for (const fs::path& game : games()) {
    const fs::path locales = game / "locales";
    const Keys used = keys_in_code(game / "scripts", kRubyKey, ".rb");
    const Keys own = fs::is_directory(locales) ? keys_of(locales / kEnglishFile) : Keys();
    for (const std::string& key : used) {
      CHECK_MESSAGE(own.count(key) + engine.count(key) > 0, (game.string() + " shows " + key + " with no text"));
    }
    for (const std::string& key : own) CHECK_MESSAGE(used.count(key) == 1, (game.string() + " never shows " + key));
    if (fs::is_directory(locales)) check_translations(locales);
  }
}
