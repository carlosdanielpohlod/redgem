// The language the user reads, as each system says it.

#include "core/platform.hpp"

#include <algorithm>

#include "core/language_names.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI   // raylib defines its own Rectangle
#define NOUSER  // ... and CloseWindow, ShowCursor
#include <windows.h>
#endif

namespace redgem::platform {
namespace {

// Where the language and region of a locale's name end: pt_BR.UTF-8, de@euro.
constexpr const char* kAfterLanguage = ".@";
// The locales that name no language, only the system's plain behaviour.
constexpr const char* kPlainLocales[] = {"c", "posix"};

bool names_no_language(const std::string& language) {
  return std::find(std::begin(kPlainLocales), std::end(kPlainLocales), language) != std::end(kPlainLocales);
}

#ifndef _WIN32
// The environment variables that say the user's locale, the one that wins first.
constexpr const char* kLocaleVariables[] = {"LC_ALL", "LC_MESSAGES", "LANG"};
#endif

}  // namespace

std::string language_of_locale(std::string_view locale) {
  const std::string language = language_key(locale.substr(0, locale.find_first_of(kAfterLanguage)));
  return names_no_language(language) ? std::string() : language;
}

#ifdef _WIN32
std::string system_language() {
  wchar_t name[LOCALE_NAME_MAX_LENGTH];
  if (GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH) == 0) return "";
  // (A locale's name is plain ASCII, such as "pt-BR".)
  std::string ascii;
  for (const wchar_t* letter = name; *letter != L'\0'; ++letter) ascii.push_back(static_cast<char>(*letter));
  return language_of_locale(ascii);
}
#else
std::string system_language() {
  for (const char* variable : kLocaleVariables) {
    const std::string locale = environment(variable);
    if (!locale.empty()) return language_of_locale(locale);
  }
  return "";
}
#endif

}  // namespace redgem::platform
