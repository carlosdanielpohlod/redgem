#pragma once

// What `redgem check` says of the I18n keys a game's files show (dialogues, quests): a key with no
// text anywhere is an error, and one a language of the game lacks, a warning for that language.

#include <string>
#include <vector>

#include "project/game_checks.hpp"
#include "script/dialogue_file.hpp"
#include "script/game_locales.hpp"

namespace redgem::tools {

// Every key of `texts` (each said once), used in `file` ("dialogues/baker.json").
void check_texts(const GameLocales& locales, CheckReport& report, const std::string& file,
                 const std::vector<dialogue::NameUse>& texts);

// Each line after its file's name: "quests/bread.json: stage bake: ...".
void add_lines(std::vector<std::string>& to, const std::string& file, const std::vector<std::string>& lines);

}  // namespace redgem::tools
