// What names people type are made into to name a folder on every system.

#include <doctest/doctest.h>

#include <string>

#include "core/folder_names.hpp"
#include "core/short_text.hpp"

using namespace redgem;

namespace {

constexpr std::size_t kLongName = 200;              // letters: far more than a folder's name holds
constexpr const char* kTwoByteLetter = "\xC3\xA9";  // é in UTF-8
constexpr std::size_t kTwoBytes = 2;

}  // namespace

TEST_CASE("a plain name is its own folder's name") {
  CHECK(safe_folder_name("My Studio") == "My Studio");
  CHECK(safe_folder_name("Coin Rush 2") == "Coin Rush 2");
  CHECK(safe_folder_name("v1.2 (beta)") == "v1.2 (beta)");
}

TEST_CASE("what no system's folder name holds becomes '_'") {
  CHECK(safe_folder_name("Coin: Rush?") == "Coin_ Rush_");
  CHECK(safe_folder_name("a<b>c\"d|e*f") == "a_b_c_d_e_f");
  CHECK(safe_folder_name("tab\there") == "tab_here");
  CHECK(safe_folder_name("line\nbreak") == "line_break");
  CHECK(safe_folder_name("\x1b[2J") == "_[2J");                // a terminal's escape
  CHECK(safe_folder_name("next\xC2\x85line") == "next_line");  // U+0085, a control of the second set
}

TEST_CASE("no name leads out of its folder") {
  CHECK(safe_folder_name("../../etc/passwd") == "_.._etc_passwd");
  CHECK(safe_folder_name("..\\..\\Windows") == "_.._Windows");
  CHECK(safe_folder_name("C:\\Games") == "C__Games");
  CHECK(safe_folder_name("/root") == "_root");
  CHECK(safe_folder_name("..").empty());
  CHECK(safe_folder_name(".").empty());
}

TEST_CASE("spaces and dots are taken off both ends, and a name of nothing else is no name") {
  CHECK(safe_folder_name("  Studio  ") == "Studio");
  CHECK(safe_folder_name(".hidden") == "hidden");
  CHECK(safe_folder_name("Game...") == "Game");
  CHECK(safe_folder_name(" . . ").empty());
  CHECK(safe_folder_name("").empty());
}

TEST_CASE("the names Windows keeps for its devices are given a '_', with or without an extension") {
  CHECK(safe_folder_name("CON") == "CON_");
  CHECK(safe_folder_name("nul") == "nul_");
  CHECK(safe_folder_name("Com1") == "Com1_");
  CHECK(safe_folder_name("LPT9") == "LPT9_");
  CHECK(safe_folder_name("aux.txt") == "aux_.txt");
  CHECK(safe_folder_name("PRN ") == "PRN_");
  CHECK(safe_folder_name("COM\xC2\xB9") == "COM\xC2\xB9_");  // COM with a superscript one
  CHECK(safe_folder_name("CONSOLE") == "CONSOLE");
  CHECK(safe_folder_name("Icon") == "Icon");
  CHECK(safe_folder_name("COM10") == "COM10");
}

TEST_CASE("letters of every language are kept whole") {
  CHECK(safe_folder_name("Est\xC3\xBA"
                         "dio A\xC3\xA7\xC3\xA3o") ==
        "Est\xC3\xBA"
        "dio A\xC3\xA7\xC3\xA3o");
  CHECK(safe_folder_name("\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0") == "\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0");
  CHECK(safe_folder_name("\xF0\x9F\x8E\xAE Arcade") == "\xF0\x9F\x8E\xAE Arcade");  // an emoji, four bytes
}

TEST_CASE("bytes that are not UTF-8 become '_'") {
  CHECK(safe_folder_name("a\xFF"
                         "b") == "a_b");
  CHECK(safe_folder_name("cut\xC3") == "cut_");  // a letter cut short
  CHECK(safe_folder_name("\xC0\xAF"
                         "x") == "__x");                        // '/' written too long
  CHECK(safe_folder_name("half\xED\xA0\x80") == "half___");     // half of a UTF-16 pair
  CHECK(safe_folder_name("big\xF4\x90\x80\x80") == "big____");  // past the last character
}

TEST_CASE("a long name is cut between whole letters") {
  std::string name;
  for (std::size_t letter = 0; letter < kLongName; ++letter) name += kTwoByteLetter;
  const std::string folder = safe_folder_name(name);
  CHECK(folder.size() <= kMaxFolderNameBytes);
  CHECK(folder.size() % kTwoBytes == 0);
  CHECK(utf8_prefix(folder, folder.size()) == folder);
  CHECK(safe_folder_name(std::string(kLongName, 'a')).size() <= kMaxFolderNameBytes);
  CHECK(safe_folder_name("CON" + std::string(kLongName, ' ')) == "CON_");
}
