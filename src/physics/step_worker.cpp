#include "physics/step_worker.hpp"

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
}

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
    work();
    lock.lock();
    busy_ = false;
    changed_.notify_all();
  }
}

}  // namespace redgem
