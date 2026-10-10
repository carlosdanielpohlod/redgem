#include "physics/body_config.hpp"

#include <optional>

#include "core/config.hpp"

namespace redgem {
namespace {

std::optional<BodyType> parse_body_type(const std::string& name) {
  if (name == "static") return BodyType::Static;
  if (name == "dynamic") return BodyType::Dynamic;
  if (name == "kinematic") return BodyType::Kinematic;
  if (name == "character") return BodyType::Character;
  if (name == "vehicle") return BodyType::Vehicle;
  return std::nullopt;
}

std::optional<ColliderShape> parse_collider(const std::string& name) {
  if (name == "box") return ColliderShape::Box;
  if (name == "sphere") return ColliderShape::Sphere;
  if (name == "capsule") return ColliderShape::Capsule;
  if (name == "mesh") return ColliderShape::Mesh;
  return std::nullopt;
}

}  // namespace

const char* body_type_name(BodyType type) {
  switch (type) {
    case BodyType::None: return "none";
    case BodyType::Static: return "static";
    case BodyType::Dynamic: return "dynamic";
    case BodyType::Kinematic: return "kinematic";
    case BodyType::Character: return "character";
    case BodyType::Vehicle: return "vehicle";
  }
  return "unknown";
}

BodyConfig body_config_from_props(const PropertyMap& props) {
  BodyConfig config;
  const std::optional<std::string> body = get_string(props, config::kPropBody);
  const std::optional<std::string> collider = get_string(props, config::kPropCollider);
  config.trigger = get_bool(props, config::kPropTrigger).value_or(false);
  const std::optional<double> mass = get_number(props, config::kPropMass);

  if (body) {
    if (const auto parsed = parse_body_type(*body)) {
      config.type = *parsed;
    } else {
      config.warnings.push_back("unknown body type '" + *body + "'");
    }
  }
  if (collider) {
    if (const auto parsed = parse_collider(*collider)) {
      config.collider = *parsed;
      if (config.type == BodyType::None && !config.trigger) config.type = BodyType::Static;
    } else {
      config.warnings.push_back("unknown collider '" + *collider + "'");
    }
  } else if (config.type == BodyType::Character) {
    config.collider = ColliderShape::Capsule;
  }

  if (config.trigger && config.type != BodyType::None && config.type != BodyType::Static) {
    config.warnings.push_back(std::string("trigger ignores body type '") + body_type_name(config.type) + "'");
  }
  if (config.trigger) config.type = BodyType::None;
  if (config.collider == ColliderShape::Mesh && config.type != BodyType::Static && config.type != BodyType::None) {
    config.warnings.push_back("mesh collider is only supported on static bodies, using box");
    config.collider = ColliderShape::Box;
  }
  if (config.type == BodyType::Character && config.collider != ColliderShape::Capsule) {
    config.warnings.push_back("characters always use a capsule collider");
    config.collider = ColliderShape::Capsule;
  }
  if (config.type == BodyType::Vehicle && config.collider != ColliderShape::Box) {
    config.warnings.push_back("vehicles always use a box chassis");
    config.collider = ColliderShape::Box;
  }

  const float default_mass = config.type == BodyType::Vehicle ? config::kVehicleDefaultMass : config::kDefaultBodyMass;
  config.mass = mass ? static_cast<float>(*mass) : default_mass;
  if (config.mass <= 0.0f) {
    config.warnings.push_back("mass must be positive, using " + std::to_string(default_mass));
    config.mass = default_mass;
  }
  return config;
}

}  // namespace redgem
