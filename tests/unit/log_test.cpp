#include <doctest/doctest.h>

#include <string>
#include <thread>
#include <vector>

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

TEST_CASE("messages written from several threads at once all arrive, whole") {
  constexpr int kThreads = 4;
  constexpr int kMessagesEach = 500;
  test::LogCapture capture;
  std::vector<std::thread> writers;
  for (int writer = 0; writer < kThreads; ++writer) {
    writers.emplace_back([] {
      for (int message = 0; message < kMessagesEach; ++message) log::info("from a room");
    });
  }
  for (std::thread& writer : writers) writer.join();
  CHECK(capture.count(log::Level::Info) == kThreads * kMessagesEach);
}

TEST_CASE("level names") {
  CHECK(std::string(log::level_name(log::Level::Debug)) == "debug");
  CHECK(std::string(log::level_name(log::Level::Error)) == "error");
}

TEST_CASE("thread log captures are nested and do not redirect other threads") {
  test::LogCapture process;
  std::vector<std::string> outer;
  std::vector<std::string> inner;
  {
    const log::Capture first([&](log::Level, const std::string& text) { outer.push_back(text); });
    log::warning("outer first");
    {
      const log::Capture second([&](log::Level, const std::string& text) { inner.push_back(text); });
      log::error("inner");
    }
    std::thread other([] { log::info("other thread"); });
    other.join();
    log::info("outer last");
  }
  log::info("after capture");
  CHECK(outer == std::vector<std::string>{"outer first", "outer last"});
  CHECK(inner == std::vector<std::string>{"inner"});
  CHECK_FALSE(process.contains("outer"));
  CHECK(process.contains("other thread"));
  CHECK(process.contains("after capture"));
}
