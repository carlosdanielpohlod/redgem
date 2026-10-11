#include <doctest/doctest.h>

#include "net/frame_buffer.hpp"

using namespace redgem::net;

namespace {

constexpr std::size_t kLengthBytes = 4;
constexpr std::uint8_t kHugeLength[] = {0x7F, 0xFF, 0xFF, 0xFF};  // far more than a message may be

}  // namespace

TEST_CASE("a message is framed by its length and comes back whole") {
  const std::vector<std::uint8_t> bytes = frame("hello");
  REQUIRE(bytes.size() == kLengthBytes + 5);
  CHECK(bytes[kLengthBytes - 1] == 5);
  FrameBuffer buffer;
  buffer.append(bytes.data(), bytes.size());
  CHECK(buffer.next() == std::string("hello"));
  CHECK_FALSE(buffer.next().has_value());
}

TEST_CASE("messages arriving in pieces, or several at once, are cut apart correctly") {
  std::vector<std::uint8_t> stream = frame("first");
  const std::vector<std::uint8_t> second = frame("");
  const std::vector<std::uint8_t> third = frame("the third one");
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
  const std::vector<std::uint8_t> fine = frame("fine");
  buffer.append(fine.data(), fine.size());
  CHECK_FALSE(buffer.next().has_value());
}

TEST_CASE("an empty message and the longest one allowed both come back whole") {
  FrameBuffer buffer;
  const std::vector<std::uint8_t> empty = frame("");
  CHECK(empty.size() == kLengthBytes);
  buffer.append(empty.data(), empty.size());
  CHECK(buffer.next() == std::string());
  const std::string longest(kMaxFrameBytes, 'x');
  const std::vector<std::uint8_t> bytes = frame(longest);
  buffer.append(bytes.data(), bytes.size());
  CHECK(buffer.next() == longest);
  CHECK_FALSE(buffer.broken());
}

TEST_CASE("one byte more than allowed breaks the buffer before any of the message is kept") {
  FrameBuffer buffer;
  const std::vector<std::uint8_t> bytes = frame(std::string(kMaxFrameBytes + 1, 'x'));
  buffer.append(bytes.data(), kLengthBytes);
  CHECK_FALSE(buffer.next().has_value());
  CHECK(buffer.broken());
}

TEST_CASE("a length still arriving is waited for") {
  FrameBuffer buffer;
  const std::vector<std::uint8_t> bytes = frame("late");
  buffer.append(bytes.data(), kLengthBytes - 1);
  CHECK_FALSE(buffer.next().has_value());
  CHECK_FALSE(buffer.broken());
  buffer.append(bytes.data() + kLengthBytes - 1, bytes.size() - (kLengthBytes - 1));
  CHECK(buffer.next() == std::string("late"));
}
