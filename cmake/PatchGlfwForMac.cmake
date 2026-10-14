# Script mode, run in raylib's sources once they are fetched (on a Mac only): lets the library raylib
# opens windows with (GLFW) open one on a Mac that has no graphics card OpenGL can use, such as a
# virtual machine. GLFW asks the Mac for a hardware-accelerated OpenGL and gives up without one;
# patched, it then asks for any, which is Apple's software renderer. Where there is a card, nothing
# changes: the first answer is the card's. Running it twice changes nothing.
#   cmake -P PatchGlfwForMac.cmake      (in raylib's source folder)
set(context_file "src/external/glfw/src/nsgl_context.m")
# What GLFW does: the first of its attributes (attribs[0]) asks for a hardware-accelerated renderer.
set(asked
"    window->context.nsgl.pixelFormat =
        [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];
")
set(or_any
"${asked}    // (RedGem) No accelerated renderer: any, without the first attribute.
    if (window->context.nsgl.pixelFormat == nil)
        window->context.nsgl.pixelFormat =
            [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs + 1];
")

file(READ "${context_file}" code)
string(FIND "${code}" "${or_any}" patched)
if(patched EQUAL -1)
  string(FIND "${code}" "${asked}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${context_file} is not the GLFW this patch was written for: look at it again")
  endif()
  string(REPLACE "${asked}" "${or_any}" code "${code}")
  file(WRITE "${context_file}" "${code}")
endif()
