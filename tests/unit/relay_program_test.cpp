#include <doctest/doctest.h>

#include <string>

#include "net/rooms/relay_program.hpp"

using namespace redgem::net;

namespace {

constexpr std::uint16_t kChosenPort = 9000;
constexpr std::uint16_t kLargestPort = 65535;
constexpr std::uint16_t kWebPort = 80;

}  // namespace

TEST_CASE("the relay program listens on the usual port, or on the one it is told") {
  const RelayCommand plain = parse_relay_command({});
  REQUIRE(plain.ok);
  CHECK_FALSE(plain.help);
  CHECK(plain.port == kDefaultRelayPort);
  const RelayCommand chosen = parse_relay_command({"--port", "9000"});
  REQUIRE(chosen.ok);
  CHECK(chosen.port == kChosenPort);
  // (Port 0 is no port to meet on: it stands for the usual one.)
  CHECK(parse_relay_command({"--port", "0"}).port == kDefaultRelayPort);
}

TEST_CASE("a relay's port is a number of at most five digits, up to the largest there is") {
  CHECK(parse_relay_command({"--port", "65535"}).port == kLargestPort);
  CHECK(parse_relay_command({"--port", "00080"}).port == kWebPort);
  for (const char* wrong : {"65536", "70000", "000080", "80a", "-80", "8 0", "many", ""}) {
    CAPTURE(wrong);
    const RelayCommand refused = parse_relay_command({"--port", wrong});
    CHECK_FALSE(refused.ok);
    CHECK(refused.error == "--port needs a number up to 65535");
  }
  CHECK_FALSE(parse_relay_command({"--port"}).ok);
  CHECK_FALSE(parse_relay_command({"--port", "9000", "9001"}).ok);
}

TEST_CASE("what the relay program does not understand is refused by name") {
  CHECK(parse_relay_command({"somewhere"}).error == "unexpected argument somewhere");
  CHECK(parse_relay_command({"--bogus"}).error == "unexpected argument --bogus");
  // The editor's way of saying it, from when the relay was one of its commands.
  CHECK_FALSE(parse_relay_command({"relay"}).ok);
}

TEST_CASE("the relay program's help wins over the rest of its line and names the program") {
  for (const char* option : {"--help", "-h"}) {
    const RelayCommand help = parse_relay_command({"--port", "many", option});
    CHECK(help.ok);
    CHECK(help.help);
  }
  const std::string text = relay_usage();
  CHECK(text.rfind("usage:", 0) == 0);
  CHECK(text.find("redgem-relay [--port <number>]") != std::string::npos);
  CHECK(text.find(std::to_string(kDefaultRelayPort)) != std::string::npos);
}

TEST_CASE("the relay program fails, with the reason said, on a line it cannot read") {
  constexpr int kUsageError = 2;
  CHECK(run_relay_program({"--port", "many"}) == kUsageError);
  CHECK(run_relay_program({"--help"}) == 0);
}
