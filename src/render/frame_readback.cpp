#include "render/frame_readback.hpp"

#include <rlgl.h>

#if defined(GRAPHICS_API_OPENGL_ES3)
#include <GLES3/gl3.h>
#else
#include "external/glad.h"  // raylib's loader: its functions are loaded once raylib opens a window
#endif

namespace redgem {
namespace {

constexpr int kBytesPerPixel = 3;  // red, green, blue
constexpr int kTightRows = 1;      // rows packed with no padding

}  // namespace

FrameReadback::FrameReadback(int width, int height)
    : width_(width),
      height_(height),
      bytes_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * kBytesPerPixel) {
  glGenBuffers(static_cast<GLsizei>(kBuffers), buffers_.data());
  for (const unsigned int buffer : buffers_) {
    glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer);
    glBufferData(GL_PIXEL_PACK_BUFFER, static_cast<GLsizeiptr>(bytes_), nullptr, GL_STREAM_READ);
  }
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

FrameReadback::~FrameReadback() { glDeleteBuffers(static_cast<GLsizei>(kBuffers), buffers_.data()); }

void FrameReadback::read(unsigned int framebuffer, const Consumer& consumer) {
  if (pending_ == kBuffers) {
    hand_over(next_, consumer);  // the oldest frame is in the buffer the next one goes to
    --pending_;
  }
  rlBindFramebuffer(RL_READ_FRAMEBUFFER, framebuffer);
  glPixelStorei(GL_PACK_ALIGNMENT, kTightRows);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, buffers_[next_]);
  glReadPixels(0, 0, width_, height_, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  rlDisableFramebuffer();
  next_ = (next_ + 1) % kBuffers;
  ++pending_;
}

void FrameReadback::drain(const Consumer& consumer) {
  for (; pending_ > 0; --pending_) hand_over((next_ + kBuffers - pending_) % kBuffers, consumer);
}

void FrameReadback::hand_over(std::size_t buffer, const Consumer& consumer) {
  glBindBuffer(GL_PIXEL_PACK_BUFFER, buffers_[buffer]);
  const void* pixels = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, static_cast<GLsizeiptr>(bytes_), GL_MAP_READ_BIT);
  if (pixels != nullptr) consumer(static_cast<const std::uint8_t*>(pixels), bytes_);
  glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

}  // namespace redgem
