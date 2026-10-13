#include "physics/body_config.hpp"

#include <optional>

#include "physics/physics_config.hpp"
#include "scene/props.hpp"

namespace redgem {
namespace {

constexpr const char* kNoBody = "none";  // said in messages only
constexpr const char* kUnknownBody = "unknown";

// What the `body` property calls each type. An object without the property has none; "none"
// itself cannot be asked for.
struct BodyTypeName {
  BodyType type;
  const char* name;
};
constexpr BodyTypeName kBodyTypeNames[] = {
    {BodyType::None, kNoBody},
    {BodyType::Static, props::kBodyStatic},
    {BodyType::Dynamic, props::kBodyDynamic},
    {BodyType::Kinematic, props::kBodyKinematic},
    {BodyType::Character, props::kBodyCharacter},
    {BodyType::Vehicle, props::kBodyVehicle},
};

struct ColliderName {
  ColliderShape shape;
  const char* name;
};
constexpr ColliderName kColliderNames[] = {
    {ColliderShape::Box, props::kColliderBox},
    {ColliderShape::Sphere, props::kColliderSphere},
    {ColliderShape::Capsule, props::kColliderCapsule},
    {ColliderShape::Mesh, props::kColliderMesh},
};

// ("none" is what an object without the property has: it cannot be asked for.)
bool asked_for_by(const BodyTypeName& known, const std::string& name) {
  return known.type != BodyType::None && name == known.name;
}

std::optional<BodyType> parse_body_type(const std::string& name) {
  for (const BodyTypeName& known : kBodyTypeNames) {
    if (asked_for_by(known, name)) return known.type;
  }
  return std::nullopt;
}

std::optional<ColliderShape> parse_collider(const std::string& name) {
  for (const ColliderName& known : kColliderNames) {
    if (name == known.name) return known.shape;
  }
  return std::nullopt;
}

void read_body_type(const PropertyMap& props, BodyConfig& config) {
  const std::optional<std::string> body = get_string(props, props::kBody);
  if (!body) return;
  const std::optional<BodyType> parsed = parse_body_type(*body);
  if (!parsed) config.warnings.push_back("unknown body type '" + *body + "'");
  config.type = parsed.value_or(config.type);
}

// A character with no collider asked for gets its capsule here.
void read_collider(const PropertyMap& props, BodyConfig& config) {
  const std::optional<std::string> collider = get_string(props, props::kCollider);
  if (!collider) {
    if (config.type == BodyType::Character) config.collider = ColliderShape::Capsule;
    return;
  }
  const std::optional<ColliderShape> parsed = parse_collider(*collider);
  if (!parsed) {
    config.warnings.push_back("unknown collider '" + *collider + "'");
    return;
  }
  config.collider = *parsed;
  if (!config.wants_physics()) config.type = BodyType::Static;
}

// A body that can change place: anything but a static one (or none at all).
bool moves(BodyType type) { return type != BodyType::None && type != BodyType::Static; }

// A trigger is a volume, not a body: whatever body type was asked for is dropped.
void apply_trigger(BodyConfig& config) {
  if (!config.trigger) return;
  if (moves(config.type)) {
    config.warnings.push_back(std::string("trigger ignores body type '") + body_type_name(config.type) + "'");
  }
  config.type = BodyType::None;
}

void use_collider(BodyConfig& config, ColliderShape collider, const char* reason) {
  config.warnings.push_back(reason);
  config.collider = collider;
}

bool is_moving_mesh(const BodyConfig& config) { return config.collider == ColliderShape::Mesh && moves(config.type); }

// Whether a body of `type` was given another collider than the only one it works with.
bool has_wrong_collider(const BodyConfig& config, BodyType type, ColliderShape only) {
  return config.type == type && config.collider != only;
}

// Some body types only work with some colliders.
void settle_collider(BodyConfig& config) {
  if (is_moving_mesh(config)) {
    use_collider(config, ColliderShape::Box, "mesh collider is only supported on static bodies, using box");
  }
  if (has_wrong_collider(config, BodyType::Character, ColliderShape::Capsule)) {
    use_collider(config, ColliderShape::Capsule, "characters always use a capsule collider");
  }
  if (has_wrong_collider(config, BodyType::Vehicle, ColliderShape::Box)) {
    use_collider(config, ColliderShape::Box, "vehicles always use a box chassis");
  }
}

void read_mass(const PropertyMap& props, BodyConfig& config) {
  const float default_mass = config.type == BodyType::Vehicle ? config::kVehicleDefaultMass : config::kDefaultBodyMass;
  const std::optional<double> mass = get_number(props, props::kMass);
  config.mass = mass ? static_cast<float>(*mass) : default_mass;
  if (config.mass <= 0.0f) {
    config.warnings.push_back("mass must be positive, using " + std::to_string(default_mass));
    config.mass = default_mass;
  }
}

}  // namespace

bool is_dynamic(BodyType type) { return type == BodyType::Dynamic || type == BodyType::Vehicle; }

const char* body_type_name(BodyType type) {
  for (const BodyTypeName& known : kBodyTypeNames) {
    if (known.type == type) return known.name;
  }
  return kUnknownBody;
}

Bounds default_collider_bounds() {
  const Vec3 half = Vec3::one() * config::kDefaultColliderHalfExtent;
  Bounds bounds;
  bounds.include(-half);
  bounds.include(half);
  return bounds;
}

BodyConfig body_config_from_props(const PropertyMap& props) {
  BodyConfig config;
  config.trigger = get_bool(props, props::kTrigger).value_or(false);
  read_body_type(props, config);
  read_collider(props, config);
  apply_trigger(config);
  settle_collider(config);
  read_mass(props, config);
  return config;
}

}  // namespace redgem
