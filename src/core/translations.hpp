#pragma once

#include <string>
#include <string_view>

#include "core/embed.hpp"
#include "core/texts.hpp"

// The engine's own texts, those its users read: the translation files built into this program
// (locales/<part>/<language>.json: each program carries the parts it shows), in its user's
// language. A program adds them and picks the language as it starts, before it starts a thread;
// from then on any thread may read them.
namespace redgem::translations {

// Adds a translation file built into the program, named for its language ("en.json"). One that
// is no translation file was built into the program broken: that is thrown.
void add_file(std::string_view file_name, const EmbeddedFile& file);
// Adds every file of a folder of them built into the program (cmake/EmbedFolder.cmake's list of
// {path, file}).
template <typename Files>
void add_files(const Files& files) {
  for (const auto& file : files) add_file(file.path, file.file);
}
// Shows the texts in `language` ("pt_BR", "pt-BR", "PT") from now on: in that language, else in
// that language without its region, else in another region of it, else in English.
void use_language(std::string_view language);
// The language the texts are shown in, as a language_key (see core/language_names.hpp).
const std::string& language();
// The language the user asked for, whether or not the engine has texts in it: a game may have
// some of its own (empty when the user and the system named none).
const std::string& requested_language();
// Every text added.
const Texts& all();

}  // namespace redgem::translations

namespace redgem {

// The text of `key` (such as editor.menu.file) in the language in use. What it points at stays where it
// is, so drawing it every frame costs nothing more than the lookup. The key itself when no
// translation file has it (tests/unit/translation_keys_test.cpp makes sure none is missing).
const char* tr(const char* key);
// The same text with its placeholders filled: tr(key, {{"game", name}}) fills {game}.
std::string tr(const char* key, const TextValues& values);

}  // namespace redgem
