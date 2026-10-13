#pragma once

namespace redgem::jolt {

// Jolt keeps process-wide registries (its allocator, its factory, its types): they are there
// while at least one of these is alive, so every world holds one.
class Runtime {
 public:
  Runtime();
  ~Runtime();
  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;
};

}  // namespace redgem::jolt
