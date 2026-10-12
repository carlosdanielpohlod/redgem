#include "testing/test_library.hpp"

#include "generated/testing_ruby.hpp"
#include "testing/test_input_bindings.hpp"

namespace redgem::testing {

std::optional<std::string> load_test_library(ScriptHost& host, ScriptedInputSource& input, FramePictures* pictures) {
  install_test_input(host.vm().state(), input);
  if (pictures != nullptr) install_test_screen(host.vm().state(), *pictures);
  for (const auto& source : generated::kTestingSources) {
    const RubyResult loaded = host.eval(source.code, source.file_name);
    if (!loaded.ok) return std::string("cannot load ") + source.file_name + ": " + loaded.error;
  }
  return std::nullopt;
}

}  // namespace redgem::testing
