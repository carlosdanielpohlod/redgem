#include "net/http/http_exchange.hpp"

#include <optional>
#include <vector>

#include "net/http/http_encoding.hpp"
#include "net/http/http_message.hpp"
#include "net/http/http_redirect.hpp"
#include "net/http/http_response_parser.hpp"
#include "net/http/http_stream.hpp"
#include "net/private_address.hpp"

namespace redgem::net {

namespace {

bool waits(Flow flow) { return flow == Flow::WantRead || flow == Flow::WantWrite; }

// A connection to the machine at `url`, made; invalid, with `error`, when none could be.
Socket connect(const Url& url, const Patience& patience, std::string& error) {
  Socket socket = Socket::connect_to(url.host, url.port, error);
  while (socket.valid()) {
    const IoStatus status = socket.connect_status();
    if (status == IoStatus::Ok) return socket;
    if (status == IoStatus::Closed) {
      error = "cannot connect to " + url.authority();
    } else if (!patience.wait(socket, true)) {
      error = patience.why();
    }
    if (!error.empty()) socket.close();
  }
  return socket;
}

std::string metadata_refusal(const std::string& address) {
  return "no request goes to a cloud's metadata address (" + address + ")";
}

std::unique_ptr<HttpStream> open_stream(Socket socket, const Url& url, const std::shared_ptr<const TrustStore>& trust,
                                        std::string& error) {
  // (Told apart by where the name led, as below: a name of the game's choosing can lead there too.)
  if (is_cloud_metadata_address(socket.peer_address())) {
    error = metadata_refusal(socket.peer_address());
    return nullptr;
  }
  if (url.secure) return secure_stream(std::move(socket), url.host, trust ? trust : TrustStore::system(), error);
  // (Plain text only where nobody on the internet can read it: told apart by where the name led.)
  if (is_private_address(socket.peer_address())) return plain_stream(std::move(socket));
  error = "http:// only reaches this machine and its local network: use https://";
  return nullptr;
}

// One step: sending what is left of the request, or once it all went, reading the answer.
Flow step(HttpStream& stream, std::vector<std::uint8_t>& outgoing, HttpResponseParser& parser) {
  if (!outgoing.empty()) return stream.send(outgoing);
  std::vector<std::uint8_t> arrived;
  const Flow flow = stream.receive(arrived);
  parser.feed(reinterpret_cast<const char*>(arrived.data()), arrived.size());
  if (flow == Flow::Ended) parser.end_of_stream();
  if (flow == Flow::Cut) parser.cut_off();
  return flow;
}

// Whether the connection ended (cleanly or not) before the whole request went: nothing more can
// go, nor come.
bool ended(Flow flow) { return flow == Flow::Ended || flow == Flow::Cut; }

// One turn of the exchange: the step taken, and the request's failure when that step ended it.
std::optional<HttpResponse> turn(HttpStream& stream, std::vector<std::uint8_t>& outgoing, HttpResponseParser& parser,
                                 const Patience& patience) {
  const bool sending = !outgoing.empty();
  const Flow flow = step(stream, outgoing, parser);
  if (flow == Flow::Failed) return HttpResponse::failure(stream.error());
  if (sending && ended(flow)) return HttpResponse::failure("the connection ended before the request was sent");
  const bool can_go_on = !waits(flow) || patience.wait(stream.socket(), flow == Flow::WantWrite);
  if (!can_go_on) return HttpResponse::failure(patience.why());
  return std::nullopt;
}

HttpResponse converse(HttpStream& stream, const std::string& request, const Patience& patience) {
  std::vector<std::uint8_t> outgoing(request.begin(), request.end());
  HttpResponseParser parser;
  // (The time and the game ending are looked at on every turn: no flow keeps the loop going past them.)
  while (parser.state() == HttpResponseParser::State::Reading) {
    if (patience.over()) return HttpResponse::failure(patience.why());
    if (auto failed = turn(stream, outgoing, parser, patience)) return std::move(*failed);
  }
  return parser.take();
}

// One request to one address, a redirect in its answer not followed. Every address a request is
// sent to goes through here, its checks and all: a redirect cannot lead around them.
HttpResponse exchange_once(const HttpRequest& request, const Url& url, const std::shared_ptr<const TrustStore>& trust,
                           const Patience& patience) {
  // (An address written in numbers is refused before connecting; one a name leads to, once connected.)
  if (is_cloud_metadata_address(url.host)) return HttpResponse::failure(metadata_refusal(url.host));
  std::string error;
  Socket socket = connect(url, patience, error);
  if (!socket.valid()) return HttpResponse::failure(error);
  std::unique_ptr<HttpStream> stream = open_stream(std::move(socket), url, trust, error);
  if (!stream) return HttpResponse::failure(error);
  return converse(*stream, request_text(request, url), patience);
}

// Follows one more redirect, unless as many as a request follows were already.
bool redirect_again(int redirects, HttpRequest& request, Url& url, const HttpResponse& answer, std::string& error) {
  if (redirects < kMaxHttpRedirects) return follow_redirect(request, url, answer, error);
  error = "the answer redirected more than " + std::to_string(kMaxHttpRedirects) + " times";
  return false;
}

// Whether the request goes on to where its answer sends it.
bool goes_on(const HttpRequest& request, const HttpResponse& response) {
  return request.follow_redirects && is_followed_redirect(response);
}

// The answer as the game gets it: where it came from, and its body decompressed. A body that
// cannot be is the answer's error, its status and headers kept (and its body dropped).
HttpResponse finished(HttpResponse response, const Url& url) {
  response.url = url_text(url);
  if (!response.error.empty()) return response;
  response.error = decode_body(response);
  if (!response.error.empty()) response.body.clear();
  return response;
}

}  // namespace

HttpResponse exchange(const HttpRequest& request, const Url& url, const std::shared_ptr<const TrustStore>& trust,
                      const std::atomic<bool>& cancelled) {
  // (The timeout is the whole request's, its redirects and all.)
  const Patience patience(request.timeout_seconds, cancelled);
  HttpRequest next = request;
  Url at = url;
  for (int redirects = 0;; ++redirects) {
    HttpResponse response = exchange_once(next, at, trust, patience);
    if (!goes_on(next, response)) return finished(std::move(response), at);
    std::string error;
    if (!redirect_again(redirects, next, at, response, error)) return finished(HttpResponse::failure(error), at);
  }
}

}  // namespace redgem::net
