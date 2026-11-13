#include "project/search_command.hpp"

#include <cmath>
#include <limits>
#include <optional>
#include <utility>

#include "app/scene_files.hpp"
#include "core/command_line.hpp"
#include "core/translations.hpp"
#include "project/asset_search.hpp"
#include "project/closest_words.hpp"

namespace redgem::tools {
namespace {

constexpr const char* kKindOption = "--kind";
constexpr const char* kTagOption = "--tag";
constexpr const char* kLimitOption = "--limit";
constexpr const char* kKindSeparator = "|";
constexpr const char* kWordSeparator = " ";
// The words of `redgem assets`: the game, then what to look for.
constexpr std::size_t kGameWord = 0;
constexpr std::size_t kFirstSearchWord = 1;

ToolGrammar assets_grammar() { return {{kKindOption, kTagOption, kLimitOption}, {command_line::kHelpOption}}; }
ToolGrammar templates_grammar() { return {{kLimitOption}, {command_line::kHelpOption}}; }

ToolResult answer(std::string text) { return ToolResult{kExitSuccess, std::move(text), {}}; }

std::string assets_usage() { return tr("project.search.assets_usage", {{"kinds", kind_words(kKindSeparator)}}); }

ToolResult usage_failure(const std::string& problem, const std::string& usage) {
  return failed(problem + "\n\n" + usage, kExitUsage);
}

bool is_count(double number) {
  const double end = std::ldexp(1.0, std::numeric_limits<std::size_t>::digits);
  return number >= 0 && number < end && std::floor(number) == number;
}

// --limit's count, the default without it; nothing when it is no count.
std::optional<std::size_t> limit_of(const ToolLine& line) {
  const auto value = line.value(kLimitOption);
  if (!value) return kDefaultSearchLimit;
  const auto number = number_from(*value);
  if (!number || !is_count(*number)) return std::nullopt;
  return static_cast<std::size_t>(*number);
}

std::string words_from(const ToolLine& line, std::size_t first) {
  std::vector<std::string> words;
  for (std::size_t index = first; index < line.words.size(); ++index) words.push_back(line.words[index]);
  return joined(words, kWordSeparator);
}

// The line under the found ones when the limit left some out.
std::string more_line(std::size_t shown, std::size_t matched) {
  if (matched <= shown) return {};
  return tr("project.search.more", {{"count", std::to_string(matched - shown)}}) + "\n";
}

std::string assets_text(const SearchResult<AssetEntry>& result) {
  if (result.found.empty()) return std::string(tr("project.search.no_assets")) + "\n";
  std::string text;
  for (const AssetEntry& asset : result.found) text += std::string(kind_word(asset.kind)) + " " + asset.path + "\n";
  return text + more_line(result.found.size(), result.matched);
}

std::vector<std::string> template_list() {
  std::vector<std::string> names;
  for (const GameTemplate& made : game_templates()) names.push_back(made.name);
  return names;
}

std::string templates_text(const SearchResult<GameTemplate>& result) {
  if (result.found.empty()) return tr("project.search.no_templates", {{"names", joined(template_list())}}) + "\n";
  std::string text;
  for (const GameTemplate& made : result.found) text += made.name + ": " + made.description + "\n";
  return text + more_line(result.found.size(), result.matched);
}

// The query a line asks for; why it cannot be one otherwise.
std::optional<AssetQuery> asset_query(const ToolLine& line, std::string& problem) {
  const auto limit = limit_of(line);
  if (!limit) {
    problem = tr("project.search.needs_count");
    return std::nullopt;
  }
  AssetQuery query{words_from(line, kFirstSearchWord), std::nullopt, line.value(kTagOption), *limit};
  const auto kind = line.value(kKindOption);
  if (!kind) return query;
  query.kind = kind_named(*kind);
  if (!query.kind) problem = tr("project.search.bad_kind", {{"kinds", kind_words(kKindSeparator)}});
  return query.kind ? std::optional<AssetQuery>(query) : std::nullopt;
}

ToolResult search_game(const ToolLine& line) {
  const std::filesystem::path game_dir = line.words[kGameWord];
  if (!is_game_folder(game_dir)) return failed(tr("project.tools.not_a_game", {{"game_dir", game_dir.string()}}));
  std::string problem;
  const auto query = asset_query(line, problem);
  if (!query) return usage_failure(problem, assets_usage());
  return answer(assets_text(search_assets(game_assets(game_dir), *query)));
}

}  // namespace

ToolResult run_assets_command(const std::vector<std::string>& args) {
  std::string error;
  const auto line = read_tool_line(args, 0, assets_grammar(), error);
  if (!line) return usage_failure(error, assets_usage());
  if (line->has(command_line::kHelpOption)) return answer(assets_usage());
  if (line->words.empty()) return usage_failure(tr("project.tools.needs_game"), assets_usage());
  return search_game(*line);
}

ToolResult run_templates_command(const std::vector<std::string>& args) {
  std::string error;
  const auto line = read_tool_line(args, 0, templates_grammar(), error);
  const std::string usage = tr("project.search.templates_usage");
  if (!line) return usage_failure(error, usage);
  if (line->has(command_line::kHelpOption)) return answer(usage);
  const auto limit = limit_of(*line);
  if (!limit) return usage_failure(tr("project.search.needs_count"), usage);
  return answer(templates_text(search_templates(game_templates(), words_from(*line, 0), *limit)));
}

}  // namespace redgem::tools
