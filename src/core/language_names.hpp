#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The names of languages ("en", "pt_BR", "pt-BR", "zh-Hant"), as people, systems and translation
// files write them, and how one asked for is matched with those there are texts in.
namespace redgem {

// A language's name as the engine compares it: in small letters, with '_' between its parts
// ("pt-BR" and "PT_br" are both "pt_br").
std::string language_key(std::string_view name);
// The language alone, without its region or script: "pt_BR" -> "pt".
std::string base_language(std::string_view name);
// Of `available` (names as they are written there), the one that best serves `asked`: the same
// language and region, else the language alone, else the same language in another region.
// Nothing when none of them is that language.
std::optional<std::string> best_language(std::string_view asked, const std::vector<std::string>& available);

}  // namespace redgem
