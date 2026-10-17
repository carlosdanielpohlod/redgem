#pragma once

#include <map>
#include <string>
#include <string_view>

// A text with one value in it ("{count} objects") drawn every frame: each value's text is filled
// once and kept, so a frame that shows a value seen before allocates nothing (tr(key, values)
// would fill it again every frame). For the interfaces' drawing, by one thread.
namespace redgem {

class FilledTexts {
 public:
  // The texts of `key` (see core/translations.hpp), whose placeholder is {`name`}.
  FilledTexts(const char* key, const char* name) : key_(key), name_(name) {}

  // The text with this value in its placeholder. It stays where it is while its value is among
  // the few hundred kept: use it in the frame it was asked for.
  const char* with(std::string_view value);
  const char* with(long long number);

 private:
  const char* key_;
  const char* name_;
  std::map<std::string, std::string, std::less<>> texts_;  // by value
};

}  // namespace redgem
