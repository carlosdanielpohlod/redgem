#include "server/status_request.hpp"

namespace redgem::server {

namespace {

constexpr const char* kHeadEnd = "\r\n\r\n";
constexpr const char* kBareHeadEnd = "\n\n";  // (a request typed by hand may end its lines so)
constexpr const char* kGet = "GET ";
constexpr const char* kHead = "HEAD ";
constexpr const char* kEmptyLines = "\r\n";  // (a request may follow empty lines: they are skipped)
constexpr const char* kOk = "HTTP/1.1 200 OK\r\n";
constexpr const char* kNotAllowed = "HTTP/1.1 405 Method Not Allowed\r\nAllow: GET\r\n";
constexpr const char* kJsonType = "Content-Type: application/json\r\n";
constexpr const char* kLength = "Content-Length: ";
// Every answer is the last on its connection, and is about now: nothing keeps it.
constexpr const char* kClosingHeaders = "Cache-Control: no-store\r\nConnection: close\r\n";
constexpr const char* kLineEnd = "\r\n";

bool contains(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

bool head_arrived(const std::string& received) {
  return contains(received, kHeadEnd) || contains(received, kBareHeadEnd);
}

// Whether a connection asks for nothing: it is still there, and said nothing for long enough.
// (One that hung up having said nothing, a check that the port is open, is not answered.)
bool asks_nothing(const std::string& received, float waited, bool hung_up) {
  return received.empty() && !hung_up && waited >= kPlainAnswerAfterSeconds;
}

// Whether a request that is not whole never will be in time.
bool gave_up(float waited, bool hung_up) { return hung_up || waited >= kStatusRequestSeconds; }

// The request with the empty lines before it skipped.
std::string request_line_onwards(const std::string& request) {
  const std::size_t start = request.find_first_not_of(kEmptyLines);
  return start == std::string::npos ? std::string() : request.substr(start);
}

bool starts_with(const std::string& text, const char* start) { return text.rfind(start, 0) == 0; }

std::string answer(const std::string& status_and_headers, const std::string& body) {
  return status_and_headers + kLength + std::to_string(body.size()) + kLineEnd + kClosingHeaders + kLineEnd + body;
}

// An answer's status line and headers, its body (at its end) left out.
std::string without_body(const std::string& whole, const std::string& body) {
  return whole.substr(0, whole.size() - body.size());
}

}  // namespace

StatusReply status_reply(const std::string& received, float waited, bool hung_up) {
  if (received.size() > kMostStatusRequestBytes) return StatusReply::Drop;
  if (head_arrived(received)) return StatusReply::Http;
  if (asks_nothing(received, waited, hung_up)) return StatusReply::Plain;
  if (gave_up(waited, hung_up)) return StatusReply::Drop;
  return StatusReply::Wait;
}

std::string http_status_answer(const std::string& request, const std::string& document) {
  const std::string asked = request_line_onwards(request);
  const std::string ok = std::string(kOk) + kJsonType;
  if (starts_with(asked, kGet)) return answer(ok, document);
  // (HEAD: what GET would answer, without the document.)
  if (starts_with(asked, kHead)) return without_body(answer(ok, document), document);
  return answer(kNotAllowed, {});
}

std::string plain_status_answer(const std::string& document) { return document + "\n"; }

}  // namespace redgem::server
