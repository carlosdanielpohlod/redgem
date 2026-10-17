// The text a Radiance picture starts with: a line naming the kind, settings one a line up to an
// empty line, then the size ("-Y <height> +X <width>").

#include <algorithm>
#include <charconv>
#include <string_view>

#include "core/image_header.hpp"
#include "core/radiance_image.hpp"

namespace redgem {
namespace {

constexpr std::string_view kMagic = "#?";
constexpr std::string_view kFormatKey = "FORMAT=";
constexpr std::string_view kRgbeFormat = "32-bit_rle_rgbe";
constexpr std::string_view kRowsFromTop = "-Y ";
constexpr std::string_view kColumnsFromLeft = " +X ";
constexpr std::size_t kMaxHeaderBytes = 65536;  // real headers are a few lines
constexpr char kCarriageReturn = '\r';          // what a line written on Windows ends with, before its newline

// One line of text: what it says (without its newline, nor a carriage return before it) and where
// the next starts.
struct Line {
  std::string_view text;
  std::size_t next = 0;
};

std::string_view without_carriage_return(std::string_view text) {
  if (!text.empty() && text.back() == kCarriageReturn) text.remove_suffix(1);
  return text;
}

std::optional<Line> line_at(const std::uint8_t* data, std::size_t size, std::size_t at) {
  const std::size_t limit = std::min(size, kMaxHeaderBytes);
  for (std::size_t end = at; end < limit; ++end) {
    if (data[end] != '\n') continue;
    return Line{without_carriage_return({reinterpret_cast<const char*>(data) + at, end - at}), end + 1};
  }
  return std::nullopt;
}

bool starts_with(std::string_view text, std::string_view start) { return text.substr(0, start.size()) == start; }

// A setting the picture cannot be read with: another kind of pixels than RGBE.
bool refused_setting(std::string_view line) {
  return starts_with(line, kFormatKey) && line.substr(kFormatKey.size()) != kRgbeFormat;
}

// A side's length, all of `text`, from one pixel up to the most decoded.
std::optional<std::uint32_t> side_of(std::string_view text) {
  std::uint32_t side = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), side);
  const bool whole = error == std::errc() && end == text.data() + text.size();
  if (!whole || side == 0 || side > kMaxImageSide) return std::nullopt;
  return side;
}

// "-Y <height> +X <width>": rows from the top, each from the left.
std::optional<RadianceLayout> size_line(std::string_view line, std::size_t next) {
  if (!starts_with(line, kRowsFromTop)) return std::nullopt;
  const std::string_view rest = line.substr(kRowsFromTop.size());
  const std::size_t columns = rest.find(kColumnsFromLeft);
  if (columns == std::string_view::npos) return std::nullopt;
  const auto height = side_of(rest.substr(0, columns));
  const auto width = side_of(rest.substr(columns + kColumnsFromLeft.size()));
  if (!height || !width) return std::nullopt;
  return RadianceLayout{*width, *height, next};
}

}  // namespace

bool is_radiance(const std::uint8_t* data, std::size_t size) {
  return data != nullptr && size >= kMagic.size() &&
         std::string_view(reinterpret_cast<const char*>(data), kMagic.size()) == kMagic;
}

std::optional<RadianceLayout> radiance_layout(const std::uint8_t* data, std::size_t size) {
  if (!is_radiance(data, size)) return std::nullopt;
  std::optional<Line> line = line_at(data, size, 0);
  while (line && !line->text.empty()) {
    if (refused_setting(line->text)) return std::nullopt;
    line = line_at(data, size, line->next);
  }
  if (!line) return std::nullopt;
  const std::optional<Line> size_text = line_at(data, size, line->next);
  if (!size_text) return std::nullopt;
  return size_line(size_text->text, size_text->next);
}

}  // namespace redgem
