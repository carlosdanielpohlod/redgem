#include "server/status_request.hpp"

namespace redgem::server {

namespace {

constexpr const char* kHeadEnd = "\r\n\r\n";
constexpr const char* kBareHeadEnd = "\n\n";  // (a request typed by hand may end its lines so)
constexpr const char* kGet = "GET ";
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

// Whether a connection that said nothing will not: it kept quiet long enough, or hung up.
bool will_not_speak(float waited, bool hung_up) { return hung_up || waited >= kPlainAnswerAfterSeconds; }

// Whether a connection asks for nothing: it said nothing, and will not.
bool asks_nothing(const std::string& received, float waited, bool hung_up) {
  return received.empty() && will_not_speak(waited, hung_up);
}

// Whether a request that is not whole never will be in time.
bool gave_up(float waited, bool hung_up) { return hung_up || waited >= kStatusRequestSeconds; }

bool is_get(const std::string& request) { return request.rfind(kGet, 0) == 0; }

std::string answer(const std::string& status_and_headers, const std::string& body) {
  return status_and_headers + kLength + std::to_string(body.size()) + kLineEnd + kClosingHeaders + kLineEnd + body;
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
  if (!is_get(request)) return answer(kNotAllowed, {});
  return answer(std::string(kOk) + kJsonType, document);
}

std::string plain_status_answer(const std::string& document) { return document + "\n"; }

}  // namespace redgem::server
