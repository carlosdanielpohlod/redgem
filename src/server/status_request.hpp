#pragma once

#include <cstddef>
#include <string>

// What a dedicated server's status port makes of what a machine that connected to it said (see
// status_port.hpp): an HTTP request gets an HTTP answer; a connection that says nothing gets the
// bare document; anything else, too long or too slow, gets nothing.
namespace redgem::server {

// The most a request may be: a request line and a few headers. One longer is let go unanswered.
inline constexpr std::size_t kMostStatusRequestBytes = 4096;
// A connection that said nothing for this long is not sending a request: it is given the bare
// document (as `nc 127.0.0.1 <port>` does).
inline constexpr float kPlainAnswerAfterSeconds = 0.25f;
// A request not whole after this long is let go unanswered: a slow sender holds nothing for long.
inline constexpr float kStatusRequestSeconds = 2.0f;

enum class StatusReply {
  Wait,   // more may be on its way
  Http,   // an HTTP request arrived whole
  Plain,  // it said nothing, and keeps quiet
  Drop    // too long, too slow, or hung up: no answer
};

// What to do with a connection that said `received` in `waited` seconds, and has `hung_up` or not.
StatusReply status_reply(const std::string& received, float waited, bool hung_up);

// The answer to an HTTP request whose head is `request`: the document for a GET, its headers
// alone for a HEAD, a refusal for any other method. The connection is closed after it.
std::string http_status_answer(const std::string& request, const std::string& document);
// The answer to a connection that asked nothing: the document and an end of line.
std::string plain_status_answer(const std::string& document);

}  // namespace redgem::server
