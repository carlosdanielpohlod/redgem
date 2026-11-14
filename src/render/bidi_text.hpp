#pragma once

#include <string>

namespace redgem {

// The text in the order its letters are drawn, left to right, by the Unicode Bidirectional
// Algorithm: a right-to-left run (Arabic, Hebrew) is reversed, its brackets mirrored, and each
// letter keeps the marks that follow it (vowel signs stay on their letter). Every line is its own
// paragraph, going the way of its first strong letter (left to right when it has none). Letters
// keep their own forms: Arabic is not joined (there is no shaping).
std::string visual_order(const std::string& text);

}  // namespace redgem
