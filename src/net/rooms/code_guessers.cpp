#include "net/rooms/code_guessers.hpp"

#include <algorithm>
#include <iterator>

#include "net/wire.hpp"

namespace redgem::net {

namespace {

// Wrong codes before an address is kept out: a whole party behind one address may each get the
// code wrong twice (someone read it out badly). Guessing a room takes millions more.
constexpr float kMaxWrongCodes = 2.0f * kMaxPeers;
constexpr float kKeptOutSeconds = 120.0f;         // kept out for this long
constexpr float kForgetWrongCodeSeconds = 30.0f;  // one wrong code is forgotten after this

}  // namespace

bool CodeGuessers::keeps_out(const std::string& address) const {
  const auto visitor = visitors_.find(address);
  return visitor != visitors_.end() && visitor->second.kept_out();
}

void CodeGuessers::note_wrong_code(const std::string& address) {
  Visitor& visitor = visitors_[address];
  visitor.wrong_codes += 1.0f;
  if (visitor.wrong_codes >= kMaxWrongCodes) visitor.kept_out_for = kKeptOutSeconds;
}

void CodeGuessers::forgive(float seconds) {
  for (auto visitor = visitors_.begin(); visitor != visitors_.end();) {
    Visitor& record = visitor->second;
    record.kept_out_for = std::max(0.0f, record.kept_out_for - seconds);
    record.wrong_codes = std::max(0.0f, record.wrong_codes - seconds / kForgetWrongCodeSeconds);
    visitor = record.forgiven() ? visitors_.erase(visitor) : std::next(visitor);
  }
}

}  // namespace redgem::net
