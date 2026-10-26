#pragma once

#include "app/engine.hpp"
#include "project/run_plan.hpp"

namespace redgem::tools {

// Keys change before a frame. Expressions run after it, with frame numbers starting at 1.
RunOutcome play_run(Engine& engine, const RunPlan& plan, long long frames);

}  // namespace redgem::tools
