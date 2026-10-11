// Particles natives.

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/string.h>

#include "scene/particles.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

// --- Particles ---------------------------------------------------------------------------------

ParticleSystem& particles(mrb_state* mrb) {
  ParticleSystem* system = context(mrb).particles;
  if (system == nullptr) mrb_raise(mrb, E_RUNTIME_ERROR, "Particles are only available while a game runs");
  return *system;
}

// What Particles.__burst is called with, as mruby hands it over.
struct BurstArgs {
  mrb_value position = mrb_nil_value();
  mrb_value direction = mrb_nil_value();
  mrb_int count = 0, r = 0, g = 0, b = 0, a = 0;
  mrb_float speed = 0.0, spread = 0.0, life = 0.0, size = 0.0, gravity = 0.0, spin = 0.0;
  mrb_bool glow = false;
  const char* image = nullptr;
};

BurstArgs burst_args(mrb_state* mrb) {
  BurstArgs args;
  mrb_get_args(mrb, "ooifffffiiiibzf", &args.position, &args.direction, &args.count, &args.speed, &args.spread,
               &args.life, &args.size, &args.gravity, &args.r, &args.g, &args.b, &args.a, &args.glow, &args.image,
               &args.spin);
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
}

// What each particle looks like.
void set_look(const BurstArgs& args, ParticleBurst& burst) {
  burst.size = static_cast<float>(args.size);
  burst.color = color_from(args.r, args.g, args.b, args.a);
  burst.glow = args.glow;
  burst.image = args.image;
  burst.spin_degrees = static_cast<float>(args.spin);
}

mrb_value particles_burst(mrb_state* mrb, mrb_value) {
  const BurstArgs args = burst_args(mrb);
  ParticleBurst burst;
  set_motion(mrb, args, burst);
  set_look(args, burst);
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
  mrb_define_module_function(mrb, particles_module, "__burst", particles_burst, MRB_ARGS_REQ(15));
  mrb_define_module_function(mrb, particles_module, "count", particles_count, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, particles_module, "clear", particles_clear, MRB_ARGS_NONE());
}

}  // namespace redgem
