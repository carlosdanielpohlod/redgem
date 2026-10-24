#pragma once

#include <string>
#include <vector>

#include "project/icons/bytes.hpp"
#include "project/icons/icon_set.hpp"

// The icon a Windows program carries, which the desktop, the taskbar and the file manager show it
// by. The player for Windows is built with the engine's icon in it (an icon group, as resource
// compilers write one); a build of a game with an icon of its own puts the game's pictures in its
// place, in a section added to the copy of the player. Nothing has to run on Windows for it.
namespace redgem {

// The sides of the pictures of the program's first icon group, smallest first; empty for a file
// that is not a Windows program or carries no icon.
std::vector<int> exe_icon_sides(const bytes::Bytes& exe);

// Gives the program `icons` as its icon: each picture of its first icon group is replaced by the
// one of `icons` of the same side. False, with `error` saying why (not a Windows program, no
// icon in it, no room for one more section, a side `icons` lacks), with `exe` left as it was.
bool replace_exe_icon(bytes::Bytes& exe, const IconSet& icons, std::string& error);

// The pictures of the program's first icon group, by side, as Windows reads them.
IconSet exe_icons(const bytes::Bytes& exe);

}  // namespace redgem
