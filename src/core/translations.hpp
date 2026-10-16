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
// Shows the texts in `language` ("pt") from now on, when there are some in it; else in English.
void use_language(std::string_view language);
// The language the texts are shown in.
const std::string& language();
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
