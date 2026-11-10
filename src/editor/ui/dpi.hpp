#pragma once

namespace redgem::ui {
// Retina uses logical coordinates with a denser framebuffer. Windows/X11 use screen pixels.
bool native_dpi_window();
float interface_scale(float monitor_scale, bool logical_coordinates);
// Rebuild spacing from its baseline; fonts remain owned by the dynamic font atlas.
void apply_interface_scale(float scale);
float scaled(float pixels);
}  // namespace redgem::ui
