#pragma once

#include <cstddef>
#include <string>
#include <string_view>

// What a name people type (a studio's, a game's title) is made into to name a folder of its own on
// every system a game is played on.
namespace redgem {

// The longest folder name made, in bytes of UTF-8: well inside what every system's disks allow.
inline constexpr std::size_t kMaxFolderNameBytes = 64;
// What stands in for each character no folder name may hold.
inline constexpr char kFolderNameStandIn = '_';

// The name as one folder's name, the same on every system: letters of any language are kept
// whole, and
//   - what Windows or the Mac forbid in a name (< > : " / \ | ? *), control characters and bytes
//     that are not UTF-8 become '_', so no name is a path to somewhere else;
//   - spaces and dots are taken off both ends ("..", "." and ".hidden" are no folder of their own,
//     and Windows drops what ends a name);
//   - a name Windows keeps for a device (CON, NUL, COM1, LPT1...), with or without an extension,
//     is given a '_' after it;
//   - it is cut to kMaxFolderNameBytes, between whole letters.
// Empty when nothing of the name is left: the caller picks a name of its own.
std::string safe_folder_name(std::string_view name);

}  // namespace redgem
