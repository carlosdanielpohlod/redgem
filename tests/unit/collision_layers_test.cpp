#include <doctest/doctest.h>

#include <string>

#include <nlohmann/json.hpp>

#include "physics/body_config.hpp"
#include "physics/collision_layers.hpp"

using namespace redgem;

namespace {

constexpr std::size_t kPlayer = 1;
constexpr std::size_t kEnemy = 2;
constexpr std::size_t kBullet = 3;
constexpr std::size_t kPickup = 4;
constexpr int kTooManyLayers = 20;         // past kMaxCollisionLayers
constexpr std::size_t kOddEntries = 2;     // the number and the empty name; the repeated one is no harm

CollisionLayersReading read(const std::string& physics) {
  return collision_layers_from_json(nlohmann::json::parse(physics));
}

constexpr const char* kShooter = R"({
  "layers": ["default", "player", "enemy", "bullet", "pickup"],
  "collides": {"bullet": ["enemy", "default"]}
})";

bool mentions(const CollisionLayersReading& reading, const std::string& text) {
  for (const std::string& warning : reading.warnings) {
    if (warning.find(text) != std::string::npos) return true;
  }
  return false;
}

}  // namespace

TEST_CASE("a game without physics layers has only the default one, which collides with itself") {
  const CollisionLayers layers;
  CHECK(layers.count() == 1);
  CHECK(layers.name(kDefaultLayer) == kDefaultLayerName);
  CHECK(layers.collide(kDefaultLayer, kDefaultLayer));
  CHECK(collision_layers_from_json(nlohmann::json()).warnings.empty());
}

TEST_CASE("game.json names the layers, and a layer's list says what it collides with, both ways") {
  const CollisionLayersReading reading = read(kShooter);
  REQUIRE(reading.warnings.empty());
  const CollisionLayers& layers = reading.layers;
  REQUIRE(layers.count() == 5);
  CHECK(layers.find("bullet") == kBullet);
  CHECK(layers.collide(kBullet, kEnemy));
  CHECK(layers.collide(kEnemy, kBullet));
  CHECK(layers.collide(kBullet, kDefaultLayer));
  CHECK_FALSE(layers.collide(kBullet, kPickup));
  CHECK_FALSE(layers.collide(kPickup, kBullet));
  CHECK_FALSE(layers.collide(kBullet, kBullet));
  CHECK_FALSE(layers.collide(kPlayer, kBullet));
  CHECK(layers.collide(kPlayer, kPickup));  // layers without a list meet every layer that lets them in
  CHECK(layers.names_listed() == "default, player, enemy, bullet, pickup");
}

TEST_CASE("the default layer is always the first, even when the list leaves it out or names it later") {
  CHECK(read(R"({"layers": ["player", "enemy"]})").layers.names_listed() == "default, player, enemy");
  CHECK(read(R"({"layers": ["player", "default"]})").layers.names_listed() == "default, player");
}

TEST_CASE("layers past sixteen, repeated names and names that are not text are left out") {
  nlohmann::json many = nlohmann::json::array();
  for (int i = 0; i < kTooManyLayers; ++i) many.push_back("layer" + std::to_string(i));
  const CollisionLayersReading crowded = collision_layers_from_json({{"layers", many}});
  CHECK(crowded.layers.count() == kMaxCollisionLayers);
  CHECK(mentions(crowded, "at most 16 layers"));

  const CollisionLayersReading odd = read(R"({"layers": ["enemy", "enemy", 3, ""]})");
  CHECK(odd.layers.names_listed() == "default, enemy");
  CHECK(odd.warnings.size() == kOddEntries);
}

TEST_CASE("collides naming no layer is warned of, and the rest of it still counts") {
  const CollisionLayersReading reading =
      read(R"({"layers": ["bullet", "enemy"], "collides": {"bullet": ["enemy", "ghost"], "ghost": []}})");
  CHECK(mentions(reading, "\"ghost\", which is no layer"));
  const std::size_t bullet = *reading.layers.find("bullet");
  const std::size_t enemy = *reading.layers.find("enemy");
  CHECK(reading.layers.collide(bullet, enemy));
  CHECK_FALSE(reading.layers.collide(bullet, kDefaultLayer));
}

TEST_CASE("a physics section of the wrong shape is warned of and gives the default layer") {
  CHECK(mentions(read(R"("bullets")"), "must be an object"));
  CHECK(mentions(read(R"({"layers": "bullet"})"), "list of names"));
  CHECK(mentions(read(R"({"collides": ["bullet"]})"), "must map layers"));
  CHECK(mentions(read(R"({"collides": {"default": "default"}})"), "must be a list of layers"));
  CHECK(read(R"({"layers": "bullet"})").layers.count() == 1);
}

TEST_CASE("an object's layer property names its collision layer") {
  CHECK(body_config_from_props({{"body", std::string("dynamic")}}).layer == kDefaultLayerName);
  CHECK(body_config_from_props({{"body", std::string("dynamic")}, {"layer", std::string("enemy")}}).layer ==
        "enemy");
}
