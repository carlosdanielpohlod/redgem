#include "net/http_chunks.hpp"

#include <algorithm>
#include <cctype>

#include "net/http_message.hpp"
#include "net/http_request.hpp"

namespace redgem::net {

namespace {

constexpr int kHexadecimal = 16;
constexpr std::size_t kMostSizeDigits = 16;  // what a 64-bit length takes
constexpr char kExtensionStart = ';';        // a chunk's extensions follow its length: none is used
constexpr char kLineFeed = '\n';
constexpr char kCarriageReturn = '\r';

bool is_chunk_size(const std::string& digits) {
  const bool hexadecimal = std::all_of(digits.begin(), digits.end(),
                                       [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; });
  return hexadecimal && !digits.empty() && digits.size() <= kMostSizeDigits;
}

// A line that ended with CR LF, of which only the LF was taken off.
bool ends_in_return(const std::string& line) { return !line.empty() && line.back() == kCarriageReturn; }

}  // namespace

bool take_line(std::string& text, std::string& line) {
  const std::size_t end = text.find(kLineFeed);
  if (end == std::string::npos) return false;
  line = text.substr(0, end);
  if (ends_in_return(line)) line.pop_back();
  text.erase(0, end + 1);
  return true;
}

bool ChunkedBody::read(std::string& pending, std::string& body) {
  bool waiting = false;
  while (!done() && !waiting) {
    if (!step(pending, body, waiting)) return false;
  }
  // (What is left waits for the rest of a line: one that never ends is no chunk.)
  return pending.size() <= kMaxHttpHeadBytes;
}

bool ChunkedBody::step(std::string& pending, std::string& body, bool& waiting) {
  switch (part_) {
    case Part::Size:
      return read_size(pending, waiting);
    case Part::Data:
      read_data(pending, body, waiting);
      return true;
    case Part::DataEnd:
      return read_data_end(pending, waiting);
    default:
      return read_trailer(pending, waiting);
  }
}

bool ChunkedBody::read_size(std::string& pending, bool& waiting) {
  std::string line;
  waiting = !take_line(pending, line);
  if (waiting) return true;
  const std::string digits = trimmed(line.substr(0, line.find(kExtensionStart)));
  if (!is_chunk_size(digits)) return false;
  left_ = std::stoull(digits, nullptr, kHexadecimal);
  part_ = left_ == 0 ? Part::Trailer : Part::Data;
  return left_ <= kMaxHttpBodyBytes;
}

void ChunkedBody::read_data(std::string& pending, std::string& body, bool& waiting) {
  const std::size_t taken = std::min(left_, pending.size());
  body.append(pending, 0, taken);
  pending.erase(0, taken);
  left_ -= taken;
  waiting = left_ > 0;
  if (!waiting) part_ = Part::DataEnd;
}

bool ChunkedBody::read_data_end(std::string& pending, bool& waiting) {
  std::string line;
  waiting = !take_line(pending, line);
  if (waiting) return true;
  part_ = Part::Size;
  return line.empty();  // a chunk's bytes end with a line break, and nothing before it
}

bool ChunkedBody::read_trailer(std::string& pending, bool& waiting) {
  std::string line;
  waiting = !take_line(pending, line);
  if (!waiting && line.empty()) part_ = Part::Done;
  return true;
}

}  // namespace redgem::net
