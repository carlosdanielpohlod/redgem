#include "app/profile_overlay.hpp"

#include <iomanip>
#include <sstream>

#include "core/frame_profiler.hpp"
#include "script/script_context.hpp"

namespace redgem {
namespace {
constexpr int kMargin = 12;
constexpr int kHeaderRows = 2;  // FPS and total frame time precede the phase rows
constexpr int kVerticalMargins = 2;
constexpr int kTextSize = 16;
constexpr int kLineHeight = 22;
constexpr int kPanelWidth = 330;
constexpr int kPrecision = 2;  // milliseconds retain sub-millisecond phase differences
constexpr Color kPanelColor{0.0f, 0.0f, 0.0f, 0.85f};
constexpr Color kTextColor{1.0f, 1.0f, 1.0f, 1.0f};

std::string measured(const std::string& label, double value, const char* unit) {
  std::ostringstream text;
  text << label << ": " << std::fixed << std::setprecision(kPrecision) << value << unit;
  return text.str();
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
void background(ScriptContext& ctx) {
  HudItem item;
  item.kind = HudItem::Kind::Rect;
  item.width = kPanelWidth;
  item.height = kMargin * kVerticalMargins + static_cast<int>(kProfilePhaseCount + kHeaderRows) * kLineHeight;
  item.color = kPanelColor;
  ctx.hud.push_back(std::move(item));
}
}  // namespace

bool shows_profile_stats(const ScriptContext& ctx) {
  return ctx.stats_visible && ctx.profiler != nullptr && ctx.profiler->enabled();
}

void append_profile_overlay(ScriptContext& ctx) {
  if (!shows_profile_stats(ctx)) return;
  const auto stats = ctx.profiler->summary();
  background(ctx);
  line(ctx, stats.mean.has_interval ? measured("FPS", stats.frames_per_second, "") : "FPS: collecting...", 0);
  line(ctx, measured("Frame", stats.mean.frame_ms, " ms (wall time)"), 1);
  for (std::size_t i = 0; i < kProfilePhaseCount; ++i) {
    const auto phase = static_cast<ProfilePhase>(i);
    line(ctx, measured(std::string(profile_phase_name(phase)), stats.mean.phase_ms[i], " ms"),
         static_cast<int>(i) + kHeaderRows);
  }
}
}  // namespace redgem
