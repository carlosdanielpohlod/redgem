#include "app/profile_overlay.hpp"

#include <iomanip>
#include <sstream>

#include "script/script_context.hpp"
#include "script/script_profile.hpp"

namespace redgem {
namespace {
constexpr int kMargin = 12;
constexpr int kVerticalMargins = 2;
constexpr int kTextSize = 16;
constexpr int kLineHeight = 22;
constexpr int kPanelWidth = 420;
constexpr int kPrecision = 2;  // milliseconds retain sub-millisecond phase differences
constexpr Color kPanelColor{0.0f, 0.0f, 0.0f, 0.85f};
constexpr Color kTextColor{1.0f, 1.0f, 1.0f, 1.0f};
constexpr const char* kGpuPrefix = "GPU ";
constexpr const char* kMilliseconds = " ms";
constexpr const char* kScriptsTitle = "Scripts (";
constexpr const char* kScriptsSuffix = " samples):";

std::string measured(const std::string& label, double value, const char* unit) {
  std::ostringstream text;
  text << label << ": " << std::fixed << std::setprecision(kPrecision) << value << unit;
  return text.str();
}
void add_frame_lines(std::vector<std::string>& lines, const ProfileSummary& stats) {
  lines.push_back(stats.mean.has_interval ? measured("FPS", stats.frames_per_second, "") : "FPS: collecting...");
  lines.push_back(measured("Frame", stats.mean.frame_ms, " ms (wall time)"));
  for (std::size_t i = 0; i < kProfilePhaseCount; ++i) {
    const auto phase = static_cast<ProfilePhase>(i);
    lines.push_back(measured(std::string(profile_phase_name(phase)), stats.mean.phase_ms[i], kMilliseconds));
  }
}
void add_gpu_lines(std::vector<std::string>& lines, const GpuTimings& gpu) {
  if (!gpu.measured) return;
  for (std::size_t i = 0; i < kGpuPassCount; ++i) {
    const std::string name = kGpuPrefix + std::string(gpu_pass_name(static_cast<GpuPass>(i)));
    lines.push_back(measured(name, gpu.pass_ms[i], kMilliseconds));
  }
}
bool has_samples(const ScriptProfile* scripts) { return scripts != nullptr && scripts->samples() > 0; }
void add_script_lines(std::vector<std::string>& lines, const ScriptProfile* scripts) {
  if (!has_samples(scripts)) return;
  lines.push_back(kScriptsTitle + std::to_string(scripts->samples()) + kScriptsSuffix);
  for (const ScriptMethodStat& method : scripts->top(kOverlayScriptRows))
    lines.push_back(script_method_line(method, scripts->samples()));
}
void line(ScriptContext& ctx, const std::string& text, int row) {
  HudItem item;
  item.text = text;
  item.x = kMargin;
  item.y = kMargin + row * kLineHeight;
  item.size = kTextSize;
  item.color = kTextColor;
  ctx.hud.push_back(std::move(item));
}
void background(ScriptContext& ctx, std::size_t rows) {
  HudItem item;
  item.kind = HudItem::Kind::Rect;
  item.width = kPanelWidth;
  item.height = kMargin * kVerticalMargins + static_cast<int>(rows) * kLineHeight;
  item.color = kPanelColor;
  ctx.hud.push_back(std::move(item));
}
}  // namespace

std::vector<std::string> profile_overlay_lines(const ProfileSummary& stats, const ProfileOverlayExtras& extras) {
  std::vector<std::string> lines;
  add_frame_lines(lines, stats);
  add_gpu_lines(lines, extras.gpu);
  add_script_lines(lines, extras.scripts);
  return lines;
}

bool shows_profile_stats(const ScriptContext& ctx) {
  return ctx.stats_visible && ctx.profiler != nullptr && ctx.profiler->enabled();
}

void append_profile_overlay(ScriptContext& ctx, const ProfileOverlayExtras& extras) {
  if (!shows_profile_stats(ctx)) return;
  const std::vector<std::string> lines = profile_overlay_lines(ctx.profiler->summary(), extras);
  background(ctx, lines.size());
  for (std::size_t row = 0; row < lines.size(); ++row) line(ctx, lines[row], static_cast<int>(row));
}
}  // namespace redgem
