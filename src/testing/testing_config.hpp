#pragma once

// The numbers and names of the test tool.

namespace redgem::config {

// Where a system test's pictures go unless the command line says, and what kind of file they are.
inline constexpr const char* kDefaultScreenshotDir = "screenshots";
inline constexpr const char* kScreenshotExtension = ".png";

// The real seconds a system test may spend in all in waits for another program (t.wait_for),
// unless the command line says (--wait-limit), and the most it may say: a day.
inline constexpr int kDefaultWaitLimitSeconds = 300;
inline constexpr int kMostWaitLimitSeconds = 86400;

}  // namespace redgem::config
