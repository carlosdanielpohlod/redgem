#pragma once

#include <cstddef>
#include <string>

#include "net/http/http_chunks.hpp"
#include "net/http/http_request.hpp"

namespace redgem::net {

// Reads an answer as its bytes arrive, in whatever pieces: the status line and the headers, then
// the body, of the length the headers give, in chunks, or up to the end of the connection. An
// answer is untrusted: its headers and its body are held to kMaxHttpHeadBytes and
// kMaxHttpBodyBytes, and anything that is not HTTP fails it.
class HttpResponseParser {
 public:
  enum class State { Reading, Done, Failed };

  void feed(const char* bytes, std::size_t count);
  // The connection ended: an answer that lasts until then is whole, any other is cut short.
  void end_of_stream();
  // The connection ended in a way that does not vouch for its end (TLS with no close_notify): only
  // an answer whose length or chunks said it was whole is.
  void cut_off();
  State state() const { return state_; }
  // The answer once Done; once Failed, one with the reason as its error.
  HttpResponse take();

 private:
  enum class Framing { Length, Chunked, UntilClose };

  bool reading_head() const { return state_ == State::Reading && !head_read_; }
  bool reading_body() const { return state_ == State::Reading && head_read_; }
  // Whether the answer is whole once the connection ends: its body was to last until then.
  bool lasts_until_the_end() const { return head_read_ && framing_ == Framing::UntilClose; }
  void read_heads();
  // The status line and headers (a head of an interim answer, 1xx, is passed over).
  void read_head(const std::string& head);
  bool read_status(const std::string& line);
  bool read_header(const std::string& line);
  void choose_framing();
  void read_length(const std::string& value);
  void read_body();
  void read_measured();
  void read_chunks();
  // Moves the first `count` bytes that arrived into the body.
  void take_pending(std::size_t count);
  // Whether the answer's status says it has no body at all.
  bool bodiless() const;
  std::string header(const std::string& lower_name) const;
  void fail(const std::string& why);

  State state_ = State::Reading;
  bool head_read_ = false;
  Framing framing_ = Framing::UntilClose;
  std::size_t length_ = 0;  // of the body, with Framing::Length
  std::string pending_;     // what arrived and was not read yet
  ChunkedBody chunks_;
  HttpResponse response_;
};

}  // namespace redgem::net
