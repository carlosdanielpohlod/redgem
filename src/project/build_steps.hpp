#pragma once

#include <functional>
#include <string>

// What a build says it is doing while it works ("copying the game's files", "signing the APK"),
// for whoever shows how it goes: the editor, which builds on a thread of its own. A build tells
// only whoever listens on its own thread, so builds on other threads are not heard.
namespace redgem::build_steps {

// The steps every build has, in the words its errors use.
inline constexpr const char* kCopyingPlayer = "copying the player";
inline constexpr const char* kCopyingFiles = "copying the game's files";
inline constexpr const char* kShippingFiles = "writing the game's icon";
inline constexpr const char* kSigningApp = "signing the app";

// Hears each step a build begins.
using Listener = std::function<void(const std::string& step)>;

// Passes the steps of the builds on this thread to `listener` until it is destroyed; one made
// inside another gives them back to the outer one when it goes.
class Listen {
 public:
  explicit Listen(Listener listener);
  ~Listen();
  Listen(const Listen&) = delete;
  Listen& operator=(const Listen&) = delete;

 private:
  Listener previous_;
};

// The build on this thread begins `step`: whoever listens on it hears so (nobody may).
void begin(const std::string& step);

}  // namespace redgem::build_steps
