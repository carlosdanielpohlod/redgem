#pragma once

// The few graphics calls raylib's rlgl does not offer, the same on desktop OpenGL 3.3 and on
// OpenGL ES 3 (whose headers this part alone includes): a picture drawn with several samples per
// pixel, depth textures of the same format as its depth, a picture's colors copied, alpha to
// coverage, and cube maps filtered across their faces' seams.

namespace redgem::render_detail {

// A framebuffer of several samples per pixel: color (half floats, as the scene's picture) and
// depth, as render buffers. Its framebuffer is 0 when the graphics card cannot make it (its size
// and samples still say what was asked).
struct Multisampled {
  unsigned int framebuffer = 0;
  unsigned int color = 0;
  unsigned int depth = 0;
  int width = 0;
  int height = 0;
  int samples = 0;
};
Multisampled load_multisampled(int width, int height, int samples);
void release(Multisampled& target);
// Averages its samples into the color and depth of `framebuffer` (of the same size, its depth a
// depth_texture). False when the graphics card would not.
bool resolve(const Multisampled& target, unsigned int framebuffer);

// A depth texture of 24 bits, as a Multisampled's depth is: depth is only copied between the two
// when their formats are the same.
unsigned int depth_texture(int width, int height);

// What of a framebuffer is copied.
enum class Buffer { Color, Depth };
// Copies `which` of framebuffer `from` into `to`, both `width` x `height`, texel for texel (no
// filtering: float texels come out the same; several samples a pixel are resolved). The
// framebuffers stay bound.
void copy_buffer(unsigned int from, unsigned int to, int width, int height, Buffer which);

// While on, a fragment's alpha says how many of its pixel's samples it covers.
void set_alpha_to_coverage(bool on);

// Cube maps filtered across the seams between their faces (always so on OpenGL ES 3).
void filter_cube_maps_across_seams();

}  // namespace redgem::render_detail
