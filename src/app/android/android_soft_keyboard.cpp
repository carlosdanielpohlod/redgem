#include "app/android/android_soft_keyboard.hpp"

#include <android/keycodes.h>
#include <android/native_activity.h>
#include <jni.h>

#include <optional>

namespace redgem {

namespace {

constexpr jint kJniVersion = JNI_VERSION_1_6;
constexpr jint kLocalReferences = 16;  // more than any one call into Java here makes
constexpr jint kNoFlags = 0;
constexpr uint32_t kNoHideFlags = 0;
constexpr int32_t kHandled = 1;    // what an input reader says of an event it took
constexpr int32_t kOneLetter = 1;  // what a key going down types (a repeated one says how many)
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
constexpr JavaMethod kNewKeyEvent = {"<init>", "(II)V"};  // KeyEvent(action, code)
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

// The keyboard reading the activity's input events, while one does.
AndroidSoftKeyboard* g_reader = nullptr;

// A call into Java from the game's thread: the thread is attached to the virtual machine for it
// (and let go after, when it was not attached before), and the call's local references are freed
// and any exception it left is cleared as it ends. No environment when the thread cannot attach.
class JavaCall {
 public:
  explicit JavaCall(JavaVM* vm);
  ~JavaCall();
  JavaCall(const JavaCall&) = delete;
  JavaCall& operator=(const JavaCall&) = delete;
  JNIEnv* env() const { return framed_ ? env_ : nullptr; }

 private:
  void attach();
  // Makes room for the call's local references: false when the thread has no environment, or none
  // is left.
  bool open_frame();

  JavaVM* vm_;
  JNIEnv* env_ = nullptr;
  bool attached_here_ = false;
  bool framed_ = false;
};

JavaCall::JavaCall(JavaVM* vm) : vm_(vm) {
  attach();
  framed_ = open_frame();
}

bool JavaCall::open_frame() {
  if (env_ == nullptr) return false;
  return env_->PushLocalFrame(kLocalReferences) == JNI_OK;
}

void JavaCall::attach() {
  void* env = nullptr;
  const jint state = vm_->GetEnv(&env, kJniVersion);
  if (state == JNI_OK) {
    env_ = static_cast<JNIEnv*>(env);
  } else if (state == JNI_EDETACHED) {
    attached_here_ = vm_->AttachCurrentThread(&env_, nullptr) == JNI_OK;
    if (!attached_here_) env_ = nullptr;
  }
}

JavaCall::~JavaCall() {
  if (env_ != nullptr) env_->ExceptionClear();
  if (framed_) env_->PopLocalFrame(nullptr);
  if (attached_here_) vm_->DetachCurrentThread();
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

bool show_through_java(ANativeActivity* activity) {
  const JavaCall call(activity->vm);
  JNIEnv* env = call.env();
  if (env == nullptr) return false;
  const jobject view = decor_view(env, activity->clazz);
  return ask_input_methods(env, input_methods(env, activity->clazz), kShowSoftInput, view);
}

bool hide_through_java(ANativeActivity* activity) {
  const JavaCall call(activity->vm);
  JNIEnv* env = call.env();
  if (env == nullptr) return false;
  const jobject window = object_from(env, decor_view(env, activity->clazz), kGetWindowToken);
  return ask_input_methods(env, input_methods(env, activity->clazz), kHideSoftInput, window);
}

// A Java KeyEvent of a key going down; null when it cannot be made.
jobject key_event(JNIEnv* env, jclass type, int32_t code) {
  const jmethodID make = env->GetMethodID(type, kNewKeyEvent.name, kNewKeyEvent.signature);
  if (threw(env)) return nullptr;
  const jobject event = env->NewObject(type, make, static_cast<jint>(AKEY_EVENT_ACTION_DOWN), static_cast<jint>(code));
  return threw(env) ? nullptr : event;
}

// The letter a key types with the modifier keys `meta` held, as Java's KeyEvent says: none for a
// key that types none (and a negative number, an accent waiting for its letter, reads as no letter).
char32_t letter_of(ANativeActivity* activity, int32_t code, int32_t meta) {
  const JavaCall call(activity->vm);
  JNIEnv* env = call.env();
  if (env == nullptr) return kNoLetter;
  const jclass type = env->FindClass(kKeyEventClass);
  const jobject event = threw(env) ? nullptr : key_event(env, type, code);
  const jmethodID read = event == nullptr ? nullptr : method_of(env, event, kGetUnicodeChar);
  if (read == nullptr) return kNoLetter;
  const jint letter = env->CallIntMethod(event, read, static_cast<jint>(meta));
  return threw(env) ? kNoLetter : static_cast<char32_t>(letter);
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

int32_t read_input_event(android_app* app, AInputEvent* event) {
  if (g_reader->takes(event)) return kHandled;
  return g_reader->hand_to_raylib(app, event);
}

}  // namespace

AndroidSoftKeyboard::~AndroidSoftKeyboard() {
  if (!reading()) return;
  app_->onInputEvent = raylib_reader_;
  g_reader = nullptr;
}

void AndroidSoftKeyboard::show() {
  // (A decor view that is not the one the system serves input to refuses: the activity is asked.)
  if (!show_through_java(app_->activity)) {
    ANativeActivity_showSoftInput(app_->activity, ANATIVEACTIVITY_SHOW_SOFT_INPUT_FORCED);
  }
}

void AndroidSoftKeyboard::hide() {
  if (!hide_through_java(app_->activity)) ANativeActivity_hideSoftInput(app_->activity, kNoHideFlags);
}

void AndroidSoftKeyboard::begin_frame() {
  if (!ready_to_read()) return;
  raylib_reader_ = app_->onInputEvent;
  app_->onInputEvent = read_input_event;
  g_reader = this;
}

bool AndroidSoftKeyboard::reading() const { return app_->onInputEvent == read_input_event; }

bool AndroidSoftKeyboard::ready_to_read() const {
  return !reading() && source_ != nullptr && app_->onInputEvent != nullptr;
}

bool AndroidSoftKeyboard::takes(const AInputEvent* event) {
  if (!keyboard_key(event)) return false;
  if (AKeyEvent_getKeyCode(event) == AKEYCODE_BACK) {
    return source_->back(AKeyEvent_getAction(event) != AKEY_EVENT_ACTION_UP);
  }
  hand_on(event);
  return false;
}

void AndroidSoftKeyboard::hand_on(const AInputEvent* event) {
  const int32_t action = AKeyEvent_getAction(event);
  const std::optional<Key> key = editing_key(AKeyEvent_getKeyCode(event));
  if (key) {
    hand_on_editing(*key, action);
  } else if (action != AKEY_EVENT_ACTION_UP) {
    type_letters(event);
  }
}

void AndroidSoftKeyboard::hand_on_editing(Key key, int32_t action) {
  if (action == AKEY_EVENT_ACTION_DOWN) {
    source_->press(key);
  } else if (action == AKEY_EVENT_ACTION_UP) {
    source_->release(key);
  } else {
    source_->hit(key);  // (several presses in one event: it was hit)
  }
}

void AndroidSoftKeyboard::type_letters(const AInputEvent* event) {
  const int32_t code = AKeyEvent_getKeyCode(event);
  // (Letters a keyboard sends as a whole text have no key of their own: Java keeps the text.)
  if (code == AKEYCODE_UNKNOWN) return;
  const bool repeated = AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_MULTIPLE;
  const int32_t times = repeated ? AKeyEvent_getRepeatCount(event) : kOneLetter;
  const char32_t letter = letter_of(app_->activity, code, AKeyEvent_getMetaState(event));
  for (int32_t i = 0; i < times; ++i) source_->type(letter);
}

}  // namespace redgem
