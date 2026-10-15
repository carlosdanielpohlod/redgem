#pragma once

#include <map>
#include <string>

namespace redgem::net {

// The addresses that asked for rooms nobody has, on a relay or a dedicated server. A room's code
// is all that keeps strangers out of it: each wrong code is held against its address for a while,
// and an address with too many is kept out for longer.
class CodeGuessers {
 public:
  // Whether `address` is kept out for guessing.
  bool keeps_out(const std::string& address) const;
  // A wrong code from `address`: a typo, or one try of many.
  void note_wrong_code(const std::string& address);
  // Time passing: wrong codes are forgotten, and addresses kept out are let back in.
  void forgive(float seconds);

 private:
  // What is remembered about an address: its wrong codes (forgotten with time), and for how long
  // it is kept out for guessing too much.
  struct Visitor {
    float wrong_codes = 0.0f;
    float kept_out_for = 0.0f;

    bool kept_out() const { return kept_out_for > 0.0f; }
    // Nothing is held against it any more: it need not be remembered.
    bool forgiven() const { return !kept_out() && wrong_codes <= 0.0f; }
  };

  std::map<std::string, Visitor> visitors_;
};

}  // namespace redgem::net
