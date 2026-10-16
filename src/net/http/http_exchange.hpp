#pragma once

#include <atomic>
#include <memory>

#include "net/http/http_request.hpp"
#include "net/http/http_trust.hpp"
#include "net/http/http_url.hpp"

namespace redgem::net {

// Carries out one request on the thread it is called from (it waits: never on a game's own):
// connects to the machine at `url`, speaks TLS with it for https:// (checked against `trust`, the
// system's when null), sends the request and reads the answer. Gives up at the request's timeout,
// or soon after `cancelled` is set. A plain http:// address that turns out to be neither this
// machine nor one of its network is not sent anything.
HttpResponse exchange(const HttpRequest& request, const Url& url, const std::shared_ptr<const TrustStore>& trust,
                      const std::atomic<bool>& cancelled);

}  // namespace redgem::net
