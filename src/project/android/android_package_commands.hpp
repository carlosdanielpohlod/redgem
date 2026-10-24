#pragma once

#include <filesystem>
#include <string>

#include "project/android/android_package.hpp"

// The steps the packer of an APK and the packer of an App Bundle share (android_package_tools.cpp).
namespace redgem::android_commands {

// Where the tools' output goes, in the stage folder: what a failed one said is shown from it.
inline constexpr const char* kToolLog = "tool-output.log";

// Runs a tool, keeping what it says in `log`. When it fails, `error` is what it was doing and the
// last lines it said (not the command itself, which may carry a password).
bool run(const std::string& command, const std::filesystem::path& log, const std::string& what, std::string& error);

// Makes the engine's own signing key the first time it is used (a developer's is theirs to make).
bool ensure_key(const SigningKey& key, const std::filesystem::path& log, std::string& error);

// Whether the stage has resources of its own (the launcher icon), which are compiled first.
bool has_resources(const std::filesystem::path& stage_dir);
std::string compile_resources_command(const AndroidTools& tools, const std::filesystem::path& stage_dir);
// Links the manifest, the assets and the compiled resources into `output`: an APK, or with
// `proto` the protocol-buffer form an App Bundle's module is made of.
std::string link_command(const AndroidTools& tools, const std::filesystem::path& stage_dir,
                         const std::filesystem::path& output, bool proto);

}  // namespace redgem::android_commands
