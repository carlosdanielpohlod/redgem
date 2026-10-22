#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include "scene/scene.hpp"

namespace redgem {

inline constexpr std::size_t kMaxWaterSurfaceObjects = 128;  // cache keys and geometry share this candidate budget
inline constexpr std::size_t kMaxWaterSurfaces = 128;        // geometry work per fixed step is bounded
inline constexpr std::size_t kMaxWaterTriangles = 262144;
inline constexpr float kWaterMaxCoordinate = 1000000.0f;  // reject unbounded geometry before squared-distance math
inline constexpr float kWaterPlaneTolerance = 0.01f;      // meters: supported surfaces are horizontal and planar

inline constexpr std::size_t kWaterTriangleCorners = 3;  // vertices in each footprint triangle

struct WaterTriangle {
  std::array<Vec3, kWaterTriangleCorners> points;
};
struct WaterBvhNode {
  Bounds bounds;
  std::size_t first = 0;
  std::size_t count = 0;
  std::size_t left = 0;
  std::size_t right = 0;
};

struct WaterSurface {
  ObjectHandle object;
  std::size_t primitive = 0;
  Bounds bounds;
  std::vector<WaterTriangle> triangles;
  std::vector<WaterBvhNode> nodes;
};

struct WaterSweep {
  Vec3 point;
  float along = 0.0f;
};

bool finite_water_point(Vec3 point);
Bounds water_world_bounds(const Bounds& local, const Transform& world);
std::vector<WaterSurface> water_surfaces(const Scene& scene, const SceneDesc& desc);
std::optional<Vec3> water_contact_point(const WaterSurface& surface, const Bounds& body);
std::optional<WaterSweep> water_swept_contact(const WaterSurface& surface, const Bounds& before, const Bounds& after);

// Runtime geometry changes are uncommon; moving bodies do not rebuild the water's spatial index.
class WaterGeometryCache {
 public:
  const std::vector<WaterSurface>& refresh(const Scene& scene, const SceneDesc& desc);
  void clear() {
    keys_.clear();
    surfaces_.clear();
  }

 private:
  struct Key {
    ObjectHandle object;
    int mesh;
    Transform world;
  };
  static std::vector<Key> collect_keys(const Scene& scene, const SceneDesc& desc);
  std::vector<Key> keys_;
  std::vector<WaterSurface> surfaces_;
};

}  // namespace redgem
