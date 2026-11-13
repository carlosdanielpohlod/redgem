// The scripts' sampling profiler: the interpreter's call stack read from its code fetch hook.

#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/string.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "core/files.hpp"
#include "script/ruby_vm.hpp"
#include "script/script_sampler.hpp"
#include "test_support.hpp"

using namespace redgem;

namespace {

// A clock a millisecond later at every read: every read of it is due a sample.
struct SteppingClock {
  std::chrono::nanoseconds now{0};
  std::chrono::nanoseconds operator()() {
    now += std::chrono::milliseconds(1);
    return now;
  }
};

constexpr const char* kBusy = R"(
  class Busy
    def self.run
      new.spin
    end

    def spin
      i = 0
      i += 1 while i < 20_000
      i
    end
  end
  Busy.run
)";

bool has_method(const std::vector<ScriptMethodStat>& methods, const std::string& name) {
  for (const ScriptMethodStat& method : methods) {
    if (method.name == name) return true;
  }
  return false;
}

}  // namespace

TEST_CASE("the sampler names the methods on the stack and charges the innermost one") {
  SteppingClock clock;
  ScriptSampler sampler([&clock] { return clock(); });
  RubyVm vm;
  sampler.set_enabled(true);
  vm.budget_keeper().set_sampler(&sampler);
  REQUIRE(vm.eval(kBusy, "busy.rb").ok);
  vm.budget_keeper().set_sampler(nullptr);
  const ScriptProfile& profile = sampler.profile();
  REQUIRE(profile.samples() > 0);
  const std::vector<ScriptMethodStat> top = profile.top(kMaxProfiledMethods);
  REQUIRE_FALSE(top.empty());
  CHECK(top.front().name == "Busy#spin");
  CHECK(top.front().self * 2 > profile.samples());
  CHECK(has_method(top, "Busy.run"));
}

TEST_CASE("a sampler that is off, or not given to the interpreter, samples nothing") {
  SteppingClock clock;
  ScriptSampler sampler([&clock] { return clock(); });
  RubyVm vm;
  vm.budget_keeper().set_sampler(&sampler);
  REQUIRE(vm.eval(kBusy, "busy.rb").ok);
  vm.budget_keeper().set_sampler(nullptr);
  sampler.set_enabled(true);
  REQUIRE(vm.eval(kBusy, "busy.rb").ok);
  CHECK(sampler.profile().samples() == 0);
}

TEST_CASE("profiling a game samples its scripts, lists them and exports them to its data folder") {
  const std::filesystem::path data = std::filesystem::temp_directory_path() / "redgem_script_profile_data";
  std::filesystem::remove_all(data);
  EngineOptions options;
  options.game_dir = test::fixture("games/sandbox");
  options.fixed_step = true;
  options.data_folder = data;
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  Engine& engine = *std::get<std::unique_ptr<Engine>>(created);
  REQUIRE(engine.scripts().eval("Debug.profiling = true", "profile.rb").ok);
  engine.frame(config::kFixedDeltaSeconds);  // the engine turns the sampler on with profiling
  REQUIRE(engine.scripts().vm().sampler().enabled());
  engine.scripts().vm().sampler().sample(engine.scripts().vm().state());
  const RubyResult listed = engine.scripts().eval("Debug.script_profile(3).size", "profile.rb");
  REQUIRE(listed.ok);
  const RubyResult exported = engine.scripts().eval("Debug.export_script_profile", "profile.rb");
  REQUIRE_MESSAGE(exported.ok, exported.error);
  const std::string path = RSTRING_PTR(mrb_obj_as_string(engine.scripts().vm().state(), exported.value));
  CHECK(std::filesystem::path(path) == data / "script_profile.tsv");
  std::string error;
  const auto report = read_text_file(path, error);
  REQUIRE(report.has_value());
  CHECK(report->rfind("# samples: ", 0) == 0);
  REQUIRE(engine.scripts().eval("Debug.reset_frame_stats", "profile.rb").ok);
  CHECK(engine.scripts().vm().sampler().profile().samples() == 0);
  std::filesystem::remove_all(data);
}
