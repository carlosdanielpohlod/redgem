// Packing an Android app: the key it is signed with.

#include "project/android/android_package.hpp"

#include "core/log.hpp"
#include "core/platform.hpp"

namespace redgem {

namespace {

// The engine's own key lives in the user's RedGem folder, on the machine that made it. It only
// says "the same machine built this again", so its password is no secret; it reaches the tools
// through the environment all the same, like a developer's own.
constexpr const char* kKeyAlias = "redgem";
constexpr const char* kKeyPassword = "redgem-debug";
constexpr const char* kEnginePasswordVariable = "REDGEM_ENGINE_KEY_PASSWORD";

bool in_environment(const char* variable) { return !platform::environment(variable).empty(); }

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

std::optional<SigningKey> signing_key_for(const std::filesystem::path& game_dir, const AndroidSettings& settings,
                                          const std::filesystem::path& engine_keystore, std::string& error) {
  if (settings.keystore.empty()) return engine_key(engine_keystore);
  return developer_key(game_dir, settings, error);
}

void keep_to_owner(const std::filesystem::path& file) {
  std::error_code fs_error;
  std::filesystem::permissions(file, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::replace, fs_error);
  if (fs_error) log::warning("cannot keep " + file.string() + " to its owner: " + fs_error.message());
}

}  // namespace redgem
