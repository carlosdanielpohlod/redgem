#include "project/asset_search.hpp"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <system_error>
#include <utility>

#include "core/config.hpp"
#include "project/closest_words.hpp"

namespace redgem::tools {
namespace {

namespace fs = std::filesystem;

// How much each way a word can match adds to a score.
constexpr int kNameIsWord = 100;
constexpr int kNameStartsWithWord = 60;
constexpr int kNameHoldsWord = 30;
constexpr int kTagIsWord = 20;
constexpr int kTextHoldsWord = 10;
constexpr int kNoMatch = 0;
// The score of everything when no word is asked for: all match, none better.
constexpr int kEveryMatch = 1;
// A kind's folder is read no further than this many files: a search answers, it does not walk a disk.
constexpr std::size_t kMostAssetsOfAKind = 10000;
constexpr char kHiddenMark = '.';

struct KindFolder {
  AssetKind kind;
  const char* word;
  const char* folder;
};
constexpr KindFolder kKindFolders[] = {
    {AssetKind::Model, "model", config::kModelsDirName},    {AssetKind::Texture, "texture", config::kImagesDirName},
    {AssetKind::Sound, "sound", config::kSoundsDirName},    {AssetKind::Script, "script", config::kScriptsDirName},
    {AssetKind::Prefab, "prefab", config::kPrefabsDirName},
};

std::vector<std::string> split_words(const std::string& text) {
  std::istringstream stream(lowered(text));
  return std::vector<std::string>(std::istream_iterator<std::string>(stream), std::istream_iterator<std::string>());
}

bool holds(const std::string& text, const std::string& word) { return text.find(word) != std::string::npos; }

bool has_tag(const std::vector<std::string>& tags, const std::string& word) {
  return std::find(tags.begin(), tags.end(), word) != tags.end();
}

// What one (lowercase) word adds to the score of lowercase fields.
int word_score(const SearchFields& fields, const std::string& word) {
  if (fields.name == word) return kNameIsWord;
  if (fields.name.rfind(word, 0) == 0) return kNameStartsWithWord;
  if (holds(fields.name, word)) return kNameHoldsWord;
  if (has_tag(fields.tags, word)) return kTagIsWord;
  if (holds(fields.text, word)) return kTextHoldsWord;
  return kNoMatch;
}

std::vector<std::string> lowered_all(const std::vector<std::string>& texts) {
  std::vector<std::string> low;
  low.reserve(texts.size());
  for (const std::string& text : texts) low.push_back(lowered(text));
  return low;
}

SearchFields lowered_fields(const SearchFields& fields) {
  return {lowered(fields.name), lowered_all(fields.tags), lowered(fields.text)};
}

bool hidden(const fs::path& file) {
  const std::string name = file.filename().string();
  return !name.empty() && name.front() == kHiddenMark;
}

// An asset the game may use: a plain file (no link).
bool listed_file(const fs::directory_entry& entry) {
  std::error_code error;
  return entry.is_regular_file(error) && !entry.is_symlink(error);
}

std::vector<std::string> folder_tags(const fs::path& relative) {
  std::vector<std::string> tags;
  for (const fs::path& part : relative.parent_path()) tags.push_back(part.string());
  return tags;
}

AssetEntry entry_of(const KindFolder& kind, const fs::path& folder, const fs::path& file) {
  const fs::path relative = file.lexically_relative(folder);
  return {(fs::path(kind.folder) / relative).generic_string(), kind.kind, folder_tags(relative)};
}

// Whether a walk of a folder goes on: it did not fail, has files left, and found few enough.
bool keeps_reading(const fs::recursive_directory_iterator& files, const std::error_code& error, std::size_t found) {
  return !error && files != fs::recursive_directory_iterator() && found < kMostAssetsOfAKind;
}

// Takes the file a walk is at as an asset; a hidden one is left out, and so is what a hidden folder holds.
void visit(fs::recursive_directory_iterator& files, const KindFolder& kind, const fs::path& folder,
           std::vector<AssetEntry>& assets) {
  if (hidden(files->path())) {
    files.disable_recursion_pending();
    return;
  }
  if (listed_file(*files)) assets.push_back(entry_of(kind, folder, files->path()));
}

void add_kind(const fs::path& game_dir, const KindFolder& kind, std::vector<AssetEntry>& assets) {
  const fs::path folder = game_dir / kind.folder;
  std::error_code error;
  if (fs::is_symlink(folder, error)) return;
  fs::recursive_directory_iterator files(folder, fs::directory_options::skip_permission_denied, error);
  const std::size_t before = assets.size();
  for (; keeps_reading(files, error, assets.size() - before); files.increment(error))
    visit(files, kind, folder, assets);
}

SearchFields asset_fields(const AssetEntry& asset) {
  return {fs::path(asset.path).stem().string(), asset.tags, asset.path};
}

bool of_asked_kind(const AssetEntry& asset, const AssetQuery& query) {
  return !query.kind || asset.kind == *query.kind;
}

// A query's words and tag in small letters, the words apart: made once for a whole search.
struct PreparedQuery {
  std::vector<std::string> words;
  std::optional<std::string> tag;
};

PreparedQuery prepared(const AssetQuery& query) {
  PreparedQuery ready{split_words(query.words), std::nullopt};
  if (query.tag) ready.tag = lowered(*query.tag);
  return ready;
}

bool has_asked_tag(const SearchFields& low, const PreparedQuery& query) {
  return !query.tag || has_tag(low.tags, *query.tag);
}

// How well every (lowercase) word matches lowercase fields: 0 when one of them matches nothing.
int words_score(const SearchFields& low, const std::vector<std::string>& words) {
  if (words.empty()) return kEveryMatch;
  int score = 0;
  for (const std::string& word : words) {
    const int added = word_score(low, word);
    if (added == kNoMatch) return kNoMatch;
    score += added;
  }
  return score;
}

int asset_score(const AssetEntry& asset, const AssetQuery& query, const PreparedQuery& ready) {
  if (!of_asked_kind(asset, query)) return kNoMatch;
  const SearchFields low = lowered_fields(asset_fields(asset));
  return has_asked_tag(low, ready) ? words_score(low, ready.words) : kNoMatch;
}

template <typename Found>
struct Scored {
  int score;
  std::string key;  // what ties are ordered by
  const Found* found;
};

template <typename Found>
bool better(const Scored<Found>& a, const Scored<Found>& b) {
  if (a.score != b.score) return a.score > b.score;
  return a.key < b.key;
}

template <typename Found>
SearchResult<Found> ranked(std::vector<Scored<Found>> scored, std::size_t limit) {
  std::sort(scored.begin(), scored.end(), better<Found>);
  SearchResult<Found> result;
  result.matched = scored.size();
  const std::size_t shown = limit == kNoSearchLimit ? scored.size() : std::min(limit, scored.size());
  for (std::size_t index = 0; index < shown; ++index) result.found.push_back(*scored[index].found);
  return result;
}

}  // namespace

const char* kind_word(AssetKind kind) {
  for (const KindFolder& known : kKindFolders) {
    if (known.kind == kind) return known.word;
  }
  return kKindFolders[0].word;
}

std::optional<AssetKind> kind_named(const std::string& word) {
  for (const KindFolder& known : kKindFolders) {
    if (lowered(word) == known.word) return known.kind;
  }
  return std::nullopt;
}

std::string kind_words(const std::string& separator) {
  std::string words;
  for (const KindFolder& known : kKindFolders) words += (words.empty() ? "" : separator) + known.word;
  return words;
}

std::vector<AssetEntry> game_assets(const fs::path& game_dir) {
  std::vector<AssetEntry> assets;
  for (const KindFolder& kind : kKindFolders) add_kind(game_dir, kind, assets);
  std::sort(assets.begin(), assets.end(), [](const AssetEntry& a, const AssetEntry& b) { return a.path < b.path; });
  return assets;
}

int match_score(const SearchFields& fields, const std::string& words) {
  return words_score(lowered_fields(fields), split_words(words));
}

SearchResult<AssetEntry> search_assets(const std::vector<AssetEntry>& assets, const AssetQuery& query) {
  const PreparedQuery ready = prepared(query);
  std::vector<Scored<AssetEntry>> scored;
  for (const AssetEntry& asset : assets) {
    const int score = asset_score(asset, query, ready);
    if (score != kNoMatch) scored.push_back({score, asset.path, &asset});
  }
  return ranked(std::move(scored), query.limit);
}

SearchResult<GameTemplate> search_templates(const std::vector<GameTemplate>& templates, const std::string& words,
                                            std::size_t limit) {
  const std::vector<std::string> wanted = split_words(words);
  std::vector<Scored<GameTemplate>> scored;
  for (const GameTemplate& made : templates) {
    const int score = words_score(lowered_fields({made.name, {}, made.description}), wanted);
    if (score != kNoMatch) scored.push_back({score, made.name, &made});
  }
  return ranked(std::move(scored), limit);
}

}  // namespace redgem::tools
