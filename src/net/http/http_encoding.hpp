#pragma once

#include <string>

#include "net/http/http_request.hpp"

namespace redgem::net {

// Decompresses an answer's body when its Content-Encoding is gzip (or x-gzip) or deflate (with
// zlib's wrapper, or raw, as some servers send it), and then takes that header and Content-Length
// (which counted the compressed bytes) out of its headers. An answer with no encoding, identity,
// or one the engine does not read (it never asks for those) stays as it came. Empty when all went
// well; otherwise why the body could not be read: corrupt or cut short, or larger than
// kMaxHttpDecodedBodyBytes once decompressed (the body is then left as it came).
std::string decode_body(HttpResponse& response);

}  // namespace redgem::net
