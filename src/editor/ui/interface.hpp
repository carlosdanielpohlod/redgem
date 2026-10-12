#pragma once

#include "render/window.hpp"

namespace redgem::ui {

// The interface drawn in a window with the editor's fonts and theme: set up once the window is
// open, shut down before it closes. One at a time, like the window it is in.
class Interface {
 public:
  // Whether its windows can be docked into one another (the editor's panels) or stay where
  // they are put (a form).
  enum class Layout { Fixed, Dockable };
  // `window` is the open window it is drawn in: asked for so that there is one.
  Interface(const Window& window, Layout layout);
  ~Interface();
  Interface(const Interface&) = delete;
  Interface& operator=(const Interface&) = delete;

  // Everything of the interface drawn in a frame goes between these two, inside the window's frame.
  void begin_frame();
  void end_frame();
};

}  // namespace redgem::ui
