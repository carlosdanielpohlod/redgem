#include "physics/step_worker.hpp"

#include <exception>
#include <utility>

namespace redgem {

StepWorker::StepWorker() : thread_([this] { run(); }) {}

StepWorker::~StepWorker() {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    changed_.wait(lock, [this] { return !busy_; });
    stopping_ = true;
  }
  changed_.notify_all();
  thread_.join();
}

void StepWorker::start(std::function<void()> work) {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    changed_.wait(lock, [this] { return !busy_; });
    work_ = std::move(work);
    busy_ = true;
  }
  changed_.notify_all();
}

void StepWorker::wait() const {
  std::unique_lock<std::mutex> lock(mutex_);
  changed_.wait(lock, [this] { return !busy_; });
  if (failure_ != nullptr) std::rethrow_exception(std::exchange(failure_, nullptr));
}

namespace {

// Runs `work`, keeping what it throws for whoever waits (on this thread, it would end the program).
std::exception_ptr run_kept(const std::function<void()>& work) {
  try {
    work();
  } catch (...) {
    return std::current_exception();
  }
  return nullptr;
}

}  // namespace

bool StepWorker::busy() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return busy_;
}

void StepWorker::run() {
  std::unique_lock<std::mutex> lock(mutex_);
  for (;;) {
    changed_.wait(lock, [this] { return woken(); });
    if (!busy_) return;  // ended, with nothing left to do
    std::function<void()> work = std::move(work_);
    lock.unlock();
    std::exception_ptr failure = run_kept(work);
    lock.lock();
    failure_ = std::move(failure);
    busy_ = false;
    changed_.notify_all();
  }
}

}  // namespace redgem
