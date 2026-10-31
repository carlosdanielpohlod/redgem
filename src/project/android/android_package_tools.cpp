// Packing an Android app: the SDK's tools, run one after the other over a staged folder.

#include "project/android/android_package.hpp"

#include <cstdlib>
#include <fstream>
#include <vector>

#include "core/platform.hpp"
#include "project/android/android_package_commands.hpp"
#include "project/android/android_package_names.hpp"
#include "project/build_steps.hpp"

namespace redgem {

namespace {

constexpr const char* kKeyOwner = "CN=RedGem";            // who the engine's own key says it belongs to
constexpr const char* kPasswordFromEnvironment = "env:";  // apksigner: the password is in this variable
constexpr const char* kUnsignedApk = "unsigned.apk";
constexpr const char* kHintStart = " (";  // where a step's hint for when it fails begins ("(is Java installed?)")
constexpr const char* kAlignedApk = "aligned.apk";
constexpr const char* kCompiledResources = "compiled-res.zip";  // the launcher icon, compiled by aapt2
constexpr bool kApkLink = false;                                // (not the protocol-buffer form of a bundle)
constexpr std::size_t kShownToolLines = 12;                     // of a failed tool's output
constexpr int kKeyValidityDays = 10000;
constexpr int kKeyBits = 2048;
constexpr int kZipAlignment = 4;  // bytes: what Android wants uncompressed entries aligned to

// Every path and name reaches the shell as one word, whatever is in it.
using platform::posix_shell_word;

// The last lines a tool wrote to its log, each on a line of its own.
std::string last_lines(const std::filesystem::path& log) {
  std::vector<std::string> lines;
  std::ifstream said(log);
  for (std::string line; std::getline(said, line);) lines.push_back(line);
  const std::size_t first = lines.size() > kShownToolLines ? lines.size() - kShownToolLines : 0;
  std::string shown;
  for (std::size_t i = first; i < lines.size(); ++i) shown += "\n  " + lines[i];
  return shown;
}

// The commands that make an APK. Paths and names reach the shell as single quoted words;
// passwords never do: the tools are told which environment variables hold them.
std::string make_key_command(const SigningKey& key) {
  return std::string(android_names::kKeytoolTool) + " -genkeypair -keystore " + posix_shell_word(key.keystore) +
         " -storepass:env " + posix_shell_word(key.store_password_variable) + " -keypass:env " +
         posix_shell_word(key.key_password_variable) + " -alias " + posix_shell_word(key.alias) +
         " -keyalg RSA -keysize " + std::to_string(kKeyBits) + " -validity " + std::to_string(kKeyValidityDays) +
         " -dname " + posix_shell_word(kKeyOwner);
}

// The engine's libraries go in by their paths inside the APK (lib/<abi>/...), so from the stage folder.
std::string add_libraries_command(const AndroidTools& tools, const std::filesystem::path& stage_dir) {
  std::string add = "cd " + posix_shell_word(stage_dir) + " && " + posix_shell_word(tools.aapt) + " add " +
                    posix_shell_word(kUnsignedApk);
  std::error_code fs_error;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(stage_dir / android_names::kLibrariesFolder, fs_error)) {
    if (entry.is_regular_file(fs_error)) add += " " + posix_shell_word(entry.path().lexically_relative(stage_dir));
  }
  return add;
}

std::string align_command(const AndroidTools& tools, const std::filesystem::path& stage_dir) {
  return posix_shell_word(tools.zipalign) + " -f " + std::to_string(kZipAlignment) + " " +
         posix_shell_word(stage_dir / kUnsignedApk) + " " + posix_shell_word(stage_dir / kAlignedApk);
}

std::string sign_command(const AndroidTools& tools, const std::filesystem::path& stage_dir, const SigningKey& key,
                         const std::filesystem::path& apk) {
  return posix_shell_word(tools.apksigner) + " sign --ks " + posix_shell_word(key.keystore) + " --ks-key-alias " +
         posix_shell_word(key.alias) + " --ks-pass " +
         posix_shell_word(kPasswordFromEnvironment + key.store_password_variable) + " --key-pass " +
         posix_shell_word(kPasswordFromEnvironment + key.key_password_variable) + " --v4-signing-enabled false --out " +
         posix_shell_word(apk) + " " + posix_shell_word(stage_dir / kAlignedApk);
}

// The launcher icon, compiled, when the app has one.
bool compile_resources(const AndroidTools& tools, const std::filesystem::path& stage_dir,
                       const std::filesystem::path& log, std::string& error) {
  if (!android_commands::has_resources(stage_dir)) return true;
  return android_commands::run(android_commands::compile_resources_command(tools, stage_dir), log,
                               "compiling the app's icon", error);
}

}  // namespace

namespace android_commands {

bool run(const std::string& command, const std::filesystem::path& log, const std::string& what, std::string& error) {
  build_steps::begin(what.substr(0, what.find(kHintStart)));
  const std::string logged = command + " >" + posix_shell_word(log) + " 2>&1";
  // The SDK's tools are programs to run: every word of `command` that is not fixed is quoted.
  if (std::system(logged.c_str()) == 0) return true;  // NOLINT(bugprone-command-processor)
  error = what + " failed" + last_lines(log);
  return false;
}

// Only the engine's own key is ever made here; a developer's is theirs to make and keep.
bool ensure_key(const SigningKey& key, const std::filesystem::path& log, std::string& error) {
  std::error_code fs_error;
  if (std::filesystem::exists(key.keystore, fs_error)) return true;
  std::filesystem::create_directories(key.keystore.parent_path(), fs_error);
  if (!run(make_key_command(key), log, "making the signing key (is Java installed?)", error)) return false;
  keep_to_owner(key.keystore);
  return true;
}

bool has_resources(const std::filesystem::path& stage_dir) {
  std::error_code fs_error;
  return std::filesystem::is_directory(stage_dir / android_names::kResourcesFolder, fs_error);
}

std::string compile_resources_command(const AndroidTools& tools, const std::filesystem::path& stage_dir) {
  return posix_shell_word(tools.aapt2) + " compile --dir " +
         posix_shell_word(stage_dir / android_names::kResourcesFolder) + " -o " +
         posix_shell_word(stage_dir / kCompiledResources);
}

std::string link_command(const AndroidTools& tools, const std::filesystem::path& stage_dir,
                         const std::filesystem::path& output, bool proto) {
  std::string link = posix_shell_word(tools.aapt2) + " link -o " + posix_shell_word(output) + " --manifest " +
                     posix_shell_word(stage_dir / android_names::kManifestFile) + " -I " +
                     posix_shell_word(tools.platform) + " -A " +
                     posix_shell_word(stage_dir / android_names::kAssetsFolder);
  if (proto) link += " --proto-format";
  if (has_resources(stage_dir)) link += " " + posix_shell_word(stage_dir / kCompiledResources);
  return link;
}

}  // namespace android_commands

bool pack_android_apk(const AndroidTools& tools, const std::filesystem::path& stage_dir, const SigningKey& key,
                      const std::filesystem::path& apk, std::string& error) {
  using android_commands::run;
  const std::filesystem::path log = stage_dir / android_commands::kToolLog;
  if (!android_commands::ensure_key(key, log, error) || !compile_resources(tools, stage_dir, log, error)) return false;
  const std::string link = android_commands::link_command(tools, stage_dir, stage_dir / kUnsignedApk, kApkLink);
  if (!run(link, log, "packing the game's files", error)) return false;
  if (!run(add_libraries_command(tools, stage_dir), log, "adding the engine", error)) return false;
  if (!run(align_command(tools, stage_dir), log, "aligning the APK", error)) return false;
  std::error_code fs_error;
  std::filesystem::create_directories(apk.parent_path(), fs_error);
  return run(sign_command(tools, stage_dir, key, apk), log, "signing the APK", error);
}

}  // namespace redgem
