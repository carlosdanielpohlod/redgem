// Where the engine's messages are shown: the terminal, or a phone's own log.

#include "core/platform.hpp"

#include <iostream>

#include "core/safe_path.hpp"

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace redgem::platform {
namespace {

#ifdef __ANDROID__
constexpr const char* kLogTag = "redgem";  // what the system's log files the engine's messages under
#else
constexpr const char* kLogPrefix = "[redgem] ";
#endif
constexpr char kLineBreak = '\n';
// What the lines of a message after its first start with: only a message starts with the mark.
constexpr const char* kContinuation = "\n  ";

}  // namespace

std::string log_text(const std::string& message) {
  std::string text;
  for (const char c : printable_lines(message)) {
    if (c == kLineBreak) {
      text += kContinuation;
    } else {
      text.push_back(c);
    }
  }
  return text;
}

#ifdef __ANDROID__
// An Android app has no terminal: its messages go to the system's log (adb logcat).
void write_log(log::Level level, const std::string& message) {
  const int priority = level == log::Level::Error     ? ANDROID_LOG_ERROR
                       : level == log::Level::Warning ? ANDROID_LOG_WARN
                       : level == log::Level::Info    ? ANDROID_LOG_INFO
                                                      : ANDROID_LOG_DEBUG;
  __android_log_write(priority, kLogTag, log_text(message).c_str());
}
#else
namespace {

// Errors and warnings go where a terminal shows what went wrong.
bool goes_to_error_stream(log::Level level) { return level == log::Level::Error || level == log::Level::Warning; }

}  // namespace

void write_log(log::Level level, const std::string& message) {
  std::ostream& out = goes_to_error_stream(level) ? std::cerr : std::cout;
  out << kLogPrefix << log::level_name(level) << ": " << log_text(message) << kLineBreak << std::flush;
}
#endif

}  // namespace redgem::platform
