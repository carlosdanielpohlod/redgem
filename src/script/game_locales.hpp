#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "core/texts.hpp"

// A game's own translations, which its scripts show with I18n: <game>/locales/<language>.json,
// files like the engine's (see core/texts.hpp). A game is not trusted: only files named for a
// language are read, none larger than config::kMaxLocaleFileBytes nor reached through a link,
// and each one left out is named in a warning that says why.
namespace redgem {

struct GameLocales {
  Texts texts;
  std::vector<std::string> languages;  // those a file was read for, in order
};

// The translations in the locales folder of `game_dir`; none when it has no such folder.
GameLocales read_game_locales(const std::filesystem::path& game_dir);

// Whether `name` can be a language's ("en", "pt_BR", "zh-Hant"): letters, digits, '-' and '_',
// at most config::kMaxLanguageNameLength of them.
bool is_language_name(std::string_view name);

}  // namespace redgem
