#include "app/android_package.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>

#include "app/game_project.hpp"
#include "core/config.hpp"
#include "core/platform.hpp"
#include "core/xml_writer.hpp"

#include <nlohmann/json.hpp>

namespace redgem {

namespace {

constexpr const char* kSdkVariables[] = {"ANDROID_HOME", "ANDROID_SDK_ROOT"};
constexpr const char* kBuildToolsFolder = "build-tools";
constexpr const char* kPlatformsFolder = "platforms";
constexpr const char* kPlatformJar = "android.jar";
constexpr const char* kPackagePrefix = "com.redgem.";
constexpr const char* kFallbackPackage = "game";
constexpr const char* kLibraryFile = "libredgem.so";
constexpr const char* kAssetsFolder = "assets";
constexpr const char* kLibrariesFolder = "lib";
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
constexpr const char* kAndroidSection = "android";
constexpr const char* kSettingsFileName = "game.json";
constexpr std::size_t kMaxPackageLength = 150;
constexpr int kKeyValidityDays = 10000;
constexpr int kZipAlignment = 4;       // bytes: what Android wants uncompressed entries aligned to
constexpr int kMinAndroidApi = 26;     // Android 8 (the engine is built for it, see tools/android)
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
constexpr const char* kOpenGlEs3 = "0x00030000";        // what the manifest calls OpenGL ES 3.0
constexpr const char* kStageFolder = ".redgem-apk-stage";  // inside the output folder, gone when the build ends
constexpr const char* kToolLog = "tool-output.log";
constexpr std::size_t kShownToolLines = 12;             // of a failed tool's output
constexpr char kHiddenPrefix = '.';
constexpr char kBackupSuffix = '~';
constexpr char kSkippedFolderPrefix = '_';              // the SDK's packer leaves such folders out
constexpr std::uint32_t kHashSeed = 2166136261u;        // FNV-1a
constexpr std::uint32_t kHashPrime = 16777619u;
constexpr std::uint32_t kHashShown = 0xFFFFFF;          // six hex digits of it

std::string quoted(const std::filesystem::path& path) {
  std::string out = "'";
  for (const char c : path.string()) out += c == '\'' ? std::string("'\\''") : std::string(1, c);
  return out + "'";
}

// A text as one word of a command, whatever is in it.
std::string shell_word(const std::string& text) { return quoted(std::filesystem::path(text)); }

// Runs a tool, keeping what it says in `log`. When it fails, `error` is what it was doing and the
// last lines it said (not the command itself, which may carry a password).
bool run(const std::string& command, const std::filesystem::path& log, const std::string& what, std::string& error) {
  if (std::system((command + " >" + quoted(log) + " 2>&1").c_str()) == 0) return true;
  std::vector<std::string> lines;
  std::ifstream said(log);
  for (std::string line; std::getline(said, line);) lines.push_back(line);
  error = what + " failed";
  const std::size_t first = lines.size() > kShownToolLines ? lines.size() - kShownToolLines : 0;
  for (std::size_t i = first; i < lines.size(); ++i) error += "\n  " + lines[i];
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

// Files that are not part of a game (hidden ones, editors' backups) or must never leave the
// developer's machine (signing keys) stay out of the APK.
bool leftover(const std::filesystem::path& relative) {
  for (const std::filesystem::path& part : relative) {
    const std::string name = part.string();
    if (!name.empty() && (name.front() == kHiddenPrefix || name.back() == kBackupSuffix)) return true;
  }
  return signing_key_file(relative);
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

std::optional<AndroidTools> find_android_tools(const std::filesystem::path& sdk, std::string& error) {
  if (sdk.empty()) {
    error = "the Android SDK was not found: set ANDROID_HOME to where it is installed";
    return std::nullopt;
  }
  std::error_code fs_error;
  const std::filesystem::path root = std::filesystem::absolute(sdk, fs_error);  // (tools are run from other folders)
  const std::filesystem::path build_tools = newest_in(root / kBuildToolsFolder);
  const std::filesystem::path platform = newest_in(root / kPlatformsFolder);
  AndroidTools tools;
  tools.aapt = build_tools / "aapt";
  tools.aapt2 = build_tools / "aapt2";
  tools.zipalign = build_tools / "zipalign";
  tools.apksigner = build_tools / "apksigner";
  tools.platform = platform / kPlatformJar;
  for (const std::filesystem::path* needed : {&tools.aapt, &tools.aapt2, &tools.zipalign, &tools.apksigner, &tools.platform}) {
    if (std::filesystem::exists(*needed, fs_error)) continue;
    error = "the Android SDK in " + sdk.string() + " is missing " + needed->filename().string() +
            ": install its build-tools and a platform (sdkmanager \"build-tools;35.0.0\" \"platforms;android-35\")";
    return std::nullopt;
  }
  return tools;
}

std::string android_package_name(const std::filesystem::path& game_dir) {
  const std::string folder = normalized_folder(game_dir).filename().string();
  std::string name;
  bool plain = true;  // nothing but small letters and digits: the name is the id as it is
  for (const char c : folder) {
    const auto letter = static_cast<unsigned char>(c);
    if (std::isalnum(letter) != 0) name.push_back(static_cast<char>(std::tolower(letter)));
    plain = plain && (std::islower(letter) != 0 || std::isdigit(letter) != 0);
  }
  // A part of an app's id starts with a letter.
  if (name.empty() || std::isalpha(static_cast<unsigned char>(name.front())) == 0) name = kFallbackPackage + name;
  if (plain) return kPackagePrefix + name;
  // "My Game", "my-game" and "mygame" are three games: what was dropped from the name comes
  // back as a few digits, so one does not install over another.
  std::uint32_t hash = kHashSeed;
  for (const char c : folder) hash = (hash ^ static_cast<unsigned char>(c)) * kHashPrime;
  char digits[sizeof("ffffff")];
  std::snprintf(digits, sizeof(digits), "%06x", hash & kHashShown);
  return kPackagePrefix + name + "_" + digits;
}

namespace {

// An app's id is at least two parts joined by dots, each a small letter followed by small
// letters, digits or underscores ("com.mystudio.my_game").
bool valid_package(const std::string& package) {
  if (package.empty() || package.size() > kMaxPackageLength || package.find('.') == std::string::npos) return false;
  bool part_start = true;
  for (const char c : package) {
    const auto letter = static_cast<unsigned char>(c);
    if (c == '.') {
      if (part_start) return false;  // an empty part
      part_start = true;
    } else if (part_start) {
      if (std::islower(letter) == 0) return false;
      part_start = false;
    } else if (std::islower(letter) == 0 && std::isdigit(letter) == 0 && c != '_') {
      return false;
    }
  }
  return !part_start;
}

bool in_environment(const char* variable) {
  const char* value = std::getenv(variable);
  return value != nullptr && *value != '\0';
}

}  // namespace

bool read_android_settings(const std::filesystem::path& game_dir, AndroidSettings& settings, std::string& error) {
  settings = AndroidSettings{};
  std::ifstream in(game_dir / kSettingsFileName);
  if (!in) return true;
  const nlohmann::json json = nlohmann::json::parse(in, nullptr, false);
  if (!json.is_object() || !json.contains(kAndroidSection)) return true;
  const nlohmann::json& section = json[kAndroidSection];
  const std::string where = std::string(kSettingsFileName) + ": android.";
  const auto text = [&](const char* key, std::string& into) {
    if (!section.contains(key)) return true;
    if (!section[key].is_string()) {
      error = where + key + " must be a text";
      return false;
    }
    into = section[key].get<std::string>();
    return true;
  };
  std::string keystore;
  if (!section.is_object()) {
    error = std::string(kSettingsFileName) + ": android must be a section ({ ... })";
    return false;
  }
  if (!text("package", settings.package) || !text("version_name", settings.version_name) ||
      !text("keystore", keystore) || !text("key_alias", settings.key_alias)) {
    return false;
  }
  settings.keystore = keystore;
  if (section.contains("version_code")) {
    if (!section["version_code"].is_number_integer() || section["version_code"] < 1) {
      error = where + "version_code must be a whole number, 1 or more";
      return false;
    }
    settings.version_code = section["version_code"].get<int>();
  }
  if (!settings.package.empty() && !valid_package(settings.package)) {
    error = where + "package must look like com.mystudio.mygame (small letters, digits and _, parts joined by dots)";
    return false;
  }
  if (!settings.keystore.empty() && settings.key_alias.empty()) {
    error = where + "key_alias is needed with a keystore: the name of the key inside it";
    return false;
  }
  return true;
}

std::optional<SigningKey> signing_key_for(const std::filesystem::path& game_dir, const AndroidSettings& settings,
                                          const std::filesystem::path& engine_keystore, std::string& error) {
  SigningKey key;
  if (settings.keystore.empty()) {
    // (The tools read it from the environment, where this process puts it.)
    platform::set_environment(kEnginePasswordVariable, kKeyPassword, true);
    key.keystore = engine_keystore;
    key.alias = kKeyAlias;
    key.store_password_variable = kEnginePasswordVariable;
    key.key_password_variable = kEnginePasswordVariable;
    return key;
  }
  std::error_code fs_error;
  key.keystore = std::filesystem::absolute(settings.keystore.is_absolute() ? settings.keystore : game_dir / settings.keystore, fs_error);
  if (!std::filesystem::is_regular_file(key.keystore, fs_error)) {
    error = "the keystore " + key.keystore.string() + " does not exist";
    return std::nullopt;
  }
  if (!in_environment(kKeystorePasswordVariable)) {
    error = std::string("the keystore's password is not known: put it in the environment as ") + kKeystorePasswordVariable +
            " (and the key's own in " + kKeyPasswordVariable + ", when it differs)";
    return std::nullopt;
  }
  key.alias = settings.key_alias;
  key.store_password_variable = kKeystorePasswordVariable;
  key.key_password_variable = in_environment(kKeyPasswordVariable) ? kKeyPasswordVariable : kKeystorePasswordVariable;
  return key;
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

}  // namespace

std::string android_manifest(const std::string& package, const std::string& label, const AndroidSettings& settings) {
  XmlElement manifest(tag::kManifest);
  manifest.attribute(attr::kAndroidNamespace, kAndroidNamespace)
      .attribute(attr::kPackage, package)
      .attribute(attr::kVersionCode, std::to_string(settings.version_code))
      .attribute(attr::kVersionName, settings.version_name);
  add_requirements(manifest);
  // (No Java of its own: hasCode is off.)
  XmlElement& application = manifest.child(tag::kApplication);
  application.attribute(attr::kLabel, label)
      .attribute(attr::kHasCode, kNo)
      .attribute(attr::kExtractNativeLibs, kYes)
      .attribute(attr::kTheme, kFullScreenTheme);
  add_activity(application);
  return manifest.document();
}

std::map<std::string, std::filesystem::path> android_runtimes(const std::filesystem::path& runtimes_dir) {
  std::map<std::string, std::filesystem::path> runtimes;
  std::error_code fs_error;
  for (const auto& entry : std::filesystem::directory_iterator(runtimes_dir, fs_error)) {
    const std::filesystem::path library = entry.path() / kLibraryFile;
    if (std::filesystem::is_regular_file(library, fs_error)) runtimes[entry.path().filename().string()] = library;
  }
  return runtimes;
}

bool stage_android_game(const std::filesystem::path& game_folder,
                        const std::map<std::string, std::filesystem::path>& runtimes, const std::string& stamp,
                        const std::filesystem::path& stage_dir, std::string& error) {
  const std::filesystem::path game_dir = normalized_folder(game_folder);
  std::error_code fs_error;
  if (!std::filesystem::exists(game_dir / config::kSceneFileName, fs_error)) {
    error = game_dir.string() + " is not a game (no " + config::kSceneFileName + ")";
    return false;
  }
  if (runtimes.empty()) {
    error = "the android runtime is not built yet: run tools/android/build_runtime.sh";
    return false;
  }
  std::filesystem::remove_all(stage_dir, fs_error);
  const std::filesystem::path assets = stage_dir / kAssetsFolder;
  std::filesystem::create_directories(assets, fs_error);
  // The app cannot list what is inside its own APK: the list of the game's files travels with it.
  std::vector<std::string> files;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(game_dir, fs_error)) {
    if (!entry.is_regular_file(fs_error)) continue;
    const std::filesystem::path relative = entry.path().lexically_relative(game_dir);
    if (leftover(relative)) continue;
    if (in_skipped_folder(relative)) {
      error = relative.generic_string() + " cannot go into an Android app: folders starting with '_' are left out of "
              "it. Rename the folder.";
      return false;
    }
    const std::filesystem::path copy = assets / kBundledGameFolder / relative;
    std::filesystem::create_directories(copy.parent_path(), fs_error);
    std::filesystem::copy_file(entry.path(), copy, std::filesystem::copy_options::overwrite_existing, fs_error);
    if (fs_error) {
      error = "cannot copy " + relative.generic_string() + ": " + fs_error.message();
      return false;
    }
    files.push_back((std::filesystem::path(kBundledGameFolder) / relative).generic_string());
  }
  std::sort(files.begin(), files.end());
  std::string listing;
  for (const std::string& file : files) listing += file + "\n";
  for (const auto& [abi, library] : runtimes) {
    const std::filesystem::path folder = stage_dir / kLibrariesFolder / abi;
    std::filesystem::create_directories(folder, fs_error);
    std::filesystem::copy_file(library, folder / kLibraryFile, std::filesystem::copy_options::overwrite_existing, fs_error);
    if (fs_error) {
      error = "cannot copy the engine for " + abi + ": " + fs_error.message();
      return false;
    }
  }
  AndroidSettings settings;
  if (!read_android_settings(game_dir, settings, error)) return false;
  const std::string package = settings.package.empty() ? android_package_name(game_dir) : settings.package;
  const std::string manifest = android_manifest(package, read_game_settings(game_dir).title, settings);
  if (!write_text(assets / kAndroidManifestAsset, listing) || !write_text(assets / kAndroidStampAsset, stamp) ||
      !write_text(stage_dir / kManifestFile, manifest)) {
    error = "cannot write into " + stage_dir.string();
    return false;
  }
  return true;
}

bool pack_android_apk(const AndroidTools& tools, const std::filesystem::path& stage_dir, const SigningKey& key,
                      const std::filesystem::path& apk, std::string& error) {
  const std::filesystem::path unsigned_apk = stage_dir / kUnsignedApk;
  const std::filesystem::path aligned_apk = stage_dir / kAlignedApk;
  const std::filesystem::path log = stage_dir / kToolLog;
  std::error_code fs_error;
  // Only the engine's own key is ever made here; a developer's is theirs to make and keep.
  if (!std::filesystem::exists(key.keystore, fs_error)) {
    std::filesystem::create_directories(key.keystore.parent_path(), fs_error);
    const std::string make_key = "keytool -genkeypair -keystore " + quoted(key.keystore) + " -storepass:env " +
                                 key.store_password_variable + " -keypass:env " + key.key_password_variable +
                                 " -alias " + shell_word(key.alias) + " -keyalg RSA -keysize " + std::to_string(kKeyBits) +
                                 " -validity " + std::to_string(kKeyValidityDays) + " -dname 'CN=RedGem'";
    if (!run(make_key, log, "making the signing key (is Java installed?)", error)) return false;
  }
  const std::string link = quoted(tools.aapt2) + " link -o " + quoted(unsigned_apk) + " --manifest " +
                           quoted(stage_dir / kManifestFile) + " -I " + quoted(tools.platform) + " -A " +
                           quoted(stage_dir / kAssetsFolder);
  if (!run(link, log, "packing the game's files", error)) return false;
  // The engine's libraries go in by their paths inside the APK (lib/<abi>/...), so from the stage folder.
  std::string add = "cd " + quoted(stage_dir) + " && " + quoted(tools.aapt) + " add " + kUnsignedApk;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(stage_dir / kLibrariesFolder, fs_error)) {
    if (entry.is_regular_file(fs_error)) add += " " + quoted(entry.path().lexically_relative(stage_dir));
  }
  if (!run(add, log, "adding the engine", error)) return false;
  const std::string align = quoted(tools.zipalign) + " -f " + std::to_string(kZipAlignment) + " " +
                            quoted(unsigned_apk) + " " + quoted(aligned_apk);
  if (!run(align, log, "aligning the APK", error)) return false;
  std::filesystem::create_directories(apk.parent_path(), fs_error);
  const std::string sign = quoted(tools.apksigner) + " sign --ks " + quoted(key.keystore) + " --ks-key-alias " +
                           shell_word(key.alias) + " --ks-pass env:" + key.store_password_variable + " --key-pass env:" +
                           key.key_password_variable + " --v4-signing-enabled false --out " + quoted(apk) + " " +
                           quoted(aligned_apk);
  return run(sign, log, "signing the APK", error);
}

std::optional<std::filesystem::path> build_android_game(const std::filesystem::path& game_dir,
                                                        const std::filesystem::path& out_dir,
                                                        const std::filesystem::path& runtimes_dir, std::string& error) {
#ifdef _WIN32
  error = "Android games are built on Linux";
  return std::nullopt;
#endif
  if (folder_inside(out_dir, game_dir)) {
    error = "cannot build into " + out_dir.string() + ": it is inside the game folder";
    return std::nullopt;
  }
  const auto tools = find_android_tools(android_sdk_from_environment(), error);
  if (!tools) return std::nullopt;
  std::error_code fs_error;
  const std::filesystem::path out = std::filesystem::absolute(normalized_folder(out_dir), fs_error);
  const std::filesystem::path stage = out / kStageFolder;
  const std::filesystem::path apk = out / (executable_name(game_dir) + kApkExtension);
  AndroidSettings settings;
  if (!read_android_settings(normalized_folder(game_dir), settings, error)) return std::nullopt;
  const auto key = signing_key_for(normalized_folder(game_dir), settings,
                                   recent_games_file().parent_path() / kKeystoreFile, error);
  if (!key) return std::nullopt;
  // (The stamp only has to differ from the last build's.)
  const std::string stamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
  const bool built = stage_android_game(game_dir, android_runtimes(runtimes_dir), stamp, stage, error) &&
                     pack_android_apk(*tools, stage, *key, apk, error);
  std::filesystem::remove_all(stage, fs_error);  // whether it worked or not
  if (!built) return std::nullopt;
  return apk;
}

}  // namespace redgem
