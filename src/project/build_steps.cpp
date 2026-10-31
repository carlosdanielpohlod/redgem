#include "project/build_steps.hpp"

#include <utility>

namespace redgem::build_steps {
namespace {

thread_local Listener listening;

}  // namespace

Listen::Listen(Listener listener) : previous_(std::exchange(listening, std::move(listener))) {}
Listen::~Listen() { listening = std::move(previous_); }

void begin(const std::string& step) {
  if (listening) listening(step);
}

}  // namespace redgem::build_steps
