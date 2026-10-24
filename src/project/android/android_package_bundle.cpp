// Packing an Android App Bundle (.aab), what Google Play takes: the app's files in the form a
// bundle holds them, made into one with bundletool and signed with jarsigner.

#include <sstream>

#include "core/platform.hpp"
#include "core/translations.hpp"
#include "project/android/android_package.hpp"
#include "project/android/android_package_commands.hpp"
#include "project/android/android_package_names.hpp"

namespace redgem {
namespace {

using platform::posix_shell_word;

constexpr const char* kProtoApk = "proto.apk";             // the app's files as aapt2 links them for a bundle
constexpr const char* kModuleFolder = "module";            // the bundle's one module, laid out
constexpr const char* kModuleManifestFolder = "manifest";  // where a module keeps its manifest
constexpr const char* kModuleZip = "base.zip";
constexpr const char* kUnsignedBundle = "unsigned.aab";
constexpr const char* kJarOption = " -jar ";
constexpr const char* kJavaTool = "java";
constexpr const char* kSignatureAlgorithm = "SHA256withRSA";
constexpr const char* kDigestAlgorithm = "SHA-256";
constexpr char kPathSeparator = ':';  // between the folders of the PATH
constexpr bool kBundleLink = true;    // (the protocol-buffer form a bundle's module holds)

// Runs `command` in the stage folder.
std::string in_stage(const std::filesystem::path& stage_dir, const std::string& command) {
  return "cd " + posix_shell_word(stage_dir) + " && " + command;
}

// The module, laid out as a bundle wants it: the linked files, the manifest in a folder of its
// own, and the engine's libraries.
std::string layout_command() {
  const std::string module = posix_shell_word(kModuleFolder);
  const std::string manifest = posix_shell_word(std::filesystem::path(kModuleFolder) / kModuleManifestFolder);
  return "rm -rf " + module + " && " + android_names::kUnzipTool + " -q " + posix_shell_word(kProtoApk) + " -d " +
         module + " && mkdir -p " + manifest + " && mv " +
         posix_shell_word(std::filesystem::path(kModuleFolder) / android_names::kManifestFile) + " " + manifest +
         " && cp -R " + posix_shell_word(android_names::kLibrariesFolder) + " " + module;
}

std::string zip_command() {
  return "cd " + posix_shell_word(kModuleFolder) + " && rm -f " + posix_shell_word("../" + std::string(kModuleZip)) +
         " && " + android_names::kZipTool + " -qr " + posix_shell_word("../" + std::string(kModuleZip)) + " .";
}

std::string build_bundle_command(const std::string& bundletool, const std::filesystem::path& stage_dir) {
  return bundletool + " build-bundle --overwrite --modules=" + posix_shell_word(stage_dir / kModuleZip) +
         " --output=" + posix_shell_word(stage_dir / kUnsignedBundle);
}

// Passwords never reach the command line: jarsigner reads them from the environment variables.
std::string sign_bundle_command(const std::filesystem::path& stage_dir, const SigningKey& key,
                                const std::filesystem::path& aab) {
  return std::string(android_names::kJarsignerTool) + " -keystore " + posix_shell_word(key.keystore) +
         " -storepass:env " + posix_shell_word(key.store_password_variable) + " -keypass:env " +
         posix_shell_word(key.key_password_variable) + " -sigalg " + kSignatureAlgorithm + " -digestalg " +
         kDigestAlgorithm + " -signedjar " + posix_shell_word(aab) + " " +
         posix_shell_word(stage_dir / kUnsignedBundle) + " " + posix_shell_word(key.alias);
}

constexpr auto kRunnable =
    std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec;

bool runnable(const std::filesystem::path& program) {
  std::error_code fs_error;
  const bool file = std::filesystem::is_regular_file(program, fs_error);
  return file && (std::filesystem::status(program, fs_error).permissions() & kRunnable) != std::filesystem::perms::none;
}

// Whether a folder of the PATH holds the program, which may run (an empty folder names none).
bool lies_in(const std::string& folder, const std::filesystem::path& program) {
  return !folder.empty() && runnable(program);
}

std::optional<std::filesystem::path> program_on_path(const std::string& name, const std::string& path_variable) {
  std::istringstream folders(path_variable);
  for (std::string folder; std::getline(folders, folder, kPathSeparator);) {
    std::filesystem::path program = std::filesystem::path(folder) / name;
    if (lies_in(folder, program)) return program;
  }
  return std::nullopt;
}

// The .jar the environment names, run with Java; a name that is no file is said, not passed over.
std::optional<std::string> bundletool_jar(const std::string& jar, std::string& error) {
  std::error_code fs_error;
  if (std::filesystem::is_regular_file(jar, fs_error))
    return std::string(kJavaTool) + kJarOption + posix_shell_word(jar);
  error = tr("project.android.bundletool_jar_missing", {{"variable", kBundletoolVariable}, {"jar", jar}});
  return std::nullopt;
}

}  // namespace

std::optional<std::string> find_bundletool(const std::string& jar_variable_value, const std::string& path_variable,
                                           std::string& error) {
  if (!jar_variable_value.empty()) return bundletool_jar(jar_variable_value, error);
  if (const auto program = program_on_path(android_names::kBundletoolTool, path_variable)) {
    return posix_shell_word(*program);
  }
  error = tr("project.android.no_bundletool", {{"variable", kBundletoolVariable}});
  return std::nullopt;
}

std::vector<PackingStep> bundle_commands(const AndroidTools& tools, const std::string& bundletool,
                                         const std::filesystem::path& stage_dir, const SigningKey& key,
                                         const std::filesystem::path& aab) {
  std::vector<PackingStep> steps;
  if (android_commands::has_resources(stage_dir)) {
    steps.push_back({android_commands::compile_resources_command(tools, stage_dir), "compiling the app's icon"});
  }
  steps.push_back({android_commands::link_command(tools, stage_dir, stage_dir / kProtoApk, kBundleLink),
                   "packing the game's files"});
  steps.push_back(
      {in_stage(stage_dir, layout_command()), "laying out the bundle's module (are zip and unzip installed?)"});
  steps.push_back({in_stage(stage_dir, zip_command()), "zipping the bundle's module"});
  steps.push_back({build_bundle_command(bundletool, stage_dir), "making the App Bundle"});
  steps.push_back({sign_bundle_command(stage_dir, key, aab), "signing the App Bundle"});
  return steps;
}

bool pack_android_bundle(const AndroidTools& tools, const std::string& bundletool,
                         const std::filesystem::path& stage_dir, const SigningKey& key,
                         const std::filesystem::path& aab, std::string& error) {
  const std::filesystem::path log = stage_dir / android_commands::kToolLog;
  if (!android_commands::ensure_key(key, log, error)) return false;
  std::error_code fs_error;
  std::filesystem::create_directories(aab.parent_path(), fs_error);
  for (const PackingStep& step : bundle_commands(tools, bundletool, stage_dir, key, aab)) {
    if (!android_commands::run(step.command, log, step.what, error)) return false;
  }
  return true;
}

}  // namespace redgem
