// The Navigation module's natives: paths through the level, the nearest walkable point, and the
// walkable cells drawn for a frame (ruby/engine/navigation.rb builds the methods games call).

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/hash.h>

#include <cstddef>
#include <optional>
#include <vector>

#include "navigation/navigation.hpp"
#include "render/debug_draw.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr int kDrawArgs = 6;                  // center, radius, red, green, blue, alpha
constexpr float kDrawLift = 0.03f;            // how far above its ground a cell's square is drawn
constexpr float kDrawFill = 0.8f;             // the share of a cell's width its square covers
constexpr std::size_t kMaxDrawnCells = 2048;  // half of a frame's debug shapes

// Whether the scene's grid is still to be baked, by an engine that can.
bool bake_due(const Navigation& navigation, const ScriptContext& game) {
  return !navigation.baked() && static_cast<bool>(game.bake_navigation);
}

// The running scene's navigation, its grid baked first when it is not yet.
const Navigation& navigation(mrb_state* mrb) {
  ScriptContext& game = context(mrb);
  const Navigation& found = required(mrb, game.navigation, "Navigation is only available in a running game");
  if (bake_due(found, game)) game.bake_navigation();
  return found;
}

mrb_value points_value(mrb_state* mrb, const std::vector<Vec3>& points) {
  mrb_value list = mrb_ary_new_capa(mrb, static_cast<mrb_int>(points.size()));
  for (const Vec3 point : points) {
    const int arena = mrb_gc_arena_save(mrb);  // a long path makes many vectors: the list keeps them
    mrb_ary_push(mrb, list, vec3_value(mrb, point));
    mrb_gc_arena_restore(mrb, arena);
  }
  return list;
}

mrb_value report_value(mrb_state* mrb, const NavBakeReport& report) {
  mrb_value result = mrb_hash_new(mrb);
  set_entry(mrb, result, "columns", mrb_int_value(mrb, static_cast<mrb_int>(report.columns)));
  set_entry(mrb, result, "cells", mrb_int_value(mrb, static_cast<mrb_int>(report.cells)));
  set_entry(mrb, result, "cell_size", mrb_float_value(mrb, report.cell_size));
  set_entry(mrb, result, "cells_enlarged", mrb_bool_value(report.cells_enlarged));
  set_entry(mrb, result, "milliseconds", mrb_float_value(mrb, report.milliseconds));
  return result;
}

// __path(from, to): the waypoints, or nil.
mrb_value navigation_path(mrb_state* mrb, mrb_value) {
  mrb_value from, to;
  mrb_get_args(mrb, "oo", &from, &to);
  const Vec3 start = to_vec3(mrb, from);
  const Vec3 goal = to_vec3(mrb, to);
  const std::optional<std::vector<Vec3>> found = navigation(mrb).path(start, goal);
  return found ? points_value(mrb, *found) : mrb_nil_value();
}

// __nearest(point): a Vector3, or nil.
mrb_value navigation_nearest(mrb_state* mrb, mrb_value) {
  const Vec3 point = to_vec3(mrb, single_arg(mrb));
  const std::optional<Vec3> found = navigation(mrb).nearest(point);
  return found ? vec3_value(mrb, *found) : mrb_nil_value();
}

// __walkable(point): true or false.
mrb_value navigation_walkable(mrb_state* mrb, mrb_value) {
  const Vec3 point = to_vec3(mrb, single_arg(mrb));
  return mrb_bool_value(navigation(mrb).walkable(point));
}

// bake: bakes the scene's grid again now (a level a script changed), and says how it went.
mrb_value navigation_bake(mrb_state* mrb, mrb_value) {
  ScriptContext& game = context(mrb);
  const Navigation& found = required(mrb, game.navigation, "Navigation is only available in a running game");
  if (game.bake_navigation) game.bake_navigation();
  return report_value(mrb, found.report());
}

// stats: how the grid in use was baked.
mrb_value navigation_stats(mrb_state* mrb, mrb_value) { return report_value(mrb, navigation(mrb).report()); }

// Draws the walkable cells within a radius of a point, a flat square each, as debug shapes.
class CellPainter {
 public:
  CellPainter(const NavGrid& grid, Vec3 center, float radius, Color color)
      : grid_(grid), center_(center), radius_(radius), color_(color) {}
  std::size_t paint(DebugDrawBuffer& buffer);

 private:
  void paint_row(int z, NavCell low, NavCell high, DebugDrawBuffer& buffer);
  void paint_column(int column, DebugDrawBuffer& buffer);
  void paint_node(NavGrid::Node node, DebugDrawBuffer& buffer);
  bool within_reach(NavGrid::Node node) const { return length(grid_.position(node) - center_) <= radius_; }
  bool full() const { return drawn_ >= kMaxDrawnCells; }
  // Whether a cell is left undrawn: out of reach, or past the most drawn.
  bool skips(NavGrid::Node node) const { return full() || !within_reach(node); }

  const NavGrid& grid_;
  Vec3 center_;
  float radius_;
  Color color_;
  std::size_t drawn_ = 0;
};

void CellPainter::paint_node(NavGrid::Node node, DebugDrawBuffer& buffer) {
  if (skips(node)) return;
  const float side = grid_.shape().cell_size * kDrawFill;
  const DebugDrawItem square{
      DebugDrawItem::Kind::Box, grid_.position(node) + Vec3::up() * kDrawLift, {side, 0.0f, side}, color_, true};
  if (buffer.append(square)) ++drawn_;
}

void CellPainter::paint_column(int column, DebugDrawBuffer& buffer) {
  const auto [first, last] = grid_.nodes_in(column);
  for (NavGrid::Node node = first; node < last; ++node) paint_node(node, buffer);
}

void CellPainter::paint_row(int z, NavCell low, NavCell high, DebugDrawBuffer& buffer) {
  for (int x = low.x; x <= high.x; ++x) {
    const std::optional<int> column = grid_.column_of_cell({x, z});
    if (full()) return;
    if (column) paint_column(*column, buffer);
  }
}

std::size_t CellPainter::paint(DebugDrawBuffer& buffer) {
  if (grid_.empty()) return 0;
  const NavCell low = grid_.nearest_cell(center_.x - radius_, center_.z - radius_);
  const NavCell high = grid_.nearest_cell(center_.x + radius_, center_.z + radius_);
  for (int z = low.z; z <= high.z; ++z) paint_row(z, low, high, buffer);
  return drawn_;
}

// What __draw is given: where, how far around, and in what color (channels 0 to 255).
struct DrawArgs {
  mrb_value center{};
  mrb_float radius = 0.0;
  mrb_int red = 0, green = 0, blue = 0, alpha = 0;
};

Color draw_color(mrb_state* mrb, const DrawArgs& args) {
  return {channel_from(mrb, static_cast<mrb_float>(args.red), "navigation red"),
          channel_from(mrb, static_cast<mrb_float>(args.green), "navigation green"),
          channel_from(mrb, static_cast<mrb_float>(args.blue), "navigation blue"),
          channel_from(mrb, static_cast<mrb_float>(args.alpha), "navigation alpha")};
}

// __draw(center, radius, red, green, blue, alpha): how many cells were drawn.
mrb_value navigation_draw(mrb_state* mrb, mrb_value) {
  DrawArgs args;
  mrb_get_args(mrb, "ofiiii", &args.center, &args.radius, &args.red, &args.green, &args.blue, &args.alpha);
  const Vec3 point = to_vec3(mrb, args.center);
  const auto reach = static_cast<float>(at_least(mrb, args.radius, 0.0, "Navigation.draw's radius"));
  const Color color = draw_color(mrb, args);
  DebugDrawBuffer* buffer = context(mrb).debug_draw;
  if (buffer == nullptr) return mrb_int_value(mrb, 0);
  const std::size_t drawn = CellPainter(navigation(mrb).grid(), point, reach, color).paint(*buffer);
  return mrb_int_value(mrb, static_cast<mrb_int>(drawn));
}

}  // namespace

void install_navigation_bindings(mrb_state* mrb) {
  RClass* module = mrb_define_module(mrb, "Navigation");
  mrb_define_module_function(mrb, module, "__path", navigation_path, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, module, "__nearest", navigation_nearest, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, module, "__walkable", navigation_walkable, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, module, "__draw", navigation_draw, MRB_ARGS_REQ(kDrawArgs));
  mrb_define_module_function(mrb, module, "bake", navigation_bake, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, module, "stats", navigation_stats, MRB_ARGS_NONE());
}

}  // namespace redgem
