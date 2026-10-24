// Packing an Android app: its AndroidManifest.xml.

#include "project/android/android_package.hpp"

#include "core/xml_writer.hpp"
#include "project/game_shipping.hpp"

namespace redgem {

namespace {

// The oldest Android the app runs on: the one the engine is built for. It is written once, in
// tools/android/min_api.txt, which the build of the engine for Android reads as well.
constexpr int kMinAndroidApi = REDGEM_ANDROID_MIN_API;
constexpr int kTargetAndroidApi = 34;
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
constexpr const char* kIcon = "android:icon";
constexpr const char* kExported = "android:exported";
constexpr const char* kOrientation = "android:screenOrientation";
constexpr const char* kConfigChanges = "android:configChanges";
}  // namespace attr
// The values that go in them.
constexpr const char* kAndroidNamespace = "http://schemas.android.com/apk/res/android";
constexpr const char* kYes = "true";
constexpr const char* kNo = "false";
constexpr const char* kInternetPermission = "android.permission.INTERNET";
constexpr const char* kLauncherIcon = "@mipmap/ic_launcher";  // see stage_android_icon
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
constexpr const char* kOpenGlEs3 = "0x00030000";  // what the manifest calls OpenGL ES 3.0

// What the app needs from the phone: a recent enough Android, OpenGL ES 3, and the network for a
// game that goes online (a game that does not, does not ask for it).
void add_requirements(XmlElement& manifest, bool network) {
  manifest.child(tag::kUsesSdk)
      .attribute(attr::kMinSdk, std::to_string(kMinAndroidApi))
      .attribute(attr::kTargetSdk, std::to_string(kTargetAndroidApi));
  manifest.child(tag::kUsesFeature).attribute(attr::kGlEsVersion, kOpenGlEs3).attribute(attr::kRequired, kYes);
  if (network) manifest.child(tag::kUsesPermission).attribute(attr::kName, kInternetPermission);
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
XmlElement& add_application(XmlElement& manifest, const std::string& label, bool icon) {
  XmlElement& application = manifest.child(tag::kApplication);
  application.attribute(attr::kLabel, label)
      .attribute(attr::kHasCode, kNo)
      .attribute(attr::kAllowBackup, kNo)
      .attribute(attr::kExtractNativeLibs, kYes)
      .attribute(attr::kTheme, kFullScreenTheme);
  if (icon) application.attribute(attr::kIcon, kLauncherIcon);
  return application;
}

}  // namespace

std::string android_manifest(const std::string& package, const std::string& label, const AndroidSettings& settings,
                             const AndroidAppLook& look) {
  XmlElement manifest(tag::kManifest);
  manifest.attribute(attr::kAndroidNamespace, kAndroidNamespace)
      .attribute(attr::kPackage, package)
      .attribute(attr::kVersionCode, std::to_string(settings.version_code))
      .attribute(attr::kVersionName, android_version_name(settings, look));
  add_requirements(manifest, look.network);
  add_activity(add_application(manifest, label, look.icon));
  return manifest.document();
}

std::string android_version_name(const AndroidSettings& settings, const AndroidAppLook& look) {
  if (!settings.version_name.empty()) return settings.version_name;
  return look.version.empty() ? kDefaultGameVersion : look.version;
}

}  // namespace redgem
