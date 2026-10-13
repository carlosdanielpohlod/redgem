#include "physics/jolt/jolt_runtime.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

#include "core/log.hpp"

namespace redgem::jolt {
namespace {

constexpr std::size_t kTraceBufferSize = 1024;

void trace(const char* format, ...) {
  char buffer[kTraceBufferSize];
  va_list args;
  va_start(args, format);
  std::vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log::debug(std::string("jolt: ") + buffer);
}

std::mutex& mutex() {
  static std::mutex instance;
  return instance;
}

// How many worlds are using the registries.
int& count() {
  static int instance = 0;
  return instance;
}

}  // namespace

Runtime::Runtime() {
  std::lock_guard<std::mutex> lock(mutex());
  if (count()++ != 0) return;
  JPH::RegisterDefaultAllocator();
  JPH::Trace = trace;
  JPH::Factory::sInstance = new JPH::Factory();
  JPH::RegisterTypes();
}

Runtime::~Runtime() {
  std::lock_guard<std::mutex> lock(mutex());
  if (--count() != 0) return;
  JPH::UnregisterTypes();
  delete JPH::Factory::sInstance;
  JPH::Factory::sInstance = nullptr;
}

}  // namespace redgem::jolt
