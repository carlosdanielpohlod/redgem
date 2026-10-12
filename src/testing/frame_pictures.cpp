#include "testing/frame_pictures.hpp"

#include <mruby/variable.h>
#include <raylib.h>
#include <rlgl.h>

#include "core/log.hpp"
#include "core/safe_path.hpp"
#include "testing/testing_config.hpp"

namespace redgem::testing {
namespace {

// Where the VM keeps the pointer to the pictures its TestScreen asks for: a variable of the module
// itself, whose name Ruby code can neither read nor write (it does not start with @).
constexpr const char* kPicturesVariable = "__frame_pictures";

mrb_value capture(mrb_state* mrb, mrb_value self) {
  const char* name = nullptr;
  mrb_get_args(mrb, "z", &name);
  const mrb_value kept = mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, kPicturesVariable));
  const bool has_pictures = mrb_cptr_p(kept);
  if (!has_pictures) mrb_raise(mrb, E_RUNTIME_ERROR, "TestScreen has no game to take pictures of");
  static_cast<FramePictures*>(mrb_cptr(kept))->ask(name);
  return mrb_nil_value();
}

}  // namespace

std::optional<std::filesystem::path> picture_file(const std::filesystem::path& folder, const std::string& name) {
  if (safe_asset_name(name)) return folder / (name + config::kScreenshotExtension);
  warn_of_unsafe_asset_name(name);
  return std::nullopt;
}

void FramePictures::ask(const std::string& name) {
  if (const auto file = picture_file(folder_, name)) pending_.push_back(*file);
}

void FramePictures::save_pending() {
  if (pending_.empty()) return;
  rlDrawRenderBatchActive();  // what was drawn and not yet sent to the card belongs in the picture
  Image frame = LoadImageFromScreen();
  for (const std::filesystem::path& path : pending_) {
    std::filesystem::create_directories(path.parent_path());
    if (!ExportImage(frame, path.string().c_str())) log::error("cannot write screenshot " + path.string());
  }
  UnloadImage(frame);
  pending_.clear();
}

void install_test_screen(mrb_state* mrb, FramePictures& pictures) {
  RClass* module = mrb_define_module(mrb, kTestScreenModule);
  mrb_iv_set(mrb, mrb_obj_value(module), mrb_intern_cstr(mrb, kPicturesVariable), mrb_cptr_value(mrb, &pictures));
  mrb_define_module_function(mrb, module, "capture", capture, MRB_ARGS_REQ(1));
}

}  // namespace redgem::testing
