#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "input/input_source.hpp"
#include "project/tool_line.hpp"

namespace redgem::tools {

inline constexpr const char* kSeedOption = "--seed";
inline constexpr const char* kInputOption = "--input";
inline constexpr const char* kAssertOption = "--assert";
inline constexpr const char* kUntilOption = "--until";
inline constexpr const char* kPrintOption = "--print";
inline constexpr const char* kEveryOption = "--every";

// At a zero-based frame, replace the held keys. Between events the keys stay held.
struct RunInput {
  long long frame = 0;
  std::vector<Key> keys;
};

inline constexpr long long kDefaultPrintEvery = 60;

struct RunPlan {
  std::uint32_t seed = 0;
  std::vector<RunInput> input;
  std::optional<std::string> assertion{};
  std::optional<std::string> until{};
  std::optional<std::string> print{};
  long long every = kDefaultPrintEvery;
};

// Fully validates a plan before a game is created or any script is run.
std::variant<RunPlan, std::string> read_run_plan(const ToolLine& line, long long frames);

struct RunOutcome {
  long long frames = 0;
  bool failed = false;
  std::string output;
  std::string problem;
};

}  // namespace redgem::tools
