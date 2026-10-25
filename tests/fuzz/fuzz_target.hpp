#pragma once

// What every fuzz target defines: libFuzzer calls it with each input it makes up, and the replay
// program (replay_main.cpp) with each file of a corpus. An input the code refuses is fine; one
// that crashes it, or that a sanitizer catches it mishandling, is a finding.
#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

namespace redgem::fuzz {

// What a target returns for every input: libFuzzer keeps any input it is given in the corpus.
inline constexpr int kKeepInput = 0;

}  // namespace redgem::fuzz
