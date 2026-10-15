#include "net/http_response_parser.hpp"

#include <algorithm>
#include <cctype>

#include "net/http_message.hpp"

namespace redgem::net {

namespace {

constexpr int kNoContent = 204;
constexpr int kNotModified = 304;
constexpr std::size_t kMostLengthDigits = 19;  // what a 64-bit length takes
constexpr const char* kChunked = "chunked";
constexpr const char* kTransferEncoding = "transfer-encoding";
constexpr const char* kContentLength = "content-length";

std::string too_large() { return "the answer is larger than " + std::to_string(kMaxHttpBodyBytes) + " bytes"; }

bool ends_with(const std::string& text, const std::string& end) {
  return text.size() >= end.size() && text.compare(text.size() - end.size(), end.size(), end) == 0;
}

}  // namespace

void HttpResponseParser::feed(const char* bytes, std::size_t count) {
  if (state_ != State::Reading) return;
  pending_.append(bytes, count);
  read_heads();
  if (reading_body()) read_body();
}

void HttpResponseParser::end_of_stream() {
  if (state_ != State::Reading) return;
  if (lasts_until_the_end()) {
    state_ = State::Done;
    return;
  }
  fail("the connection ended before the whole answer came");
}

HttpResponse HttpResponseParser::take() { return std::move(response_); }

void HttpResponseParser::choose_framing() {
  if (bodiless()) {
    state_ = State::Done;
  } else if (ends_with(lower_case(header(kTransferEncoding)), kChunked)) {
    framing_ = Framing::Chunked;
  } else if (!header(kContentLength).empty()) {
    read_length(header(kContentLength));
  }
}

void HttpResponseParser::read_length(const std::string& value) {
  if (!is_decimal(value, kMostLengthDigits)) return fail("the answer's length makes no sense");
  length_ = std::stoull(value);
  if (length_ > kMaxHttpBodyBytes) return fail(too_large());
  framing_ = Framing::Length;
}

void HttpResponseParser::read_body() {
  if (framing_ == Framing::Length) read_measured();
  if (framing_ == Framing::Chunked) read_chunks();
  if (framing_ == Framing::UntilClose) take_pending(pending_.size());
  if (response_.body.size() > kMaxHttpBodyBytes) fail(too_large());
}

void HttpResponseParser::read_measured() {
  take_pending(std::min(length_ - response_.body.size(), pending_.size()));
  if (response_.body.size() == length_) state_ = State::Done;
}

void HttpResponseParser::read_chunks() {
  if (!chunks_.read(pending_, response_.body)) return fail("the answer's chunks make no sense");
  if (chunks_.done()) state_ = State::Done;
}

void HttpResponseParser::take_pending(std::size_t count) {
  response_.body.append(pending_, 0, count);
  pending_.erase(0, count);
}

bool HttpResponseParser::bodiless() const { return response_.status == kNoContent || response_.status == kNotModified; }

std::string HttpResponseParser::header(const std::string& lower_name) const {
  const auto found = std::find_if(response_.headers.begin(), response_.headers.end(),
                                  [&lower_name](const auto& entry) { return entry.first == lower_name; });
  return found != response_.headers.end() ? found->second : std::string();
}

void HttpResponseParser::fail(const std::string& why) {
  state_ = State::Failed;
  response_ = HttpResponse::failure(why);
}

}  // namespace redgem::net
