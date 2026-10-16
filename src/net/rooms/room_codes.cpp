#include "net/rooms/room_codes.hpp"

#include <random>
#include <string_view>

#include "net/wire.hpp"

namespace redgem::net {

namespace {

// Letters and digits that are not mistaken for one another when read aloud or typed (no I, O, 0, 1).
constexpr std::string_view kCodeAlphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
constexpr int kCodeTries = 64;  // at finding a code no room has

}  // namespace

std::string random_room_code() {
  std::random_device system_source;
  std::uniform_int_distribution<std::size_t> letter(0, kCodeAlphabet.size() - 1);
  std::string code;
  for (std::size_t i = 0; i < kRoomCodeLetters; ++i) code.push_back(kCodeAlphabet[letter(system_source)]);
  return code;
}

std::string unused_room_code(const std::function<bool(const std::string&)>& taken) {
  for (int attempt = 0; attempt < kCodeTries; ++attempt) {
    std::string code = random_room_code();
    if (!taken(code)) return code;
  }
  return {};
}

}  // namespace redgem::net
