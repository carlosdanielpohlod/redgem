#pragma once

// Finding things by a few words, for coding assistants and the command line: a game's assets
// (its models, textures, sounds, scripts and prefabs) and the templates new games start from.
//
// An asset's kind is the folder of the game it is in (models/, images/, sounds/, scripts/,
// prefabs/), and its tags are the names of the folders under that one it sits in
// (models/enemies/orc.glb is a model tagged "enemies"): a game has no other place to keep tags.
// Every word of a search must match; the best matches come first: a name that is the word, then
// one that starts with it, one that holds it, a tag that is the word, and last the rest of the
// path (or a template's description) that holds it. Case does not matter.

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "project/game_templates.hpp"

namespace redgem::tools {

enum class AssetKind { Model, Texture, Sound, Script, Prefab };

// What a search shows when it is not told how many: the best twenty.
inline constexpr std::size_t kDefaultSearchLimit = 20;
// A search that lists everything it finds: its limit.
inline constexpr std::size_t kNoSearchLimit = 0;

// An asset of a game.
struct AssetEntry {
  std::string path;  // inside the game, '/' between folders: "models/enemies/orc.glb"
  AssetKind kind = AssetKind::Model;
  std::vector<std::string> tags;  // the folders under its kind's folder, outermost first
};

// What to look for. Empty words find everything (of that kind and tag).
struct AssetQuery {
  std::string words;
  std::optional<AssetKind> kind;
  std::optional<std::string> tag;
  std::size_t limit = kDefaultSearchLimit;  // kNoSearchLimit: all of them
};

// What a search found, best first, and how many matched in all (before the limit).
template <typename Found>
struct SearchResult {
  std::vector<Found> found;
  std::size_t matched = 0;
};

// The word a kind is called by ("model", "texture", "sound", "script", "prefab"), and back.
const char* kind_word(AssetKind kind);
std::optional<AssetKind> kind_named(const std::string& word);
// Every kind's word, between `separator`s: "model|texture|...".
std::string kind_words(const std::string& separator);

// Every asset of a game, by path. Links and hidden files (".name") are left out.
std::vector<AssetEntry> game_assets(const std::filesystem::path& game_dir);

SearchResult<AssetEntry> search_assets(const std::vector<AssetEntry>& assets, const AssetQuery& query);
SearchResult<GameTemplate> search_templates(const std::vector<GameTemplate>& templates, const std::string& words,
                                            std::size_t limit);

// How well a name, its tags and the rest of its text match every one of `words`: 0 when one
// of them matches nothing. (Exposed for its tests.)
struct SearchFields {
  std::string name;
  std::vector<std::string> tags;
  std::string text;
};
int match_score(const SearchFields& fields, const std::string& words);

}  // namespace redgem::tools
