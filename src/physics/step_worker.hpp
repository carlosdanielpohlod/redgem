#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace redgem {

// A thread of its own that runs one piece of work at a time, handed to it by another thread, which
// goes on with its own meanwhile and waits for the work where it needs what the work changed. All
// the work changed is seen by whoever waited for it (wait() returning is the sync point).
class StepWorker {
 public:
  StepWorker();
  // Waits for the work under way, then ends the thread.
  ~StepWorker();
  StepWorker(const StepWorker&) = delete;
  StepWorker& operator=(const StepWorker&) = delete;

  // Hands the thread `work` and returns at once (after the work before it, if that was still under way).
  void start(std::function<void()> work);
  // Returns once no work is under way.
  void wait() const;
  // Whether work is under way now.
  bool busy() const;

 private:
  void run();
  // Whether the thread has something to do: work to run, or to end.
  bool woken() const { return busy_ || stopping_; }

  mutable std::mutex mutex_;
  mutable std::condition_variable changed_;
  std::function<void()> work_;
  bool busy_ = false;
  bool stopping_ = false;
  std::thread thread_;  // last: it starts once the rest is there
};

}  // namespace redgem
