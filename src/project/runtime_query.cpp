#include "project/runtime_query.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "physics/body_config.hpp"
#include "project/scene_lookup.hpp"
#include "project/scene_text.hpp"

namespace redgem::tools {
namespace {

// Below this, a vector reads as zero and a scale as one: what a list rounds away anyway.
constexpr float kShownTolerance = 0.005f;
// Shortest ray between two points: closer points are one point, with no direction to cast in.
constexpr float kShortestRay = 1e-4f;
constexpr char kComponentSeparator = ',';

// "#7 Coin1" for an object of the loaded scene; "Coin1 (spawned)" for one made while playing.
std::string object_label(Engine& engine, const SceneDesc& scene, ObjectId id) {
  const std::string& name = engine.scene().at(id).name;
  if (static_cast<std::size_t>(id) >= scene.nodes.size()) return name + " (spawned)";
  return object_id(scene, static_cast<int>(id)) + " " + name;
}

// An object the request names: by the scene's #id, path or name, else by a spawned object's name.
ObjectId requested_object(Engine& engine, const SceneDesc& scene, const std::string& reference) {
  const FoundObject found = find_object(scene, reference);
  if (found.index != kNoIndex) return static_cast<ObjectId>(found.index);
  const ObjectId spawned = engine.scene().find(reference);
  if (spawned == kNoObject) throw std::invalid_argument(found.error);
  return spawned;
}

bool shown_vector(Vec3 value) { return !approx_equal(value, Vec3::zero(), kShownTolerance); }

bool moving_body(BodyType type) {
  return type == BodyType::Dynamic || type == BodyType::Kinematic || type == BodyType::Character ||
         type == BodyType::Vehicle;
}

// " vel=0,-0.2,0 grounded": how a body that moves is moving.
std::string motion_text(const PhysicsWorld& physics, ObjectId id, BodyType type) {
  if (!moving_body(type)) return {};
  const Vec3 velocity = physics.velocity(id);
  std::string text = shown_vector(velocity) ? " vel=" + vector_text(velocity, kListDecimals) : std::string();
  if (type == BodyType::Character) text += physics.character_grounded(id) ? " grounded" : " airborne";
  return text;
}

// " body=character vel=0,-0.2,0 grounded": what the physics world says about the object.
std::string body_text(const PhysicsWorld& physics, ObjectId id) {
  if (!physics.has_object(id)) return {};
  const BodyType type = physics.body_type(id);
  std::string text = type == BodyType::None ? std::string() : std::string(" body=") + body_type_name(type);
  if (physics.is_trigger(id)) text += " trigger";
  text += motion_text(physics, id, type);
  if (physics.sleeping(id)) text += " sleeping";
  return text;
}

// " pos=0,0.8,0 rot=0,90,0": where the object is in the world now.
std::string pose_text(const Scene& scene, ObjectId id) {
  const Transform world = scene.world_transform(id);
  std::string text = " pos=" + vector_text(world.position, kListDecimals);
  const Vec3 degrees = quat_to_euler_degrees(world.rotation);
  if (shown_vector(degrees)) text += " rot=" + vector_text(degrees, kListAngleDecimals);
  if (!approx_equal(world.scale, Vec3::one(), kShownTolerance))
    text += " scale=" + vector_text(world.scale, kListDecimals);
  return text;
}

bool unchanged(const PropertyMap* loaded, const std::string& key, const PropertyValue& value) {
  if (loaded == nullptr) return false;
  const auto found = loaded->find(key);
  return found != loaded->end() && found->second == value;
}

// " hidden speed=2": what scripts changed besides where it is. Props are listed only when they
// differ from the loaded scene's (scene_get says the rest); a spawned object lists them all.
std::string state_text(const GameObject& object, const PropertyMap* loaded) {
  std::string text = object.visible ? "" : " hidden";
  for (const auto& [key, value] : object.props) {
    if (!unchanged(loaded, key, value)) text += " " + key + "=" + property_text(value);
  }
  return text;
}

// The props the object had when the scene loaded; none for an object spawned since.
const PropertyMap* loaded_props(const SceneDesc& scene, ObjectId id) {
  if (static_cast<std::size_t>(id) >= scene.nodes.size()) return nullptr;
  return &scene.nodes[static_cast<std::size_t>(id)].props;
}

std::string inspect(Engine& engine, const SceneDesc& scene, const mcp::Json& args) {
  if (!args.contains("object")) throw std::invalid_argument("inspect needs object (a #id, path or name)");
  const ObjectId id = requested_object(engine, scene, args["object"].get<std::string>());
  const std::string label = object_label(engine, scene, id);
  if (!engine.scene().is_alive(id)) return label + " destroyed";
  return label + pose_text(engine.scene(), id) + body_text(engine.physics(), id) +
         state_text(engine.scene().at(id), loaded_props(scene, id));
}

// One line per hit, nearest first, the rest counted.
std::string hits_text(Engine& engine, const SceneDesc& scene, const std::vector<RaycastHit>& hits) {
  std::string text;
  const std::size_t shown = std::min(hits.size(), kMostQueryHits);
  for (std::size_t i = 0; i < shown; ++i) {
    const RaycastHit& hit = hits[i];
    text += object_label(engine, scene, hit.object) + " at " + vector_text(hit.point, kListDecimals) +
            " normal=" + vector_text(hit.normal, kListDecimals) + " dist=" + number_text(hit.distance, kListDecimals) +
            "\n";
  }
  if (hits.size() > kMostQueryHits) text += "+" + std::to_string(hits.size() - kMostQueryHits) + " more\n";
  return text;
}

std::string raycast(Engine& engine, const SceneDesc& scene, const mcp::Json& args) {
  const Vec3 from = point_argument(args, "from");
  const Vec3 to = point_argument(args, "to");
  const float distance = length(to - from);
  if (distance < kShortestRay) throw std::invalid_argument("from and to must be different points");
  const auto hits = engine.physics().raycast_all(from, normalize(to - from), distance, QueryFilter{});
  if (hits.empty()) return "nothing between from and to";
  return std::to_string(hits.size()) + " hits, nearest first:\n" + hits_text(engine, scene, hits);
}

std::string overlap(Engine& engine, const SceneDesc& scene, const mcp::Json& args) {
  const Vec3 center = point_argument(args, "at");
  if (!args.contains("radius")) throw std::invalid_argument("overlap needs radius (meters)");
  QueryShape sphere;
  sphere.radius = args["radius"].get<float>();
  const auto hits = engine.physics().overlap(sphere, center, QueryFilter{});
  if (hits.empty()) return "nothing within the sphere";
  return std::to_string(hits.size()) + " objects, nearest first:\n" + hits_text(engine, scene, hits);
}

// Reads the next component of "x,y,z" into `value`; false when it is no finite number.
bool read_component(std::istringstream& in, float& value) {
  std::string part;
  if (!std::getline(in, part, kComponentSeparator)) return false;
  std::istringstream number(part);
  return static_cast<bool>(number >> value) && number.eof() && std::isfinite(value);
}

}  // namespace

Vec3 point_argument(const mcp::Json& args, const char* key) {
  if (!args.contains(key)) throw std::invalid_argument(std::string("missing argument: ") + key + " (x,y,z in meters)");
  std::istringstream in(args[key].get<std::string>());
  Vec3 point;
  const bool read = read_component(in, point.x) && read_component(in, point.y) && read_component(in, point.z);
  if (!read || !in.eof())
    throw std::invalid_argument(std::string(key) + ": needs x,y,z (three numbers), like 0,1.5,-4");
  return point;
}

std::string answer_query(Engine& engine, const SceneDesc& scene, const mcp::Json& args) {
  const std::string action = args.at("action");
  if (action == kInspectAction) return inspect(engine, scene, args);
  if (action == kRaycastAction) return raycast(engine, scene, args);
  if (action == kOverlapAction) return overlap(engine, scene, args);
  throw std::invalid_argument("action must be inspect, raycast or overlap");
}

}  // namespace redgem::tools
