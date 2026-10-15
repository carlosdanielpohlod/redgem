// HttpResponseParser: an answer's head, its status line and its headers.

#include <cstring>

#include "net/http_message.hpp"
#include "net/http_response_parser.hpp"

namespace redgem::net {

namespace {

constexpr const char* kHeadEnd = "\r\n\r\n";
constexpr const char* kLineEnd = "\r\n";
constexpr const char* kVersionPrefix = "HTTP/1.";
constexpr std::size_t kStatusStart = std::char_traits<char>::length("HTTP/1.1 ");
constexpr std::size_t kStatusDigits = 3;
constexpr int kFirstFinalStatus = 200;  // below it, interim answers: the real one follows
constexpr char kHeaderSeparator = ':';

bool is_status_line(const std::string& line) {
  return line.rfind(kVersionPrefix, 0) == 0 && line.size() >= kStatusStart + kStatusDigits;
}

// Whether a header's line has a name before its colon.
bool names_something(std::size_t colon) { return colon != std::string::npos && colon > 0; }

}  // namespace

void HttpResponseParser::read_heads() {
  while (reading_head()) {
    const std::size_t end = pending_.find(kHeadEnd);
    if (end == std::string::npos) {
      if (pending_.size() > kMaxHttpHeadBytes) fail("the answer's headers are too long");
      return;
    }
    const std::string head = pending_.substr(0, end);
    pending_.erase(0, end + std::strlen(kHeadEnd));
    read_head(head);
  }
}

void HttpResponseParser::read_head(const std::string& head) {
  std::string lines = head + kLineEnd;
  std::string line;
  take_line(lines, line);
  if (!read_status(line)) return fail("the answer is not HTTP");
  response_.headers.clear();
  while (take_line(lines, line)) {
    if (!read_header(line)) return fail("a header of the answer makes no sense");
  }
  if (response_.status < kFirstFinalStatus) return;
  head_read_ = true;
  choose_framing();
}

bool HttpResponseParser::read_status(const std::string& line) {
  if (!is_status_line(line)) return false;
  const std::string code = line.substr(kStatusStart, kStatusDigits);
  if (!is_decimal(code, kStatusDigits)) return false;
  response_.status = std::stoi(code);
  return true;
}

bool HttpResponseParser::read_header(const std::string& line) {
  const std::size_t colon = line.find(kHeaderSeparator);
  if (!names_something(colon)) return false;
  response_.headers.emplace_back(lower_case(line.substr(0, colon)), trimmed(line.substr(colon + 1)));
  return true;
}

}  // namespace redgem::net
