#include "testing/frame_pictures.hpp"

#include <cstdlib>
#include <system_error>

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

FramePictures& pictures_of(mrb_state* mrb, mrb_value self);

mrb_value capture(mrb_state* mrb, mrb_value self) {
  const char* name = nullptr;
  mrb_get_args(mrb, "z", &name);
  pictures_of(mrb, self).ask(name);
  return mrb_nil_value();
}

FramePictures& pictures_of(mrb_state* mrb, mrb_value self) {
  const mrb_value kept = mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, kPicturesVariable));
  const bool has_pictures = mrb_cptr_p(kept);  // (a macro: named, so that it reads as one test)
  if (!has_pictures) mrb_raise(mrb, E_RUNTIME_ERROR, "TestScreen has no game to take pictures of");
  return *static_cast<FramePictures*>(mrb_cptr(kept));
}

bool channel_moved(const unsigned char* one, const unsigned char* other, int channels) {
  for (int channel = 0; channel < channels; ++channel) {
    if (std::abs(static_cast<int>(one[channel]) - static_cast<int>(other[channel])) > kBaselineTolerance) return true;
  }
  return false;
}

constexpr int kRgbaChannels = 4;
constexpr float kAllDiffer = 1.0f;

// The share of the pixels of two pictures of the same size that differ.
float differing_share(const Image& one, const Image& other) {
  const auto* a = static_cast<const unsigned char*>(one.data);
  const auto* b = static_cast<const unsigned char*>(other.data);
  const long pixels = static_cast<long>(one.width) * one.height;
  long moved = 0;
  for (long pixel = 0; pixel < pixels; ++pixel) {
    if (channel_moved(a + pixel * kRgbaChannels, b + pixel * kRgbaChannels, kRgbaChannels)) ++moved;
  }
  return pixels > 0 ? static_cast<float>(moved) / static_cast<float>(pixels) : kAllDiffer;
}

bool same_size(const Image& one, const Image& other) { return one.width == other.width && one.height == other.height; }

// The picture as RGBA bytes; one with no data when the file cannot be read.
Image rgba_picture(const std::filesystem::path& path) {
  Image image = LoadImage(path.string().c_str());
  if (IsImageValid(image)) ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  return image;
}

bool updating_baselines() { return std::getenv(kUpdateBaselinesVariable) != nullptr; }

// Whether a picture is written over a baseline now: while the baselines are recorded again, and
// only over its own (of its name). Compared with another's, it is compared, not copied there.
bool records_baseline(const std::string& name, const std::filesystem::path& baseline) {
  return updating_baselines() && baseline.stem() == std::filesystem::path(name).filename();
}

// The picture written as its baseline (TestScreen.difference while the baselines are recorded again).
mrb_value written_as_baseline(mrb_state* mrb, const std::filesystem::path& picture,
                              const std::filesystem::path& baseline) {
  std::error_code failed;
  std::filesystem::create_directories(baseline.parent_path(), failed);
  std::filesystem::copy_file(picture, baseline, std::filesystem::copy_options::overwrite_existing, failed);
  if (failed) mrb_raisef(mrb, E_RUNTIME_ERROR, "cannot write the baseline %s", baseline.string().c_str());
  return mrb_float_value(mrb, 0.0);
}

mrb_value difference(mrb_state* mrb, mrb_value self) {
  const char* name = nullptr;
  const char* relative = nullptr;
  mrb_get_args(mrb, "zz", &name, &relative);
  const FramePictures& pictures = pictures_of(mrb, self);
  const std::filesystem::path picture = pictures.picture_path(name);
  const std::filesystem::path baseline = pictures.baseline_path(relative);
  if (picture.empty() || baseline.empty()) mrb_raise(mrb, E_ARGUMENT_ERROR, "TestScreen.difference: unsafe name");
  if (records_baseline(name, baseline)) return written_as_baseline(mrb, picture, baseline);
  const std::optional<float> share = picture_difference(picture, baseline);
  if (!share) mrb_raisef(mrb, E_RUNTIME_ERROR, "cannot read %s or %s", picture.string().c_str(), relative);
  return mrb_float_value(mrb, *share);
}

}  // namespace

std::optional<float> picture_difference(const std::filesystem::path& picture, const std::filesystem::path& baseline) {
  Image one = rgba_picture(picture);
  Image other = rgba_picture(baseline);
  std::optional<float> share;
  if (IsImageValid(one) && IsImageValid(other))
    share = same_size(one, other) ? differing_share(one, other) : kAllDiffer;
  UnloadImage(one);
  UnloadImage(other);
  return share;
}

std::filesystem::path FramePictures::picture_path(const std::string& name) const {
  return picture_file(folder_, name).value_or(std::filesystem::path());
}

std::filesystem::path FramePictures::baseline_path(const std::string& relative) const {
  if (!safe_asset_name(relative)) return {};
  return game_dir_ / relative;
}

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
  mrb_define_module_function(mrb, module, "difference", difference, MRB_ARGS_REQ(2));
}

}  // namespace redgem::testing
