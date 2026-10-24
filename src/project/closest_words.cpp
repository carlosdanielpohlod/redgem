#include "project/closest_words.hpp"

#include <algorithm>
#include <cctype>
#include <numeric>
#include <utility>

namespace redgem::tools {
namespace {

// A name is near a word when it takes at most this share of the word's letters to change it.
constexpr std::size_t kLettersPerChange = 3;
// ... and never fewer changes than this, so short words still find their typos.
constexpr std::size_t kLeastChanges = 2;

std::string lowered(const std::string& text) {
  std::string low = text;
  std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return std::tolower(c); });
  return low;
}

// One more row of the table of distances: from `previous` (the row of the letters before `letter`).
std::vector<std::size_t> next_row(const std::vector<std::size_t>& previous, char letter, const std::string& b) {
  std::vector<std::size_t> row(previous.size());
  row[0] = previous[0] + 1;
  for (std::size_t j = 1; j < row.size(); ++j) {
    const std::size_t replaced = previous[j - 1] + (letter == b[j - 1] ? 0 : 1);
    row[j] = std::min({previous[j] + 1, row[j - 1] + 1, replaced});
  }
  return row;
}

bool holds_either(const std::string& a, const std::string& b) {
  return a.find(b) != std::string::npos || b.find(a) != std::string::npos;
}

std::size_t allowed_changes(const std::string& word) { return std::max(kLeastChanges, word.size() / kLettersPerChange); }

}  // namespace

std::size_t edit_distance(const std::string& a, const std::string& b) {
  const std::string low_a = lowered(a);
  const std::string low_b = lowered(b);
  std::vector<std::size_t> row(low_b.size() + 1);
  std::iota(row.begin(), row.end(), std::size_t{0});
  for (const char letter : low_a) row = next_row(row, letter, low_b);
  return row.back();
}

std::vector<std::string> closest_words(const std::string& word, const std::vector<std::string>& known,
                                       std::size_t most) {
  std::vector<std::pair<std::size_t, std::string>> near;
  for (const std::string& name : known) {
    const std::size_t distance = edit_distance(word, name);
    const bool related = !word.empty() && holds_either(lowered(word), lowered(name));
    if (distance <= allowed_changes(word) || related) near.emplace_back(distance, name);
  }
  std::stable_sort(near.begin(), near.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  std::vector<std::string> names;
  for (std::size_t i = 0; i < near.size() && i < most; ++i) names.push_back(near[i].second);
  return names;
}

std::string joined(const std::vector<std::string>& names, const std::string& separator) {
  std::string text;
  for (const std::string& name : names) text += (text.empty() ? "" : separator) + name;
  return text;
}

}  // namespace redgem::tools
