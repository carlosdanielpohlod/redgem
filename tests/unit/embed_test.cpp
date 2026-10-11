#include <doctest/doctest.h>

#include <cstring>

#include "editor/ui/embedded_fonts.hpp"
#include "render/ui_font.hpp"

using namespace redgem;

namespace {

// What a TrueType file starts with.
constexpr unsigned char kTrueTypeMark[] = {0x00, 0x01, 0x00, 0x00};
constexpr int kSmallestFont = 100 * 1024;  // bytes: both fonts are several times this

bool is_font(const EmbeddedFile& file) {
  return file.data != nullptr && file.size > kSmallestFont &&
         std::memcmp(file.data, kTrueTypeMark, sizeof(kTrueTypeMark)) == 0;
}

}  // namespace

TEST_CASE("the font games draw texts with is built into the program, whole") { CHECK(is_font(embedded_ui_font())); }

TEST_CASE("the editor's code font is a file of its own, apart from the one games carry") {
  CHECK(is_font(fonts::code_font()));
  CHECK(fonts::code_font().data != embedded_ui_font().data);
  CHECK(fonts::code_font().size != embedded_ui_font().size);
}
