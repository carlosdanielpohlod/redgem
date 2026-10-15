#include "net/http_exchange.hpp"

#include "net/http_message.hpp"
#include "net/http_response_parser.hpp"
#include "net/http_stream.hpp"

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

std::unique_ptr<HttpStream> open_stream(Socket socket, const Url& url, const std::shared_ptr<const TrustStore>& trust,
                                        std::string& error) {
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
  return flow;
}

HttpResponse converse(HttpStream& stream, const std::string& request, const Patience& patience) {
  std::vector<std::uint8_t> outgoing(request.begin(), request.end());
  HttpResponseParser parser;
  while (parser.state() == HttpResponseParser::State::Reading) {
    const Flow flow = step(stream, outgoing, parser);
    if (flow == Flow::Failed) return HttpResponse::failure(stream.error());
    const bool can_go_on = !waits(flow) || patience.wait(stream.socket(), flow == Flow::WantWrite);
    if (!can_go_on) return HttpResponse::failure(patience.why());
  }
  return parser.take();
}

}  // namespace

HttpResponse exchange(const HttpRequest& request, const Url& url, const std::shared_ptr<const TrustStore>& trust,
                      const std::atomic<bool>& cancelled) {
  const Patience patience(request.timeout_seconds, cancelled);
  std::string error;
  Socket socket = connect(url, patience, error);
  if (!socket.valid()) return HttpResponse::failure(error);
  std::unique_ptr<HttpStream> stream = open_stream(std::move(socket), url, trust, error);
  if (!stream) return HttpResponse::failure(error);
  return converse(*stream, request_text(request, url), patience);
}

}  // namespace redgem::net
