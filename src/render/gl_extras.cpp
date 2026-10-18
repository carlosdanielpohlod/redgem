#include "render/gl_extras.hpp"

#include <rlgl.h>

#if defined(GRAPHICS_API_OPENGL_ES3)
#include <GLES3/gl3.h>
#else
#include "external/glad.h"  // raylib's loader: its functions are loaded once raylib opens a window
#endif

namespace redgem::render_detail {
namespace {

constexpr GLenum kDepthFormat = GL_DEPTH_COMPONENT24;  // of every depth the scene's pictures share

unsigned int render_buffer(int width, int height, int samples, GLenum format) {
  unsigned int buffer = 0;
  glGenRenderbuffers(1, &buffer);
  glBindRenderbuffer(GL_RENDERBUFFER, buffer);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, format, width, height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  return buffer;
}

// Lets go of the errors earlier calls left, so that the next check is of a call's own.
void forget_errors() {
  while (glGetError() != GL_NO_ERROR) {
  }
}

}  // namespace

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
  return glGetError() == GL_NO_ERROR;
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

void filter_cube_maps_across_seams() {
#if !defined(GRAPHICS_API_OPENGL_ES3)
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif
}

}  // namespace redgem::render_detail
