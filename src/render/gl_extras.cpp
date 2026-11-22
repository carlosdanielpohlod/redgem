#include "render/gl_extras.hpp"

#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

#if defined(GRAPHICS_API_OPENGL_ES3)
#include <GLES3/gl3.h>
#else
#include "external/glad.h"  // raylib's loader: its functions are loaded once raylib opens a window
#endif

#include "core/log.hpp"

namespace redgem::render_detail {
namespace {

constexpr GLenum kDepthFormat = GL_DEPTH_COMPONENT24;  // of every depth the scene's pictures share
// The longest a frame is waited for: past it the card is taken to be stuck, and drawing goes on.
constexpr int kLongestFenceWaitSeconds = 10;
constexpr GLuint64 kNanosecondsPerSecond = 1000000000;
// How much nearer a polygon offset draws: by its slope, and by the depth buffer's least steps.
constexpr float kNearerBySlope = -1.0f;
constexpr float kNearerByUnits = -1.0f;

// What drivers that tell of the card's memory answer to (NVX_gpu_memory_info, ATI_meminfo).
constexpr GLenum kNvxTotalKilobytes = 0x9048;
constexpr GLenum kNvxAvailableKilobytes = 0x9049;
constexpr GLenum kAtiTextureFreeKilobytes = 0x87FC;
constexpr std::size_t kAtiMeminfoValues = 4;  // free, largest free block, and the same for shared memory

// Whether the card has said it ran out of memory: remembered from the first time, for good.
bool& out_of_memory_seen() {
  static bool seen = false;
  return seen;
}

// The next error the card left, remembering an out-of-memory one.
GLenum next_error() {
  const GLenum error = glGetError();
  if (error == GL_OUT_OF_MEMORY) out_of_memory_seen() = true;
  return error;
}

unsigned int render_buffer(int width, int height, int samples, GLenum format) {
  unsigned int buffer = 0;
  glGenRenderbuffers(1, &buffer);
  glBindRenderbuffer(GL_RENDERBUFFER, buffer);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, format, width, height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  return buffer;
}

}  // namespace

CardFence insert_card_fence() { return glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0); }

void wait_card_fence(CardFence fence) {
  if (fence == nullptr) return;
  const auto sync = static_cast<GLsync>(fence);
  // (Flushed first: a mark the driver still holds back would never be passed.)
  if (glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, kLongestFenceWaitSeconds * kNanosecondsPerSecond) ==
      GL_TIMEOUT_EXPIRED)
    log::warning("the graphics card has not finished a frame in " + std::to_string(kLongestFenceWaitSeconds) +
                 " seconds: going on without it");
}

void release_card_fence(CardFence fence) {
  if (fence != nullptr) glDeleteSync(static_cast<GLsync>(fence));
}

void forget_errors() {
  while (next_error() != GL_NO_ERROR) {
  }
}

bool no_new_error() { return next_error() == GL_NO_ERROR; }

bool card_ran_out_of_memory() {
  forget_errors();
  return out_of_memory_seen();
}

CardMemoryKilobytes card_memory_kilobytes() {
  forget_errors();
  GLint total = 0;
  GLint available = 0;
  glGetIntegerv(kNvxTotalKilobytes, &total);
  glGetIntegerv(kNvxAvailableKilobytes, &available);
  if (no_new_error() && total > 0) return {total, available};
  forget_errors();
  std::array<GLint, kAtiMeminfoValues> ati{};
  glGetIntegerv(kAtiTextureFreeKilobytes, ati.data());
  if (no_new_error()) return {0, ati[0]};
  forget_errors();
  return {};
}

int largest_target_side() {
  GLint texture = 0;
  GLint buffer = 0;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &texture);
  glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &buffer);
  return std::min(texture, buffer);
}

Multisampled load_multisampled(int width, int height, int samples) {
  Multisampled target{rlLoadFramebuffer(), 0, 0, width, height, samples};
  target.color = render_buffer(width, height, samples, GL_RGBA16F);
  target.depth = render_buffer(width, height, samples, kDepthFormat);
  rlFramebufferAttach(target.framebuffer, target.color, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_RENDERBUFFER, 0);
  rlFramebufferAttach(target.framebuffer, target.depth, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
  if (rlFramebufferComplete(target.framebuffer)) return target;
  release(target);
  return Multisampled{0, 0, 0, width, height, samples};  // what was asked, so that it is not asked again
}

void release(Multisampled& target) {
  glDeleteRenderbuffers(1, &target.color);
  // (rlgl lets the depth buffer go with its framebuffer.)
  if (target.framebuffer != 0) rlUnloadFramebuffer(target.framebuffer);
  target = Multisampled{};
}

bool resolve(const Multisampled& target, unsigned int framebuffer) {
  forget_errors();
  rlBindFramebuffer(RL_READ_FRAMEBUFFER, target.framebuffer);
  rlBindFramebuffer(RL_DRAW_FRAMEBUFFER, framebuffer);
  rlBlitFramebuffer(0, 0, target.width, target.height, 0, 0, target.width, target.height,
                    GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  rlDisableFramebuffer();
  return no_new_error();
}

unsigned int depth_texture(int width, int height) {
  unsigned int texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, kDepthFormat, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);
  return texture;
}

void copy_buffer(unsigned int from, unsigned int to, int width, int height, Buffer which) {
  rlBindFramebuffer(RL_READ_FRAMEBUFFER, from);
  rlBindFramebuffer(RL_DRAW_FRAMEBUFFER, to);
  const int mask = which == Buffer::Color ? GL_COLOR_BUFFER_BIT : GL_DEPTH_BUFFER_BIT;
  rlBlitFramebuffer(0, 0, width, height, 0, 0, width, height, mask);
}

void set_alpha_to_coverage(bool on) {
  if (on) {
    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
  } else {
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
  }
}

void set_clockwise_front_faces(bool clockwise) { glFrontFace(clockwise ? GL_CW : GL_CCW); }

void set_polygon_offset(bool on) {
  if (!on) {
    glDisable(GL_POLYGON_OFFSET_FILL);
    return;
  }
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(kNearerBySlope, kNearerByUnits);
}

void set_depth_range(float nearest, float farthest) {
#if defined(GRAPHICS_API_OPENGL_ES3)
  glDepthRangef(nearest, farthest);
#else
  glDepthRange(nearest, farthest);
#endif
}

void filter_cube_maps_across_seams() {
#if !defined(GRAPHICS_API_OPENGL_ES3)
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif
}

#if defined(GRAPHICS_API_OPENGL_ES3)

bool has_timer_queries() { return false; }
unsigned int make_timer_query() { return 0; }
void release_timer_query(unsigned int) {}
void begin_timer(unsigned int) {}
void end_timer() {}
bool timer_ready(unsigned int) { return false; }
std::uint64_t timer_nanoseconds(unsigned int) { return 0; }

#else

// (Loaded by raylib with the rest of OpenGL: a driver without them leaves them null.)
bool has_timer_queries() {
  const bool queries = glad_glGenQueries != nullptr && glad_glBeginQuery != nullptr && glad_glEndQuery != nullptr;
  return queries && glad_glGetQueryObjectiv != nullptr && glad_glGetQueryObjectui64v != nullptr;
}

unsigned int make_timer_query() {
  unsigned int query = 0;
  glGenQueries(1, &query);
  return query;
}

void release_timer_query(unsigned int query) {
  if (query != 0) glDeleteQueries(1, &query);
}

void begin_timer(unsigned int query) { glBeginQuery(GL_TIME_ELAPSED, query); }

void end_timer() { glEndQuery(GL_TIME_ELAPSED); }

bool timer_ready(unsigned int query) {
  GLint available = GL_FALSE;
  glGetQueryObjectiv(query, GL_QUERY_RESULT_AVAILABLE, &available);
  return available != GL_FALSE;
}

std::uint64_t timer_nanoseconds(unsigned int query) {
  GLuint64 elapsed = 0;
  glGetQueryObjectui64v(query, GL_QUERY_RESULT, &elapsed);
  return elapsed;
}

#endif

}  // namespace redgem::render_detail
