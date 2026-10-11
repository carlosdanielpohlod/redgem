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

mrb_value particles_burst(mrb_state* mrb, mrb_value) {
  mrb_value position, direction;
  mrb_int count = 0, r = 0, g = 0, b = 0, a = 0;
  mrb_float speed = 0.0, spread = 0.0, life = 0.0, size = 0.0, gravity = 0.0;
  mrb_bool glow = false;
  const char* image = nullptr;
  mrb_float spin = 0.0;
  mrb_get_args(mrb, "ooifffffiiiibzf", &position, &direction, &count, &speed, &spread, &life, &size, &gravity, &r, &g,
               &b, &a, &glow, &image, &spin);
  ParticleBurst burst;
  burst.position = to_vec3(mrb, position);
  burst.direction = to_vec3(mrb, direction);
  burst.count = static_cast<int>(count);
  burst.speed = static_cast<float>(speed);
  burst.spread_degrees = static_cast<float>(spread);
  burst.life = static_cast<float>(life);
  burst.size = static_cast<float>(size);
  burst.gravity = static_cast<float>(gravity);
  burst.color = color_from(r, g, b, a);
  burst.glow = glow;
  burst.image = image;
  burst.spin_degrees = static_cast<float>(spin);
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
