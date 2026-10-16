#include "testing/ruby_tests.hpp"

#include <mruby.h>

#include <memory>
#include <optional>
#include <string>

#include "physics/physics_config.hpp"
#include "core/config.hpp"
#include "core/exit_codes.hpp"
#include "core/log.hpp"
#include "input/input.hpp"
#include "net/http/http_client.hpp"
#include "net/discovery/lan_discovery.hpp"
#include "net/network_session.hpp"
#include "physics/physics_world.hpp"
#include "scene/scene.hpp"
#include "script/script_context.hpp"
#include "script/script_host.hpp"
#include "testing/recording_audio.hpp"
#include "testing/test_library.hpp"

namespace redgem::testing {
namespace {

// What runs the tests of a file, and how it is asked to.
constexpr const char* kRubyTestModule = "RedGemTest";
constexpr const char* kRunAllMethod = "run_all";

// What a file of Ruby unit tests runs against: a scene, physics, input, audio, a network and a web
// client with nothing in them.
struct BareWorld {
  Scene scene;
  std::unique_ptr<PhysicsWorld> physics = create_jolt_physics_world({0.0f, config::kGravityY, 0.0f});
  InputState input;
  ScriptedInputSource driven_input;  // what TestInput sets (the tests of the system test DSL use it)
  RecordingAudio audio;
  net::NetworkSession network;
  net::LanDiscovery discovery;
  net::HttpClient http;
  ScriptContext context;

  BareWorld() {
    context.scene = &scene;
    context.physics = physics.get();
    context.input = &input;
    context.audio = &audio;
    context.network = &network;
    context.discovery = &discovery;
    context.http = &http;
  }
};

// Whether every test of the file a host loaded passes, without a script error.
bool every_test_passes(ScriptHost& host) {
  mrb_state* mrb = host.vm().state();
  const RubyResult failures = host.vm().call(mrb_obj_value(mrb_module_get(mrb, kRubyTestModule)), kRunAllMethod);
  if (!failures.ok) {
    log::error(failures.error);
    return false;
  }
  return mrb_integer_p(failures.value) && mrb_integer(failures.value) == 0 && host.error_count() == 0;
}

// Why the tests could not even start; nothing when they can.
std::optional<std::string> load_tests(ScriptHost& host, BareWorld& world, const std::filesystem::path& test_file) {
  if (auto error = load_test_library(host, world.driven_input)) return error;
  const RubyResult loaded = host.load_file(test_file);
  if (!loaded.ok) return loaded.error;
  return std::nullopt;
}

// Where the classes the tests use load from: the game's scripts, or the test file's own folder.
std::optional<std::filesystem::path> scripts_for(const std::filesystem::path& test_file,
                                                 const std::filesystem::path& game_dir) {
  if (game_dir.empty()) return test_file.parent_path();
  std::filesystem::path scripts = game_dir / config::kScriptsDirName;
  std::error_code error;
  if (std::filesystem::is_directory(scripts, error)) return scripts;
  log::error(game_dir.string() + " has no " + config::kScriptsDirName + " folder: is it a game's?");
  return std::nullopt;
}

}  // namespace

int run_ruby_tests(const std::filesystem::path& test_file, const std::filesystem::path& game_dir) {
  const std::optional<std::filesystem::path> scripts = scripts_for(test_file, game_dir);
  if (!scripts) return kExitFailure;
  BareWorld world;
  ScriptHost host(world.context, *scripts);
  if (const auto error = load_tests(host, world, test_file)) {
    log::error(*error);
    return kExitFailure;
  }
  return every_test_passes(host) ? kExitSuccess : kExitFailure;
}

}  // namespace redgem::testing
