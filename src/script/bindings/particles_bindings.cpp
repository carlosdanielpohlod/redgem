// Particles natives.

#include <mruby.h>

#include "scene/particles.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/bindings/particle_look_args.hpp"

namespace redgem {
namespace {

using namespace binding;

// --- Particles ---------------------------------------------------------------------------------

ParticleSystem& particles(mrb_state* mrb) {
  return required(mrb, context(mrb).particles, "Particles are only available while a game runs");
}

// What Particles.__burst is called with, as mruby hands it over.
struct BurstArgs {
  mrb_value position = mrb_nil_value();
  mrb_value direction = mrb_nil_value();
  mrb_int count = 0;
  mrb_float speed = 0.0, spread = 0.0, life = 0.0, gravity = 0.0, spin = 0.0;
  mrb_value sizes = mrb_nil_value();  // each kind of look: an Array of what it goes through
  mrb_value colors = mrb_nil_value();
  mrb_value alphas = mrb_nil_value();
  mrb_bool glow = false, lit = false;
  mrb_float soft = 0.0;
  const char* image = nullptr;
  mrb_int columns = 1, rows = 1;  // of a sprite sheet
  mrb_float fps = 0.0;
};

BurstArgs burst_args(mrb_state* mrb) {
  BurstArgs args;
  mrb_get_args(mrb, "ooifffffAAAbbfziif", &args.position, &args.direction, &args.count, &args.speed, &args.spread,
               &args.life, &args.gravity, &args.spin, &args.sizes, &args.colors, &args.alphas, &args.glow, &args.lit,
               &args.soft, &args.image, &args.columns, &args.rows, &args.fps);
  for (mrb_float* number : {&args.speed, &args.spread, &args.gravity, &args.spin}) *number = finite(mrb, *number);
  args.life = at_least(mrb, args.life, 0.0, "a burst's life");
  return args;
}

// Where the particles go and how they move.
void set_motion(mrb_state* mrb, const BurstArgs& args, ParticleBurst& burst) {
  burst.position = to_vec3(mrb, args.position);
  burst.direction = to_vec3(mrb, args.direction);
  burst.count = static_cast<int>(args.count);
  burst.speed = static_cast<float>(args.speed);
  burst.spread_degrees = static_cast<float>(args.spread);
  burst.life = static_cast<float>(args.life);
  burst.gravity = static_cast<float>(args.gravity);
  burst.spin_degrees = static_cast<float>(args.spin);
}

// What each particle looks like.
void set_look(mrb_state* mrb, const BurstArgs& args, ParticleBurst& burst) {
  burst.look.size = sizes_over_life(mrb, args.sizes);
  burst.look.color = colors_over_life(mrb, args.colors);
  burst.look.alpha = alphas_over_life(mrb, args.alphas);
  burst.look.glow = args.glow;
  burst.look.lit = args.lit;
  burst.look.soft = static_cast<float>(at_least(mrb, args.soft, 0.0, "a burst's soft edge"));
  burst.look.flipbook = flipbook_of(mrb, args.columns, args.rows, args.fps);
  burst.image = args.image;
}

mrb_value particles_burst(mrb_state* mrb, mrb_value) {
  const BurstArgs args = burst_args(mrb);
  ParticleBurst burst;
  set_motion(mrb, args, burst);
  set_look(mrb, args, burst);
  return mrb_int_value(mrb, particles(mrb).burst(burst));
}

mrb_value particles_count(mrb_state* mrb, mrb_value) {
  return mrb_int_value(mrb, static_cast<mrb_int>(particles(mrb).particles().size()));
}

mrb_value particles_clear(mrb_state* mrb, mrb_value) {
  particles(mrb).clear();
  return mrb_nil_value();
}

}  // namespace

void install_particles_bindings(mrb_state* mrb) {
  RClass* particles_module = mrb_define_module(mrb, "Particles");
  mrb_define_module_function(mrb, particles_module, "__burst", particles_burst, MRB_ARGS_REQ(18));
  mrb_define_module_function(mrb, particles_module, "count", particles_count, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, particles_module, "clear", particles_clear, MRB_ARGS_NONE());
}

}  // namespace redgem
