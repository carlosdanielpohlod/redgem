#include <doctest/doctest.h>

#include <string>

#include "render/card_memory.hpp"

using namespace redgem;

namespace {
constexpr int kTotal = 6141;
constexpr int kFree = 1931;
}  // namespace

TEST_CASE("the card's memory is told as used of total when the driver tells both") {
  CHECK(card_memory_text({kTotal, kFree}) == "graphics memory: 4210 of 6141 MB used");
}

TEST_CASE("the card's memory is told as free when the driver tells only that") {
  CHECK(card_memory_text({std::nullopt, kFree}) == "graphics memory: 1931 MB free");
}

TEST_CASE("nothing is told of the card's memory when the driver tells nothing, but a message still says so") {
  CHECK(card_memory_text({}).empty());
  CHECK(card_memory_said({}) == "the driver does not tell how much graphics memory is used");
  CHECK(card_memory_said({kTotal, kFree}) == card_memory_text({kTotal, kFree}));
}
