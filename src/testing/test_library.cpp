#include "testing/test_library.hpp"

#include "generated/testing_ruby.hpp"
#include "testing/test_input_bindings.hpp"

namespace redgem::testing {

std::optional<std::string> load_test_library(ScriptHost& host, ScriptedInputSource& input) {
  install_test_input(host.vm().state(), input);
  for (const auto& source : generated::kTestingSources) {
    const RubyResult loaded = host.eval(source.code, source.file_name);
    if (!loaded.ok) return std::string("cannot load ") + source.file_name + ": " + loaded.error;
  }
  return std::nullopt;
}

}  // namespace redgem::testing
