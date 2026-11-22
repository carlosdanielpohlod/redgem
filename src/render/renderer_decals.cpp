// Drawing decals: the marks scripts lay on the scene's surfaces (scene/decals.hpp), drawn over the
// opaque objects they lie on and before anything see-through. Each darkens what is under it (its
// color multiplies it), so a mark takes the light and shade of its surface without being lit.

#include "render/culling.hpp"
#include "render/renderer_impl.hpp"

namespace redgem {
namespace {

using namespace render_detail;

constexpr int kSpotSide = 64;          // texels a side of the soft round spot
constexpr float kSpotFalloff = 0.6f;   // the share of its radius marked wholly, before it fades out
constexpr float kPictureTop = 1.0f;    // a picture's rows go down from its top; a decal's go up
constexpr ::Color kSpotMiddle{255, 255, 255, 255};  // marks wholly, in the decal's color
constexpr ::Color kSpotEdge{255, 255, 255, 0};      // marks nothing

// Whether an object the frame draws is shown at the viewer's distance from it.
bool shown_from(const ObjectSight& sight, Vec3 viewer) {
  const Vec3 center = sight.sight.sphere.center;
  return sight.drawn && !outside_distance_range(center, viewer, sight.visible_from, sight.visible_to);
}

// A decal's piece is on an object still there, drawn this frame and at the distance it is shown
// from (of several versions of a model, only the one shown keeps its marks).
bool on_a_drawn_object(const std::vector<ObjectSight>& sights, ObjectId id, Vec3 viewer) {
  if (id == kNoObject || static_cast<std::size_t>(id) >= sights.size()) return false;
  return shown_from(sights[static_cast<std::size_t>(id)], viewer);
}

// Darkens what is drawn: what the shader gives multiplies it. The picture's alpha is left alone.
void blend_by_multiplying() {
  rlSetBlendFactorsSeparate(RL_DST_COLOR, RL_ZERO, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
  rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);
}

// (The picture is given after rlBegin: a batch that turns to triangles starts on the blank one.)
void send_corners(const std::vector<DecalCorner>& corners, const Texture2D& picture, const redgem::Color& color) {
  rlBegin(RL_TRIANGLES);
  rlSetTexture(picture.id);
  rlColor4f(color.r, color.g, color.b, color.a);
  for (const DecalCorner& corner : corners) {
    rlTexCoord2f(corner.u, kPictureTop - corner.v);
    rlVertex3f(corner.position.x, corner.position.y, corner.position.z);
  }
  rlEnd();
}

}  // namespace

void Renderer::Impl::gather_decals(const Scene& scene, Vec3 viewer) {
  frame_decals.clear();
  if (decals == nullptr) return;
  for (const Decal& decal : decals->decals()) gather_decal(scene, decal, viewer);
}

void Renderer::Impl::gather_decal(const Scene& scene, const Decal& decal, Vec3 viewer) {
  FrameDecal drawn{decal_picture(decal.image), decal.color, {}};
  for (const DecalPiece& piece : decal.pieces) {
    const ObjectId host = scene.resolve(piece.host);
    if (!on_a_drawn_object(sights, host, viewer)) continue;
    const std::vector<DecalCorner> corners = world_corners(piece, worlds[static_cast<std::size_t>(host)]);
    drawn.corners.insert(drawn.corners.end(), corners.begin(), corners.end());
  }
  if (!drawn.corners.empty()) frame_decals.push_back(std::move(drawn));
}

// Its picture from the game's images folder; the soft spot for none, or one the folder lacks.
const Texture2D* Renderer::Impl::decal_picture(const std::string& image) {
  const Texture2D* picture = image.empty() ? nullptr : ui_image(image);
  if (picture != nullptr) return picture;
  if (decal_spot.id == 0) {
    const Image spot = GenImageGradientRadial(kSpotSide, kSpotSide, kSpotFalloff, kSpotMiddle, kSpotEdge);
    decal_spot = LoadTextureFromImage(spot);
    UnloadImage(spot);
  }
  return &decal_spot;
}

void Renderer::Impl::draw_decals() {
  if (frame_decals.empty()) return;
  begin_decals();
  for (const FrameDecal& decal : frame_decals) send_corners(decal.corners, *decal.picture, decal.color);
  end_decals();
}

void Renderer::Impl::begin_decals() {
  rlDrawRenderBatchActive();
  BeginShaderMode(decal_shader);
  rlDisableDepthMask();
  rlDisableBackfaceCulling();
  set_polygon_offset(true);
  blend_by_multiplying();
}

void Renderer::Impl::end_decals() {
  rlDrawRenderBatchActive();
  rlSetTexture(0);
  rlSetBlendMode(BLEND_ALPHA);
  set_polygon_offset(false);
  rlEnableBackfaceCulling();
  rlEnableDepthMask();
  EndShaderMode();
}

void Renderer::Impl::release_decals() {
  frame_decals.clear();
  if (decal_spot.id != 0) UnloadTexture(decal_spot);
  decal_spot = Texture2D{};
}

}  // namespace redgem
