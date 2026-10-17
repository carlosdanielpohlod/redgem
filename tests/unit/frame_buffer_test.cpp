#include <doctest/doctest.h>

#include <chrono>

#include "integration/wire_text.hpp"
#include "net/frame_buffer.hpp"

using namespace redgem::net;
using redgem::test::raw_frame;

namespace {

constexpr std::size_t kLengthBytes = 4;
constexpr std::uint8_t kHugeLength[] = {0x7F, 0xFF, 0xFF, 0xFF};  // far more than a message may be
// What a hostile peer fills a buffer with: as many bytes as one holds, in the smallest messages.
constexpr std::size_t kFloodBytes = 4 * kMaxFrameBytes;
constexpr std::chrono::seconds kFloodPatience{2};  // far more than cutting them apart takes

}  // namespace

TEST_CASE("a message is framed by its length and comes back whole") {
  const std::vector<std::uint8_t> bytes = raw_frame("hello");
  REQUIRE(bytes.size() == kLengthBytes + 5);
  CHECK(bytes[kLengthBytes - 1] == 5);
  FrameBuffer buffer;
  buffer.append(bytes.data(), bytes.size());
  CHECK(buffer.next() == std::string("hello"));
  CHECK_FALSE(buffer.next().has_value());
}

TEST_CASE("messages arriving in pieces, or several at once, are cut apart correctly") {
  std::vector<std::uint8_t> stream = raw_frame("first");
  const std::vector<std::uint8_t> second = raw_frame("");
  const std::vector<std::uint8_t> third = raw_frame("the third one");
  stream.insert(stream.end(), second.begin(), second.end());
  stream.insert(stream.end(), third.begin(), third.end());
  FrameBuffer buffer;
  std::vector<std::string> messages;
  for (const std::uint8_t byte : stream) {  // one byte at a time: the worst the network can do
    buffer.append(&byte, 1);
    while (const auto message = buffer.next()) messages.push_back(*message);
  }
  CHECK(messages == std::vector<std::string>{"first", "", "the third one"});

  FrameBuffer all_at_once;
  all_at_once.append(stream.data(), stream.size());
  CHECK(all_at_once.next() == std::string("first"));
  CHECK(all_at_once.next() == std::string(""));
  CHECK(all_at_once.next() == std::string("the third one"));
}

TEST_CASE("a message announced as longer than allowed breaks the buffer for good") {
  FrameBuffer buffer;
  buffer.append(kHugeLength, sizeof(kHugeLength));
  CHECK_FALSE(buffer.next().has_value());
  CHECK(buffer.broken());
  const std::vector<std::uint8_t> fine = raw_frame("fine");
  buffer.append(fine.data(), fine.size());
  CHECK_FALSE(buffer.next().has_value());
}

TEST_CASE("an empty message and the longest one allowed both come back whole") {
  FrameBuffer buffer;
  const std::vector<std::uint8_t> empty = raw_frame("");
  CHECK(empty.size() == kLengthBytes);
  buffer.append(empty.data(), empty.size());
  CHECK(buffer.next() == std::string());
  const std::string longest(kMaxFrameBytes, 'x');
  const std::vector<std::uint8_t> bytes = raw_frame(longest);
  buffer.append(bytes.data(), bytes.size());
  CHECK(buffer.next() == longest);
  CHECK_FALSE(buffer.broken());
}

TEST_CASE("one byte more than allowed breaks the buffer before any of the message is kept") {
  FrameBuffer buffer;
  const std::vector<std::uint8_t> bytes = raw_frame(std::string(kMaxFrameBytes + 1, 'x'));
  buffer.append(bytes.data(), kLengthBytes);
  CHECK_FALSE(buffer.next().has_value());
  CHECK(buffer.broken());
}

TEST_CASE("a length still arriving is waited for") {
  FrameBuffer buffer;
  const std::vector<std::uint8_t> bytes = raw_frame("late");
  buffer.append(bytes.data(), kLengthBytes - 1);
  CHECK_FALSE(buffer.next().has_value());
  CHECK_FALSE(buffer.broken());
  buffer.append(bytes.data() + kLengthBytes - 1, bytes.size() - (kLengthBytes - 1));
  CHECK(buffer.next() == std::string("late"));
}

TEST_CASE("a flood of tiny messages is cut apart in the time it takes to read them once") {
  const std::vector<std::uint8_t> tiny = raw_frame("{\"k\":\"i\"}");
  std::vector<std::uint8_t> flood;
  while (flood.size() < kFloodBytes) flood.insert(flood.end(), tiny.begin(), tiny.end());
  FrameBuffer buffer;
  buffer.append(flood.data(), flood.size());
  CHECK(buffer.buffered() == flood.size());
  const auto started = std::chrono::steady_clock::now();
  std::size_t messages = 0;
  while (buffer.next()) ++messages;
  CHECK(messages == flood.size() / tiny.size());
  CHECK(buffer.buffered() == 0);
  // (Moving what is left to the front for every message taken made this take many seconds.)
  CHECK(std::chrono::steady_clock::now() - started < kFloodPatience);
}

TEST_CASE("what arrives after messages were taken continues where they ended") {
  const std::vector<std::uint8_t> first = raw_frame("first");
  const std::vector<std::uint8_t> second = raw_frame("second");
  FrameBuffer buffer;
  buffer.append(first.data(), first.size());
  buffer.append(second.data(), kLengthBytes + 1);
  CHECK(buffer.next() == std::string("first"));
  CHECK_FALSE(buffer.next().has_value());
  CHECK(buffer.buffered() == kLengthBytes + 1);
  buffer.append(second.data() + kLengthBytes + 1, second.size() - kLengthBytes - 1);
  CHECK(buffer.next() == std::string("second"));
  CHECK(buffer.buffered() == 0);
}
