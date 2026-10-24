#pragma once

#include <string>

#include "net/decimal_text.hpp"

#include "net/http/http_request.hpp"
#include "net/http/http_url.hpp"

namespace redgem::net {

// The bytes of a request as they go out: HTTP/1.1, the connection closed once the answer came.
// The request is one request_problem found nothing wrong with.
std::string request_text(const HttpRequest& request, const Url& url);

// A header's name as answers are read and compared: in lower case.
std::string lower_case(std::string text);
// A header's value as it is meant: without the spaces and tabs around it.
std::string trimmed(const std::string& text);

}  // namespace redgem::net
