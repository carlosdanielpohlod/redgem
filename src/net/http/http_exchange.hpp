#pragma once

#include <memory>

#include "core/clock.hpp"
#include "net/http/http_request.hpp"
#include "net/http/http_stream.hpp"
#include "net/http/http_trust.hpp"
#include "net/http/http_url.hpp"

namespace redgem::net {

// Carries out one request on the thread it is called from (it waits: never on a game's own):
// connects to the machine at `url`, speaks TLS with it for https:// (checked against `trust`, the
// system's when null), sends the request and reads the answer. Follows up to kMaxHttpRedirects
// redirects (see http_redirect.hpp), each address checked as the first was, and decompresses a
// gzip or deflate body (http_encoding.hpp). Gives up at the request's timeout, which covers its
// redirects too and is kept on `clock`, or once `cancel` gives it up. A plain http:// address
// that turns out to be neither this machine nor one of its network is not sent anything.
HttpResponse exchange(const HttpRequest& request, const Url& url, const std::shared_ptr<const TrustStore>& trust,
                      const HttpCancel& cancel, const Clock& clock);

}  // namespace redgem::net
