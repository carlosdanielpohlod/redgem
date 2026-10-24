// Particles natives.

#include <mruby.h>

#include <array>
#include <limits>

#include "scene/particle_emitters.hpp"

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

ParticleEmitters& emitters(mrb_state* mrb) {
  return required(mrb, context(mrb).particle_emitters, "Particle emitters are only available while a game runs");
}

// What Particles.__burst or __emitter is called with, as mruby hands it over.
struct BurstArgs {
  mrb_value object = mrb_nil_value();
  mrb_float rate = 0.0;
  mrb_value position = mrb_nil_value();
  mrb_value direction = mrb_nil_value();
  mrb_int count = 0;
  mrb_float speed = 0.0, spread = 0.0, life = 0.0, gravity = 0.0, spin = 0.0;
  mrb_value sizes = mrb_nil_value();  // each kind of look: an Array of what it goes through
  mrb_value colors = mrb_nil_value();
  mrb_value alphas = mrb_nil_value();
  mrb_bool glow = false, lit = false, cast_shadow = false;
  mrb_float soft = 0.0;
  const char* image = nullptr;
  mrb_int columns = 1, rows = 1;  // of a sprite sheet
  mrb_float fps = 0.0;
};

constexpr std::size_t kEmitterPrefixArgs = 2;  // attached object and particles per second
constexpr std::size_t kBurstArgs = 19;
constexpr std::size_t kEmitterArgs = kBurstArgs + kEmitterPrefixArgs;
constexpr const char* kBurstFormat = "ooifffffAAAbbfziifb";
constexpr const char* kEmitterFormat = "ofooifffffAAAbbfziifb";

BurstArgs burst_args(mrb_state* mrb, bool emitter = false) {
  BurstArgs args;
  void* image_pointer = static_cast<void*>(&args.image);
  std::array<void*, kEmitterArgs> pointers{
      &args.object, &args.rate,    &args.position, &args.direction, &args.count,  &args.speed,  &args.spread,
      &args.life,   &args.gravity, &args.spin,     &args.sizes,     &args.colors, &args.alphas, &args.glow,
      &args.lit,    &args.soft,    image_pointer,  &args.columns,   &args.rows,   &args.fps,    &args.cast_shadow};
  mrb_get_args_a(mrb, emitter ? kEmitterFormat : kBurstFormat, pointers.data() + (emitter ? 0 : kEmitterPrefixArgs));
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
  burst.look.cast_shadow = args.cast_shadow;
  burst.look.soft = static_cast<float>(at_least(mrb, args.soft, 0.0, "a burst's soft edge"));
  burst.look.flipbook = flipbook_of(mrb, args.columns, args.rows, args.fps);
  burst.image = args.image;
}

ParticleBurst settings(mrb_state* mrb, const BurstArgs& args) {
  ParticleBurst burst;
  set_motion(mrb, args, burst);
  set_look(mrb, args, burst);
  return burst;
}

mrb_value particles_burst(mrb_state* mrb, mrb_value) {
  return mrb_int_value(mrb, particles(mrb).burst(settings(mrb, burst_args(mrb))));
}

mrb_value particles_emitter(mrb_state* mrb, mrb_value) {
  const BurstArgs args = burst_args(mrb, true);
  const float rate = static_cast<float>(from_to(mrb, args.rate, 0.0, kMaxEmitterRate, "an emitter's rate"));
  more_than(mrb, rate, 0.0, "an emitter's rate");
  alive_id(mrb, args.object);
  const int id = emitters(mrb).create(scene(mrb), wrapped_handle(mrb, args.object), settings(mrb, args), rate);
  if (id == kNoEmitter) mrb_raise(mrb, E_RUNTIME_ERROR, "no room for another particle emitter");
  return mrb_int_value(mrb, id);
}

int emitter_id_arg(mrb_state* mrb) {
  mrb_int id = 0;
  mrb_get_args(mrb, "i", &id);
  if (id <= kNoEmitter) return kNoEmitter;
  if (id > std::numeric_limits<int>::max()) return kNoEmitter;
  return static_cast<int>(id);
}

mrb_value stop_emitter(mrb_state* mrb, mrb_value) { return mrb_bool_value(emitters(mrb).stop(emitter_id_arg(mrb))); }

mrb_value emitter_active(mrb_state* mrb, mrb_value) {
  return mrb_bool_value(emitters(mrb).active(scene(mrb), emitter_id_arg(mrb)));
}

mrb_value particles_count(mrb_state* mrb, mrb_value) {
  return mrb_int_value(mrb, static_cast<mrb_int>(particles(mrb).particles().size()));
}

mrb_value particles_clear(mrb_state* mrb, mrb_value) {
  particles(mrb).clear();
  if (context(mrb).particle_emitters != nullptr) context(mrb).particle_emitters->clear();
  return mrb_nil_value();
}

}  // namespace

void install_particles_bindings(mrb_state* mrb) {
  RClass* particles_module = mrb_define_module(mrb, "Particles");
  mrb_define_module_function(mrb, particles_module, "__burst", particles_burst, MRB_ARGS_REQ(kBurstArgs));
  mrb_define_module_function(mrb, particles_module, "__emitter", particles_emitter, MRB_ARGS_REQ(kEmitterArgs));
  mrb_define_module_function(mrb, particles_module, "__stop_emitter", stop_emitter, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, particles_module, "__emitter_active", emitter_active, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, particles_module, "count", particles_count, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, particles_module, "clear", particles_clear, MRB_ARGS_NONE());
}

}  // namespace redgem
