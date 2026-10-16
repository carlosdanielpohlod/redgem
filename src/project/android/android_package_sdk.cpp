// Packing an Android app: finding the SDK's tools on this machine.

#include "project/android/android_package.hpp"

#include <algorithm>
#include <cctype>
#include <vector>

#include "core/platform.hpp"
#include "project/android/android_package_names.hpp"

namespace redgem {

namespace {

constexpr const char* kSdkVariables[] = {"ANDROID_HOME", "ANDROID_SDK_ROOT"};
constexpr const char* kBuildToolsFolder = "build-tools";
constexpr const char* kPlatformsFolder = "platforms";
constexpr const char* kPlatformJar = "android.jar";

// The numbers in a name, in order ("35.0.0" -> 35, 0, 0; "android-9" -> 9): what versions compare by.
std::vector<int> version_of(const std::string& name) {
  std::vector<int> numbers;
  std::string digits;
  for (const char c : name + " ") {
    if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
      digits += c;
    } else if (!digits.empty()) {
      numbers.push_back(std::stoi(digits));
      digits.clear();
    }
  }
  return numbers;
}

// The folder in `folder` with the highest version in its name.
std::filesystem::path newest_in(const std::filesystem::path& folder) {
  std::vector<std::filesystem::path> entries;
  std::error_code fs_error;
  for (const auto& entry : std::filesystem::directory_iterator(folder, fs_error)) {
    if (entry.is_directory(fs_error)) entries.push_back(entry.path());
  }
  const auto older = [](const std::filesystem::path& a, const std::filesystem::path& b) {
    return version_of(a.filename().string()) < version_of(b.filename().string());
  };
  return entries.empty() ? std::filesystem::path() : *std::max_element(entries.begin(), entries.end(), older);
}

AndroidTools tools_in(const std::filesystem::path& sdk_root) {
  const std::filesystem::path build_tools = newest_in(sdk_root / kBuildToolsFolder);
  AndroidTools tools;
  tools.aapt = build_tools / android_names::kAaptTool;
  tools.aapt2 = build_tools / android_names::kAapt2Tool;
  tools.zipalign = build_tools / android_names::kZipalignTool;
  tools.apksigner = build_tools / android_names::kApksignerTool;
  tools.platform = newest_in(sdk_root / kPlatformsFolder) / kPlatformJar;
  return tools;
}

// The first of the tools that is not there; nullptr when all are.
const std::filesystem::path* missing_tool(const AndroidTools& tools) {
  std::error_code fs_error;
  for (const std::filesystem::path* needed :
       {&tools.aapt, &tools.aapt2, &tools.zipalign, &tools.apksigner, &tools.platform}) {
    if (!std::filesystem::exists(*needed, fs_error)) return needed;
  }
  return nullptr;
}

}  // namespace

std::filesystem::path android_sdk_from_environment() {
  for (const char* variable : kSdkVariables) {
    std::string value = platform::environment(variable);
    if (!value.empty()) return value;
  }
  return {};
}

std::optional<AndroidTools> find_android_tools(const std::filesystem::path& sdk, std::string& error) {
  if (sdk.empty()) {
    error = "the Android SDK was not found: set ANDROID_HOME to where it is installed";
    return std::nullopt;
  }
  std::error_code fs_error;
  // (Tools are run from other folders.)
  AndroidTools tools = tools_in(std::filesystem::absolute(sdk, fs_error));
  const std::filesystem::path* missing = missing_tool(tools);
  if (missing == nullptr) return tools;
  error = "the Android SDK in " + sdk.string() + " is missing " + missing->filename().string() +
          ": install its build-tools and a platform (sdkmanager \"build-tools;35.0.0\" \"platforms;android-35\")";
  return std::nullopt;
}

}  // namespace redgem
