#pragma once

#include <string>

// The secret a machine in a game gets its place back with after its connection dropped (see
// kResumeTokenBytes): unguessable, and compared so that how long a comparison takes tells nothing
// about how much of a guess was right.
namespace redgem::net {

// A new token: kResumeTokenBytes from the system's entropy, in hexadecimal. Empty when there is no
// randomness to be had (then no place is held for anyone).
std::string new_resume_token();
// Whether `offered` is `held`, in a time that depends only on their lengths. An empty token is
// nobody's.
bool same_resume_token(const std::string& offered, const std::string& held);

}  // namespace redgem::net
