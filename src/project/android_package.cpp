#include "project/android_package.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string_view>
#include <vector>

#include "app/android_bundle.hpp"
#include "core/files.hpp"
#include "app/bundled_game.hpp"
#include "project/game_build.hpp"
#include "app/game_settings.hpp"
#include "project/notices.hpp"
#include "core/config.hpp"
#include "core/log.hpp"
#include "core/platform.hpp"
#include "core/xml_writer.hpp"

namespace redgem {

namespace {

constexpr const char* kSdkVariables[] = {"ANDROID_HOME", "ANDROID_SDK_ROOT"};
constexpr const char* kBuildToolsFolder = "build-tools";
constexpr const char* kPlatformsFolder = "platforms";
constexpr const char* kPlatformJar = "android.jar";
// The SDK's tools, in its newest build-tools folder, and Java's, found by the shell.
constexpr const char* kAaptTool = "aapt";
constexpr const char* kAapt2Tool = "aapt2";
constexpr const char* kZipalignTool = "zipalign";
constexpr const char* kApksignerTool = "apksigner";
constexpr const char* kKeytoolTool = "keytool";
constexpr const char* kKeyOwner = "CN=RedGem";            // who the engine's own key says it belongs to
constexpr const char* kPasswordFromEnvironment = "env:";  // apksigner: the password is in this variable
constexpr const char* kPackagePrefix = "com.redgem.";
constexpr const char* kFallbackPackage = "game";
constexpr const char* kAssetsFolder = "assets";
constexpr const char* kLibrariesFolder = "lib";
constexpr const char* kAnyAbi = "<abi>";  // stands for a kind of processor in a path shown to people
constexpr const char* kManifestFile = "AndroidManifest.xml";
constexpr const char* kUnsignedApk = "unsigned.apk";
constexpr const char* kAlignedApk = "aligned.apk";
constexpr const char* kApkExtension = ".apk";
constexpr const char* kKeystoreFile = "android-debug.keystore";
// The engine's own key lives in the user's RedGem folder, on the machine that made it. It only
// says "the same machine built this again", so its password is no secret; it reaches the tools
// through the environment all the same, like a developer's own.
constexpr const char* kKeyAlias = "redgem";
constexpr const char* kKeyPassword = "redgem-debug";
constexpr const char* kEnginePasswordVariable = "REDGEM_ENGINE_KEY_PASSWORD";
constexpr char kPackageJoiner = '_';
constexpr const char* kHashFormat = "%06x";  // six hex digits
constexpr int kKeyValidityDays = 10000;
constexpr int kZipAlignment = 4;  // bytes: what Android wants uncompressed entries aligned to
// The oldest Android the app runs on: the one the engine is built for. It is written once, in
// tools/android/min_api.txt, which the build of the engine for Android reads as well.
constexpr int kMinAndroidApi = REDGEM_ANDROID_MIN_API;
constexpr int kTargetAndroidApi = 34;
constexpr int kKeyBits = 2048;
// The manifest's elements and attributes, by the names Android gives them.
namespace tag {
constexpr const char* kManifest = "manifest";
constexpr const char* kUsesSdk = "uses-sdk";
constexpr const char* kUsesFeature = "uses-feature";
constexpr const char* kUsesPermission = "uses-permission";
constexpr const char* kApplication = "application";
constexpr const char* kActivity = "activity";
constexpr const char* kMetaData = "meta-data";
constexpr const char* kIntentFilter = "intent-filter";
constexpr const char* kAction = "action";
constexpr const char* kCategory = "category";
}  // namespace tag
namespace attr {
constexpr const char* kAndroidNamespace = "xmlns:android";
constexpr const char* kPackage = "package";
constexpr const char* kVersionCode = "android:versionCode";
constexpr const char* kVersionName = "android:versionName";
constexpr const char* kMinSdk = "android:minSdkVersion";
constexpr const char* kTargetSdk = "android:targetSdkVersion";
constexpr const char* kGlEsVersion = "android:glEsVersion";
constexpr const char* kRequired = "android:required";
constexpr const char* kName = "android:name";
constexpr const char* kValue = "android:value";
constexpr const char* kLabel = "android:label";
constexpr const char* kHasCode = "android:hasCode";
constexpr const char* kAllowBackup = "android:allowBackup";
constexpr const char* kExtractNativeLibs = "android:extractNativeLibs";
constexpr const char* kTheme = "android:theme";
constexpr const char* kExported = "android:exported";
constexpr const char* kOrientation = "android:screenOrientation";
constexpr const char* kConfigChanges = "android:configChanges";
}  // namespace attr
// The values that go in them.
constexpr const char* kAndroidNamespace = "http://schemas.android.com/apk/res/android";
constexpr const char* kYes = "true";
constexpr const char* kNo = "false";
constexpr const char* kInternetPermission = "android.permission.INTERNET";
constexpr const char* kFullScreenTheme = "@android:style/Theme.NoTitleBar.Fullscreen";
constexpr const char* kNativeActivity = "android.app.NativeActivity";
constexpr const char* kLibraryNameKey = "android.app.lib_name";
constexpr const char* kLandscape = "sensorLandscape";
constexpr const char* kMainAction = "android.intent.action.MAIN";
constexpr const char* kLauncherCategory = "android.intent.category.LAUNCHER";
// Changes around the game that it rides out instead of being started over.
constexpr const char* kHandledChanges =
    "orientation|keyboardHidden|screenSize|smallestScreenSize|screenLayout|keyboard|navigation|uiMode|density|"
    "fontScale|locale|layoutDirection|touchscreen";
constexpr const char* kOpenGlEs3 = "0x00030000";           // what the manifest calls OpenGL ES 3.0
constexpr const char* kStageFolder = ".redgem-apk-stage";  // inside the output folder, gone when the build ends
constexpr const char* kToolLog = "tool-output.log";
constexpr std::size_t kShownToolLines = 12;       // of a failed tool's output
constexpr const char* kLineBreaks = "\n\r";       // what the list of an app's files is split at
constexpr char kSkippedFolderPrefix = '_';        // the SDK's packer leaves such folders out
constexpr std::uint32_t kHashSeed = 2166136261u;  // FNV-1a
constexpr std::uint32_t kHashPrime = 16777619u;
constexpr std::uint32_t kHashShown = 0xFFFFFF;  // six hex digits of it

std::string quoted(const std::filesystem::path& path) {
  std::string out = "'";
  for (const char c : path.string()) out += c == '\'' ? std::string("'\\''") : std::string(1, c);
  return out + "'";
}

// A text as one word of a command, whatever is in it.
std::string shell_word(const std::string& text) { return quoted(std::filesystem::path(text)); }

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

// Runs a tool, keeping what it says in `log`. When it fails, `error` is what it was doing and the
// last lines it said (not the command itself, which may carry a password).
bool run(const std::string& command, const std::filesystem::path& log, const std::string& what, std::string& error) {
  const std::string logged = command + " >" + quoted(log) + " 2>&1";
  // The SDK's tools are programs to run: every word of `command` that is not fixed went through quoted().
  if (std::system(logged.c_str()) == 0) return true;  // NOLINT(bugprone-command-processor)
  error = what + " failed" + last_lines(log);
  return false;
}

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

// Whether a file sits in a folder the SDK's packer silently leaves out of an APK.
bool in_skipped_folder(const std::filesystem::path& relative) {
  const std::filesystem::path folders = relative.parent_path();
  return std::any_of(folders.begin(), folders.end(), [](const std::filesystem::path& part) {
    return !part.empty() && part.string().front() == kSkippedFolderPrefix;
  });
}

bool write_text(const std::filesystem::path& file, const std::string& text) {
  std::ofstream out(file, std::ios::binary);
  out << text;
  return static_cast<bool>(out);
}

}  // namespace

std::filesystem::path android_sdk_from_environment() {
  for (const char* variable : kSdkVariables) {
    const char* value = std::getenv(variable);
    if (value != nullptr && *value != '\0') return value;
  }
  return {};
}

namespace {

AndroidTools tools_in(const std::filesystem::path& sdk_root) {
  const std::filesystem::path build_tools = newest_in(sdk_root / kBuildToolsFolder);
  AndroidTools tools;
  tools.aapt = build_tools / kAaptTool;
  tools.aapt2 = build_tools / kAapt2Tool;
  tools.zipalign = build_tools / kZipalignTool;
  tools.apksigner = build_tools / kApksignerTool;
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

bool small_letter_or_digit(unsigned char letter) { return std::islower(letter) != 0 || std::isdigit(letter) != 0; }

// A folder's name with only its letters and digits, in small letters.
std::string letters_and_digits(const std::string& folder) {
  std::string name;
  for (const char c : folder) {
    const auto letter = static_cast<unsigned char>(c);
    if (std::isalnum(letter) != 0) name.push_back(static_cast<char>(std::tolower(letter)));
  }
  return name;
}

// A few digits that tell names apart once what an app's id cannot hold was dropped from them.
std::string name_digits(const std::string& folder) {
  std::uint32_t hash = kHashSeed;
  for (const char c : folder) hash = (hash ^ static_cast<unsigned char>(c)) * kHashPrime;
  char digits[sizeof("ffffff")];
  std::snprintf(digits, sizeof(digits), kHashFormat, hash & kHashShown);
  return digits;
}

}  // namespace

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

std::string android_package_name(const std::filesystem::path& game_dir) {
  const std::string folder = normalized_folder(game_dir).filename().string();
  std::string name = letters_and_digits(folder);
  // A part of an app's id starts with a letter.
  if (name.empty() || std::isalpha(static_cast<unsigned char>(name.front())) == 0) name = kFallbackPackage + name;
  // Nothing but small letters and digits: the name is the id as it is.
  if (std::all_of(folder.begin(), folder.end(), small_letter_or_digit)) return kPackagePrefix + name;
  // "My Game", "my-game" and "mygame" are three games: what was dropped from the name comes
  // back as a few digits, so one does not install over another.
  return kPackagePrefix + name + kPackageJoiner + name_digits(folder);
}

namespace {

bool in_environment(const char* variable) {
  const char* value = std::getenv(variable);
  return value != nullptr && *value != '\0';
}

// The engine's own key. (The tools read its password from the environment, where this process puts it.)
SigningKey engine_key(const std::filesystem::path& engine_keystore) {
  platform::set_environment(kEnginePasswordVariable, kKeyPassword, true);
  SigningKey key;
  key.keystore = engine_keystore;
  key.alias = kKeyAlias;
  key.store_password_variable = kEnginePasswordVariable;
  key.key_password_variable = kEnginePasswordVariable;
  return key;
}

// Where a developer's keystore is: as the game says, or in the game's folder.
std::filesystem::path keystore_location(const std::filesystem::path& game_dir, const std::filesystem::path& keystore) {
  std::error_code fs_error;
  return std::filesystem::absolute(keystore.is_absolute() ? keystore : game_dir / keystore, fs_error);
}

// Whether a developer's keystore can be signed with: it is there, and its password is in the environment.
bool keystore_usable(const std::filesystem::path& keystore, std::string& error) {
  std::error_code fs_error;
  if (!std::filesystem::is_regular_file(keystore, fs_error)) {
    error = "the keystore " + keystore.string() + " does not exist";
    return false;
  }
  if (in_environment(kKeystorePasswordVariable)) return true;
  error = std::string("the keystore's password is not known: put it in the environment as ") +
          kKeystorePasswordVariable + " (and the key's own in " + kKeyPasswordVariable + ", when it differs)";
  return false;
}

// A developer's own key, as the game's settings name it.
std::optional<SigningKey> developer_key(const std::filesystem::path& game_dir, const AndroidSettings& settings,
                                        std::string& error) {
  SigningKey key;
  key.keystore = keystore_location(game_dir, settings.keystore);
  if (!keystore_usable(key.keystore, error)) return std::nullopt;
  key.alias = settings.key_alias;
  key.store_password_variable = kKeystorePasswordVariable;
  key.key_password_variable = in_environment(kKeyPasswordVariable) ? kKeyPasswordVariable : kKeystorePasswordVariable;
  return key;
}

}  // namespace

bool read_android_settings(const std::filesystem::path& game_dir, AndroidSettings& settings, std::string& error) {
  const GameManifest manifest = read_game_manifest(game_dir);
  settings = manifest.settings.android;
  if (!manifest.android_error.empty()) error = manifest.android_error;
  return manifest.android_error.empty();
}

std::optional<SigningKey> signing_key_for(const std::filesystem::path& game_dir, const AndroidSettings& settings,
                                          const std::filesystem::path& engine_keystore, std::string& error) {
  if (settings.keystore.empty()) return engine_key(engine_keystore);
  return developer_key(game_dir, settings, error);
}

namespace {

// What the app needs from the phone: a recent enough Android, OpenGL ES 3, and the network.
void add_requirements(XmlElement& manifest) {
  manifest.child(tag::kUsesSdk)
      .attribute(attr::kMinSdk, std::to_string(kMinAndroidApi))
      .attribute(attr::kTargetSdk, std::to_string(kTargetAndroidApi));
  manifest.child(tag::kUsesFeature).attribute(attr::kGlEsVersion, kOpenGlEs3).attribute(attr::kRequired, kYes);
  manifest.child(tag::kUsesPermission).attribute(attr::kName, kInternetPermission);  // multiplayer
}

// The one screen of the app: full, sideways, the system's native activity running the engine's
// library, opened from the phone's launcher.
void add_activity(XmlElement& application) {
  XmlElement& activity = application.child(tag::kActivity);
  activity.attribute(attr::kName, kNativeActivity)
      .attribute(attr::kExported, kYes)
      .attribute(attr::kOrientation, kLandscape)
      .attribute(attr::kConfigChanges, kHandledChanges);
  activity.child(tag::kMetaData).attribute(attr::kName, kLibraryNameKey).attribute(attr::kValue, kAndroidLibraryName);
  XmlElement& launcher = activity.child(tag::kIntentFilter);
  launcher.child(tag::kAction).attribute(attr::kName, kMainAction);
  launcher.child(tag::kCategory).attribute(attr::kName, kLauncherCategory);
}

// The app itself. No Java of its own: hasCode is off. Saves stay on the device: they do not leave
// it through the system's backups unless a developer decides so.
XmlElement& add_application(XmlElement& manifest, const std::string& label) {
  XmlElement& application = manifest.child(tag::kApplication);
  application.attribute(attr::kLabel, label)
      .attribute(attr::kHasCode, kNo)
      .attribute(attr::kAllowBackup, kNo)
      .attribute(attr::kExtractNativeLibs, kYes)
      .attribute(attr::kTheme, kFullScreenTheme);
  return application;
}

}  // namespace

std::string android_manifest(const std::string& package, const std::string& label, const AndroidSettings& settings) {
  XmlElement manifest(tag::kManifest);
  manifest.attribute(attr::kAndroidNamespace, kAndroidNamespace)
      .attribute(attr::kPackage, package)
      .attribute(attr::kVersionCode, std::to_string(settings.version_code))
      .attribute(attr::kVersionName, settings.version_name);
  add_requirements(manifest);
  add_activity(add_application(manifest, label));
  return manifest.document();
}

std::map<std::string, std::filesystem::path> android_runtimes(const std::filesystem::path& runtimes_dir) {
  std::map<std::string, std::filesystem::path> runtimes;
  std::error_code fs_error;
  for (const auto& entry : std::filesystem::directory_iterator(runtimes_dir, fs_error)) {
    const std::filesystem::path library = entry.path() / android_library_file();
    if (std::filesystem::is_regular_file(library, fs_error)) runtimes[entry.path().filename().string()] = library;
  }
  return runtimes;
}

namespace {

bool copy_over(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& fs_error) {
  std::filesystem::create_directories(to.parent_path(), fs_error);
  std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, fs_error);
  return !fs_error;
}

// The app finds its files by a list with one path a line: a name with a line break in it would
// read as two other files. False, with `error` set, for such a name.
bool can_be_listed(const std::filesystem::path& relative, std::string& error) {
  if (relative.generic_string().find_first_of(kLineBreaks) == std::string::npos) return true;
  error = "a file in '" + relative.parent_path().generic_string() +
          "' has a line break in its name and cannot go into an Android app. Rename the file.";
  return false;
}

// Copies one of the game's files into the app's assets. False, with `error` set, when it cannot go.
bool stage_file(const std::filesystem::path& file, const std::filesystem::path& relative,
                const std::filesystem::path& assets, std::string& error) {
  if (!can_be_listed(relative, error)) return false;
  if (in_skipped_folder(relative)) {
    error = relative.generic_string() +
            " cannot go into an Android app: folders starting with '_' are left out of "
            "it. Rename the folder.";
    return false;
  }
  std::error_code fs_error;
  if (copy_over(file, assets / kBundledGameFolder / relative, fs_error)) return true;
  error = "cannot copy " + relative.generic_string() + ": " + fs_error.message();
  return false;
}

// Copies the game into the app's assets. `files` gets where each file is inside them: the app
// cannot list what is inside its own APK, so the list of the game's files travels with it.
bool stage_files(const std::filesystem::path& game_dir, const std::filesystem::path& assets,
                 std::vector<std::string>& files, std::string& error) {
  // (The same files as any other build of the game: no links, hidden files, backups or keys.)
  for (const std::filesystem::path& relative : shipped_files(game_dir)) {
    if (!stage_file(game_dir / relative, relative, assets, error)) return false;
    files.push_back((std::filesystem::path(kBundledGameFolder) / relative).generic_string());
  }
  return true;
}

// The paths of the game's files, sorted, one per line.
std::string listing_of(std::vector<std::string> files) {
  std::sort(files.begin(), files.end());
  std::string listing;
  for (const std::string& file : files) listing += file + "\n";
  return listing;
}

bool stage_runtimes(const std::map<std::string, std::filesystem::path>& runtimes,
                    const std::filesystem::path& stage_dir, std::string& error) {
  for (const auto& [abi, library] : runtimes) {
    std::error_code fs_error;
    if (copy_over(library, stage_dir / kLibrariesFolder / abi / android_library_file(), fs_error)) continue;
    error = "cannot copy the player for " + abi + ": " + fs_error.message();
    return false;
  }
  return true;
}

// The app's manifest, from what the game says about its app (and about itself).
bool manifest_for(const std::filesystem::path& game_dir, std::string& manifest, std::string& error) {
  const GameManifest game = read_game_manifest(game_dir);
  if (!game.android_error.empty()) {
    error = game.android_error;
    return false;
  }
  const AndroidSettings& settings = game.settings.android;
  const std::string package = settings.package.empty() ? android_package_name(game_dir) : settings.package;
  manifest = android_manifest(package, game.settings.title, settings);
  return true;
}

// The folder the app's assets go in, inside a stage folder emptied of what an earlier build left.
std::filesystem::path fresh_assets_folder(const std::filesystem::path& stage_dir) {
  std::error_code fs_error;
  std::filesystem::remove_all(stage_dir, fs_error);
  std::filesystem::path assets = stage_dir / kAssetsFolder;
  std::filesystem::create_directories(assets, fs_error);
  return assets;
}

// Where a player for Android is looked for under `runtimes_dir`, whatever its kind of processor.
std::filesystem::path any_runtime_in(const std::filesystem::path& runtimes_dir) {
  return runtimes_dir / kAnyAbi / android_library_file();
}

// What is checked before anything is staged.
bool can_stage(const std::filesystem::path& game_dir, const std::map<std::string, std::filesystem::path>& runtimes,
               std::string& error) {
  std::error_code fs_error;
  if (!std::filesystem::exists(game_dir / config::kSceneFileName, fs_error)) {
    error = game_dir.string() + " is not a game (no " + config::kSceneFileName + ")";
    return false;
  }
  if (runtimes.empty()) error = missing_runtime_message(BuildTarget::Android, any_runtime_in({}));
  return !runtimes.empty();
}

}  // namespace

bool stage_android_game(const std::filesystem::path& game_folder,
                        const std::map<std::string, std::filesystem::path>& runtimes, const std::string& stamp,
                        const std::filesystem::path& stage_dir, std::string& error) {
  const std::filesystem::path game_dir = normalized_folder(game_folder);
  if (!can_stage(game_dir, runtimes, error)) return false;
  const std::filesystem::path assets = fresh_assets_folder(stage_dir);
  std::vector<std::string> files;
  std::string manifest;
  // (The licenses of what the app contains go inside it, beside the game.)
  const bool staged = stage_files(game_dir, assets, files, error) && stage_runtimes(runtimes, stage_dir, error) &&
                      manifest_for(game_dir, manifest, error) && write_notices(assets, error);
  if (!staged) return false;
  const bool written = write_text(assets / kAndroidManifestAsset, listing_of(std::move(files))) &&
                       write_text(assets / kAndroidStampAsset, stamp) &&
                       write_text(stage_dir / kManifestFile, manifest);
  if (!written) error = "cannot write into " + stage_dir.string();
  return written;
}

namespace {

// The commands that make an APK. Paths and names reach the shell as single quoted words;
// passwords never do: the tools are told which environment variables hold them.
std::string make_key_command(const SigningKey& key) {
  return std::string(kKeytoolTool) + " -genkeypair -keystore " + quoted(key.keystore) + " -storepass:env " +
         shell_word(key.store_password_variable) + " -keypass:env " + shell_word(key.key_password_variable) +
         " -alias " + shell_word(key.alias) + " -keyalg RSA -keysize " + std::to_string(kKeyBits) + " -validity " +
         std::to_string(kKeyValidityDays) + " -dname " + shell_word(kKeyOwner);
}

std::string link_command(const AndroidTools& tools, const std::filesystem::path& stage_dir) {
  return quoted(tools.aapt2) + " link -o " + quoted(stage_dir / kUnsignedApk) + " --manifest " +
         quoted(stage_dir / kManifestFile) + " -I " + quoted(tools.platform) + " -A " +
         quoted(stage_dir / kAssetsFolder);
}

// The engine's libraries go in by their paths inside the APK (lib/<abi>/...), so from the stage folder.
std::string add_libraries_command(const AndroidTools& tools, const std::filesystem::path& stage_dir) {
  std::string add = "cd " + quoted(stage_dir) + " && " + quoted(tools.aapt) + " add " + quoted(kUnsignedApk);
  std::error_code fs_error;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(stage_dir / kLibrariesFolder, fs_error)) {
    if (entry.is_regular_file(fs_error)) add += " " + quoted(entry.path().lexically_relative(stage_dir));
  }
  return add;
}

std::string align_command(const AndroidTools& tools, const std::filesystem::path& stage_dir) {
  return quoted(tools.zipalign) + " -f " + std::to_string(kZipAlignment) + " " + quoted(stage_dir / kUnsignedApk) +
         " " + quoted(stage_dir / kAlignedApk);
}

std::string sign_command(const AndroidTools& tools, const std::filesystem::path& stage_dir, const SigningKey& key,
                         const std::filesystem::path& apk) {
  return quoted(tools.apksigner) + " sign --ks " + quoted(key.keystore) + " --ks-key-alias " + shell_word(key.alias) +
         " --ks-pass " + shell_word(kPasswordFromEnvironment + key.store_password_variable) + " --key-pass " +
         shell_word(kPasswordFromEnvironment + key.key_password_variable) + " --v4-signing-enabled false --out " +
         quoted(apk) + " " + quoted(stage_dir / kAlignedApk);
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

}  // namespace

void keep_to_owner(const std::filesystem::path& file) {
  std::error_code fs_error;
  std::filesystem::permissions(file, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::replace, fs_error);
  if (fs_error) log::warning("cannot keep " + file.string() + " to its owner: " + fs_error.message());
}

bool pack_android_apk(const AndroidTools& tools, const std::filesystem::path& stage_dir, const SigningKey& key,
                      const std::filesystem::path& apk, std::string& error) {
  const std::filesystem::path log = stage_dir / kToolLog;
  if (!ensure_key(key, log, error)) return false;
  if (!run(link_command(tools, stage_dir), log, "packing the game's files", error)) return false;
  if (!run(add_libraries_command(tools, stage_dir), log, "adding the engine", error)) return false;
  if (!run(align_command(tools, stage_dir), log, "aligning the APK", error)) return false;
  std::error_code fs_error;
  std::filesystem::create_directories(apk.parent_path(), fs_error);
  return run(sign_command(tools, stage_dir, key, apk), log, "signing the APK", error);
}

namespace {

// Stages and packs the game in a folder of its own beside the APK, gone when the build ends.
bool build_apk(const std::filesystem::path& game_dir, const std::map<std::string, std::filesystem::path>& runtimes,
               const AndroidTools& tools, const SigningKey& key, const std::filesystem::path& apk, std::string& error) {
  const std::filesystem::path stage = apk.parent_path() / kStageFolder;
  // (The stamp only has to differ from the last build's.)
  const std::string stamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
  const bool built =
      stage_android_game(game_dir, runtimes, stamp, stage, error) && pack_android_apk(tools, stage, key, apk, error);
  std::error_code fs_error;
  std::filesystem::remove_all(stage, fs_error);  // whether it worked or not
  return built;
}

// The key the game's settings ask for (see signing_key_for).
std::optional<SigningKey> signing_key_of(const std::filesystem::path& game_dir, std::string& error) {
  AndroidSettings settings;
  if (!read_android_settings(normalized_folder(game_dir), settings, error)) return std::nullopt;
  return signing_key_for(normalized_folder(game_dir), settings, platform::user_data_dir() / kKeystoreFile, error);
}

// The players a build packs; none, with the folder they were looked for in said, when it is empty.
std::map<std::string, std::filesystem::path> runtimes_to_pack(const std::filesystem::path& runtimes_dir,
                                                              std::string& error) {
  auto runtimes = android_runtimes(runtimes_dir);
  if (runtimes.empty()) error = missing_runtime_message(BuildTarget::Android, any_runtime_in(runtimes_dir));
  return runtimes;
}

// The file a game's app is written to in `out_dir`, named after the game.
std::filesystem::path apk_in(const std::filesystem::path& game_dir, const std::filesystem::path& out_dir) {
  std::error_code fs_error;
  const std::filesystem::path out = std::filesystem::absolute(normalized_folder(out_dir), fs_error);
  return out / (executable_name(game_dir) + kApkExtension);
}

// Whether a build may start: on a system that has the tools, and not into the game itself.
bool can_build(const std::filesystem::path& game_dir, const std::filesystem::path& out_dir, std::string& error) {
  // (The SDK's tools are run through a POSIX shell.)
  if (!platform::has_posix_shell()) {
    error = "Android games are built on Linux";
    return false;
  }
  if (!folder_inside(out_dir, game_dir)) return true;
  error = "cannot build into " + out_dir.string() + ": it is inside the game folder";
  return false;
}

}  // namespace

std::optional<std::filesystem::path> build_android_game(const std::filesystem::path& game_dir,
                                                        const std::filesystem::path& out_dir,
                                                        const std::filesystem::path& runtimes_dir, std::string& error) {
  if (!can_build(game_dir, out_dir, error)) return std::nullopt;
  const auto runtimes = runtimes_to_pack(runtimes_dir, error);
  if (runtimes.empty()) return std::nullopt;
  const auto tools = find_android_tools(android_sdk_from_environment(), error);
  if (!tools) return std::nullopt;
  const auto key = signing_key_of(game_dir, error);
  if (!key) return std::nullopt;
  std::filesystem::path apk = apk_in(game_dir, out_dir);
  if (!build_apk(game_dir, runtimes, *tools, *key, apk, error)) return std::nullopt;
  return apk;
}

}  // namespace redgem
