#include <doctest/doctest.h>

#include <atomic>
#include <future>
#include <ostream>  // (doctest prints a std::thread::id through it)
#include <thread>
#include <vector>

#include "physics/step_worker.hpp"

using namespace redgem;

namespace {

constexpr int kManyPieces = 200;

}  // namespace

TEST_CASE("a step worker runs its work on a thread of its own, and waiting for it shows what it did") {
  StepWorker worker;
  std::thread::id ran_on;
  int result = 0;  // (no atomic: wait() is what makes the work's writes seen)
  worker.start([&] {
    ran_on = std::this_thread::get_id();
    result = 1;
  });
  worker.wait();
  CHECK_FALSE(worker.busy());
  CHECK(result == 1);
  CHECK(ran_on != std::this_thread::get_id());
}

TEST_CASE("a step worker goes on with the caller's thread while its work is under way") {
  StepWorker worker;
  std::promise<void> released;
  std::shared_future<void> release = released.get_future().share();
  worker.start([release] { release.wait(); });
  CHECK(worker.busy());  // (held until the caller lets it go)
  released.set_value();
  worker.wait();
  CHECK_FALSE(worker.busy());
}

TEST_CASE("work handed to a busy step worker runs after the work before it, in the order handed") {
  StepWorker worker;
  std::vector<int> order;  // touched by the worker alone until the last wait
  for (int i = 0; i < kManyPieces; ++i) worker.start([&order, i] { order.push_back(i); });
  worker.wait();
  REQUIRE(order.size() == static_cast<std::size_t>(kManyPieces));
  for (int i = 0; i < kManyPieces; ++i) CHECK(order[static_cast<std::size_t>(i)] == i);
}

TEST_CASE("a step worker ends only once its work has") {
  std::atomic<bool> done{false};
  {
    StepWorker worker;
    worker.start([&done] {
      std::this_thread::yield();
      done = true;
    });
  }
  CHECK(done);
}

TEST_CASE("waiting for a step worker with nothing to do returns at once") {
  const StepWorker worker;
  worker.wait();
  CHECK_FALSE(worker.busy());
}
