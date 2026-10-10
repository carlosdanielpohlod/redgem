#include <doctest/doctest.h>

#include "core/log.hpp"
#include "test_support.hpp"

using namespace redgem;

TEST_CASE("log messages reach the installed sink with their level") {
  test::LogCapture capture;
  log::info("hello");
  log::warning("careful");
  log::error("broken");
  CHECK(capture.contains("hello"));
  CHECK(capture.count(log::Level::Warning) == 1);
  CHECK(capture.count(log::Level::Error) == 1);
}

TEST_CASE("level names") {
  CHECK(std::string(log::level_name(log::Level::Debug)) == "debug");
  CHECK(std::string(log::level_name(log::Level::Error)) == "error");
}
