#include "physics/body_config.hpp"

#include <optional>

#include "core/config.hpp"

namespace redgem {
namespace {

// What the `body` property calls each type. An object without the property has none; "none"
// itself cannot be asked for.
struct BodyTypeName {
  BodyType type;
  const char* name;
};
constexpr BodyTypeName kBodyTypeNames[] = {
    {BodyType::None, "none"},           {BodyType::Static, "static"},       {BodyType::Dynamic, "dynamic"},
    {BodyType::Kinematic, "kinematic"}, {BodyType::Character, "character"}, {BodyType::Vehicle, "vehicle"},
};

struct ColliderName {
  ColliderShape shape;
  const char* name;
};
constexpr ColliderName kColliderNames[] = {
    {ColliderShape::Box, config::kColliderBox},
    {ColliderShape::Sphere, config::kColliderSphere},
    {ColliderShape::Capsule, config::kColliderCapsule},
    {ColliderShape::Mesh, config::kColliderMesh},
};

std::optional<BodyType> parse_body_type(const std::string& name) {
  for (const BodyTypeName& known : kBodyTypeNames) {
    if (known.type != BodyType::None && name == known.name) return known.type;
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
  const std::optional<std::string> body = get_string(props, config::kPropBody);
  if (!body) return;
  const std::optional<BodyType> parsed = parse_body_type(*body);
  if (!parsed) config.warnings.push_back("unknown body type '" + *body + "'");
  config.type = parsed.value_or(config.type);
}

// A character with no collider asked for gets its capsule here.
void read_collider(const PropertyMap& props, BodyConfig& config) {
  const std::optional<std::string> collider = get_string(props, config::kPropCollider);
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
  if (config.type == BodyType::None && !config.trigger) config.type = BodyType::Static;
}

// A trigger is a volume, not a body: whatever body type was asked for is dropped.
void apply_trigger(BodyConfig& config) {
  if (!config.trigger) return;
  if (config.type != BodyType::None && config.type != BodyType::Static) {
    config.warnings.push_back(std::string("trigger ignores body type '") + body_type_name(config.type) + "'");
  }
  config.type = BodyType::None;
}

void use_collider(BodyConfig& config, ColliderShape collider, const char* reason) {
  config.warnings.push_back(reason);
  config.collider = collider;
}

// Some body types only work with some colliders.
void settle_collider(BodyConfig& config) {
  const bool moving_mesh =
      config.collider == ColliderShape::Mesh && config.type != BodyType::Static && config.type != BodyType::None;
  if (moving_mesh)
    use_collider(config, ColliderShape::Box, "mesh collider is only supported on static bodies, using box");
  if (config.type == BodyType::Character && config.collider != ColliderShape::Capsule) {
    use_collider(config, ColliderShape::Capsule, "characters always use a capsule collider");
  }
  if (config.type == BodyType::Vehicle && config.collider != ColliderShape::Box) {
    use_collider(config, ColliderShape::Box, "vehicles always use a box chassis");
  }
}

void read_mass(const PropertyMap& props, BodyConfig& config) {
  const float default_mass = config.type == BodyType::Vehicle ? config::kVehicleDefaultMass : config::kDefaultBodyMass;
  const std::optional<double> mass = get_number(props, config::kPropMass);
  config.mass = mass ? static_cast<float>(*mass) : default_mass;
  if (config.mass <= 0.0f) {
    config.warnings.push_back("mass must be positive, using " + std::to_string(default_mass));
    config.mass = default_mass;
  }
}

}  // namespace

const char* body_type_name(BodyType type) {
  for (const BodyTypeName& known : kBodyTypeNames) {
    if (known.type == type) return known.name;
  }
  return "unknown";
}

BodyConfig body_config_from_props(const PropertyMap& props) {
  BodyConfig config;
  config.trigger = get_bool(props, config::kPropTrigger).value_or(false);
  read_body_type(props, config);
  read_collider(props, config);
  apply_trigger(config);
  settle_collider(config);
  read_mass(props, config);
  return config;
}

}  // namespace redgem
