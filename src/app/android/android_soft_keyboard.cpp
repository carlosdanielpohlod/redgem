#include "app/android/android_soft_keyboard.hpp"

#include <android/keycodes.h>
#include <android/native_activity.h>

#include <optional>

namespace redgem {

namespace {

constexpr jint kJniVersion = JNI_VERSION_1_6;
constexpr jint kLocalReferences = 16;  // more than any one call into Java here makes
constexpr jint kNoFlags = 0;
constexpr jint kNoRepeat = 0;
constexpr jlong kNoTime = 0;  // (a key event made only to ask what it types needs no time)
constexpr uint32_t kNoHideFlags = 0;
constexpr int32_t kOnce = 1;  // what a key event that is no ACTION_MULTIPLE stands for
constexpr char32_t kNoLetter = 0;
constexpr const char* kInputMethodService = "input_method";  // Context.INPUT_METHOD_SERVICE
constexpr const char* kKeyEventClass = "android/view/KeyEvent";

// A method of a Java class, by its name and its signature.
struct JavaMethod {
  const char* name;
  const char* signature;
};

constexpr JavaMethod kGetSystemService = {"getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;"};
constexpr JavaMethod kGetWindow = {"getWindow", "()Landroid/view/Window;"};
constexpr JavaMethod kGetDecorView = {"getDecorView", "()Landroid/view/View;"};
constexpr JavaMethod kGetWindowToken = {"getWindowToken", "()Landroid/os/IBinder;"};
constexpr JavaMethod kShowSoftInput = {"showSoftInput", "(Landroid/view/View;I)Z"};
constexpr JavaMethod kHideSoftInput = {"hideSoftInputFromWindow", "(Landroid/os/IBinder;I)Z"};
// KeyEvent(downTime, eventTime, action, code, repeat, metaState, deviceId, scancode)
constexpr JavaMethod kNewKeyEvent = {"<init>", "(JJIIIIII)V"};
constexpr JavaMethod kGetUnicodeChar = {"getUnicodeChar", "(I)I"};

// The keys that edit a text, which a phone's keyboard lets go of the instant it presses them.
struct EditingKey {
  int32_t code;
  Key key;
};

constexpr EditingKey kEditingKeys[] = {{AKEYCODE_DEL, Key::Backspace},
                                       {AKEYCODE_FORWARD_DEL, Key::Delete},
                                       {AKEYCODE_ENTER, Key::Enter},
                                       {AKEYCODE_NUMPAD_ENTER, Key::KeypadEnter}};

// The keyboard reading the activity's events, while one does.
AndroidSoftKeyboard* g_reader = nullptr;

bool push_frame(JNIEnv* env) { return env != nullptr && env->PushLocalFrame(kLocalReferences) == JNI_OK; }

// Room for the local references of a call into Java, freed (and any exception the call left
// cleared) as it ends.
class LocalFrame {
 public:
  explicit LocalFrame(JNIEnv* env) : env_(env), open_(push_frame(env)) {}
  ~LocalFrame();
  LocalFrame(const LocalFrame&) = delete;
  LocalFrame& operator=(const LocalFrame&) = delete;
  // False when the thread has no environment, or no room is left.
  bool open() const { return open_; }

 private:
  JNIEnv* env_;
  bool open_;
};

LocalFrame::~LocalFrame() {
  if (!open_) return;
  env_->ExceptionClear();
  env_->PopLocalFrame(nullptr);
}

// Whether the last call into Java threw: the exception is cleared, so that calls can go on.
bool threw(JNIEnv* env) {
  if (!env->ExceptionCheck()) return false;
  env->ExceptionClear();
  return true;
}

jmethodID method_of(JNIEnv* env, jobject target, const JavaMethod& method) {
  const jclass type = env->GetObjectClass(target);
  const jmethodID id = env->GetMethodID(type, method.name, method.signature);
  return threw(env) ? nullptr : id;
}

// What a method that takes nothing returns; null when there is nothing to ask, or it threw.
jobject object_from(JNIEnv* env, jobject target, const JavaMethod& method) {
  if (target == nullptr) return nullptr;
  const jmethodID id = method_of(env, target, method);
  if (id == nullptr) return nullptr;
  const jobject result = env->CallObjectMethod(target, id);
  return threw(env) ? nullptr : result;
}

// The activity's InputMethodManager.
jobject input_methods(JNIEnv* env, jobject activity) {
  const jmethodID id = method_of(env, activity, kGetSystemService);
  const jstring name = env->NewStringUTF(kInputMethodService);
  if (threw(env) || id == nullptr) return nullptr;
  const jobject manager = env->CallObjectMethod(activity, id, name);
  return threw(env) ? nullptr : manager;
}

jobject decor_view(JNIEnv* env, jobject activity) {
  return object_from(env, object_from(env, activity, kGetWindow), kGetDecorView);
}

bool both_there(jobject one, jobject other) { return one != nullptr && other != nullptr; }

// Asks the InputMethodManager to show or hide the keyboard for a view (or its window): true when
// it took the call.
bool ask_input_methods(JNIEnv* env, jobject manager, const JavaMethod& method, jobject about) {
  if (!both_there(manager, about)) return false;
  const jmethodID id = method_of(env, manager, method);
  if (id == nullptr) return false;
  const jboolean took = env->CallBooleanMethod(manager, id, about, kNoFlags);
  return !threw(env) && took == JNI_TRUE;
}

bool show_through_java(JNIEnv* env, jobject activity) {
  const LocalFrame frame(env);
  if (!frame.open()) return false;
  const jobject view = decor_view(env, activity);
  return ask_input_methods(env, input_methods(env, activity), kShowSoftInput, view);
}

bool hide_through_java(JNIEnv* env, jobject activity) {
  const LocalFrame frame(env);
  if (!frame.open()) return false;
  const jobject window = object_from(env, decor_view(env, activity), kGetWindowToken);
  return ask_input_methods(env, input_methods(env, activity), kHideSoftInput, window);
}

// A Java KeyEvent of the key of an event going down, on the keyboard the event came from (whose
// layout says what the key types); null when it cannot be made.
jobject java_key_event(JNIEnv* env, jclass type, const AInputEvent* event) {
  const jmethodID make = env->GetMethodID(type, kNewKeyEvent.name, kNewKeyEvent.signature);
  if (threw(env)) return nullptr;
  const jobject made = env->NewObject(
      type, make, kNoTime, kNoTime, static_cast<jint>(AKEY_EVENT_ACTION_DOWN),
      static_cast<jint>(AKeyEvent_getKeyCode(event)), kNoRepeat, static_cast<jint>(AKeyEvent_getMetaState(event)),
      static_cast<jint>(AInputEvent_getDeviceId(event)), static_cast<jint>(AKeyEvent_getScanCode(event)));
  return threw(env) ? nullptr : made;
}

std::optional<Key> editing_key(int32_t code) {
  for (const EditingKey& each : kEditingKeys) {
    if (each.code == code) return each.key;
  }
  return std::nullopt;
}

bool has_source(const AInputEvent* event, int32_t source) { return (AInputEvent_getSource(event) & source) == source; }

bool from_gamepad(const AInputEvent* event) {
  return has_source(event, AINPUT_SOURCE_JOYSTICK) || has_source(event, AINPUT_SOURCE_GAMEPAD);
}

// Whether an input event is a key of a keyboard (the screen's, or one plugged in): a gamepad's
// buttons are key events too, and raylib reads those as the gamepad's.
bool keyboard_key(const AInputEvent* event) {
  return AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY && !from_gamepad(event);
}

// Whether a key event is a key going down, not the repeats of one held down.
bool first_press(const AInputEvent* event) {
  return AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_DOWN && AKeyEvent_getRepeatCount(event) == 0;
}

// How many presses a key event stands for: an ACTION_MULTIPLE says how many.
int32_t presses_in(const AInputEvent* event) {
  return AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_MULTIPLE ? AKeyEvent_getRepeatCount(event) : kOnce;
}

int32_t read_input_event(android_app* app, AInputEvent* event) { return g_reader->read_input(app, event); }

void read_app_command(android_app* app, int32_t command) { g_reader->read_command(app, command); }

}  // namespace

JavaThread::JavaThread(JavaVM* vm) : vm_(vm) {
  void* env = nullptr;
  const jint state = vm_->GetEnv(&env, kJniVersion);
  if (state == JNI_OK) {
    env_ = static_cast<JNIEnv*>(env);
  } else if (state == JNI_EDETACHED) {
    attached_here_ = vm_->AttachCurrentThread(&env_, nullptr) == JNI_OK;
    if (!attached_here_) env_ = nullptr;
  }
}

JavaThread::~JavaThread() {
  if (attached_here_) vm_->DetachCurrentThread();
}

AndroidSoftKeyboard::~AndroidSoftKeyboard() {
  if (!reading()) return;
  app_->onInputEvent = raylib_input_;
  app_->onAppCmd = raylib_command_;
  g_reader = nullptr;
}

void AndroidSoftKeyboard::show() {
  // (When the decor view is not the view the system serves input to, Java refuses: the activity
  // is asked, which shows the keyboard until it is told to hide it. It is, when the app leaves the
  // screen and when the game ends.)
  if (!show_through_java(java_.env(), app_->activity->clazz)) {
    ANativeActivity_showSoftInput(app_->activity, ANATIVEACTIVITY_SHOW_SOFT_INPUT_FORCED);
  }
}

void AndroidSoftKeyboard::hide() {
  if (!hide_through_java(java_.env(), app_->activity->clazz)) {
    ANativeActivity_hideSoftInput(app_->activity, kNoHideFlags);
  }
}

void AndroidSoftKeyboard::begin_frame() {
  if (!ready_to_read()) return;
  raylib_input_ = app_->onInputEvent;
  raylib_command_ = app_->onAppCmd;
  app_->onInputEvent = read_input_event;
  app_->onAppCmd = read_app_command;
  g_reader = this;
}

bool AndroidSoftKeyboard::reading() const { return app_->onInputEvent == read_input_event; }

bool AndroidSoftKeyboard::raylib_reads() const { return app_->onInputEvent != nullptr && app_->onAppCmd != nullptr; }

bool AndroidSoftKeyboard::ready_to_read() const { return raylib_reads() && !reading() && source_ != nullptr; }

int32_t AndroidSoftKeyboard::read_input(android_app* app, AInputEvent* event) {
  if (keyboard_key(event)) hand_on(event);
  return raylib_input_(app, event);
}

void AndroidSoftKeyboard::read_command(android_app* app, int32_t command) {
  if (command == APP_CMD_LOST_FOCUS) source_->leave_screen();
  raylib_command_(app, command);
}

void AndroidSoftKeyboard::hand_on(const AInputEvent* event) {
  const int32_t code = AKeyEvent_getKeyCode(event);
  const std::optional<Key> key = editing_key(code);
  if (code == AKEYCODE_BACK) {
    if (first_press(event)) source_->back();
  } else if (key) {
    hand_on_editing(*key, event);
  } else if (AKeyEvent_getAction(event) != AKEY_EVENT_ACTION_UP) {
    type_letters(event);
  }
}

void AndroidSoftKeyboard::hand_on_editing(Key key, const AInputEvent* event) {
  const int32_t action = AKeyEvent_getAction(event);
  if (action == AKEY_EVENT_ACTION_UP) {
    source_->release(key);
  } else if (first_press(event)) {
    source_->press(key);
  } else if (action == AKEY_EVENT_ACTION_MULTIPLE) {
    for (int32_t i = 0; i < presses_in(event); ++i) source_->hit(key);
  }
  // (A key held down repeats its press: it is held already, as on a computer.)
}

void AndroidSoftKeyboard::type_letters(const AInputEvent* event) {
  // (Letters a keyboard sends as a whole text have no key of their own: Java keeps the text.)
  if (AKeyEvent_getKeyCode(event) == AKEYCODE_UNKNOWN) return;
  const char32_t letter = letter_of(event);
  for (int32_t i = 0; i < presses_in(event); ++i) source_->type(letter);
}

char32_t AndroidSoftKeyboard::letter_of(const AInputEvent* event) const {
  JNIEnv* env = java_.env();
  const LocalFrame frame(env);
  if (!frame.open()) return kNoLetter;
  const jclass type = env->FindClass(kKeyEventClass);
  const jobject key = threw(env) ? nullptr : java_key_event(env, type, event);
  const jmethodID read = key == nullptr ? nullptr : method_of(env, key, kGetUnicodeChar);
  if (read == nullptr) return kNoLetter;
  const jint letter = env->CallIntMethod(key, read, static_cast<jint>(AKeyEvent_getMetaState(event)));
  // (A negative number is an accent waiting for its letter: no letter, which typing leaves out.)
  return threw(env) ? kNoLetter : static_cast<char32_t>(letter);
}

}  // namespace redgem
