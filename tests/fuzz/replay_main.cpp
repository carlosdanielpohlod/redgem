// Runs a fuzz target on every file of the corpora it is given (folders, searched through, or
// single files), without libFuzzer: how the inputs that once crashed it stay tested in every
// build, with any compiler. Fails when one is missing or empty.
//   redgem_fuzz_gltf_replay tests/fuzz/corpus/gltf
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#include "fuzz/fuzz_target.hpp"

namespace fs = std::filesystem;

namespace {

constexpr int kExitSuccess = 0;
constexpr int kExitFailure = 1;

std::vector<std::uint8_t> read_file(const fs::path& file) {
  std::ifstream in(file, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// Runs the target on `file`; the count of inputs run so far goes up by one.
void replay(const fs::path& file, int& count) {
  const std::vector<std::uint8_t> input = read_file(file);
  LLVMFuzzerTestOneInput(input.data(), input.size());
  ++count;
}

// Every file under `corpus` (or `corpus` itself, when it is a file).
int replay_corpus(const fs::path& corpus) {
  int count = 0;
  if (fs::is_regular_file(corpus)) replay(corpus, count);
  if (!fs::is_directory(corpus)) return count;
  for (const fs::directory_entry& entry : fs::recursive_directory_iterator(corpus)) {
    if (entry.is_regular_file()) replay(entry.path(), count);
  }
  return count;
}

}  // namespace

int main(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    const int count = replay_corpus(argv[i]);
    std::cout << argv[i] << ": " << count << " inputs\n";
    if (count == 0) return kExitFailure;
  }
  return kExitSuccess;
}
