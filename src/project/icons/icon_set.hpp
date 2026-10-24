#pragma once

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include "project/icons/bytes.hpp"

// A game's icon (game.json "icon": a square PNG, 1024 pixels a side at best) made into what each
// system shows programs by: pictures inside a Windows program (see exe_icon.hpp), a Mac .icns,
// PNGs for Linux and Android.
namespace redgem {

// The icon at several sizes: a side in pixels to a square PNG of that side.
using IconSet = std::map<int, bytes::Bytes>;

// What each system's file holds, smallest first.
inline constexpr std::array<int, 7> kWindowsIconSides = {16, 24, 32, 48, 64, 128, 256};
inline constexpr std::array<int, 7> kMacIconSides = {16, 32, 64, 128, 256, 512, 1024};
inline constexpr int kLinuxIconSide = 256;

// The picture in `png`, scaled to each of `sides`. nullopt, with `error` saying why, for a file
// that is not a PNG that can be decoded or whose picture is not square.
template <typename Sides>
std::optional<IconSet> icon_set_from(const std::filesystem::path& png, const Sides& sides, std::string& error);
std::optional<IconSet> scaled_icons(const std::filesystem::path& png, const int* sides, std::size_t count,
                                    std::string& error);

template <typename Sides>
std::optional<IconSet> icon_set_from(const std::filesystem::path& png, const Sides& sides, std::string& error) {
  return scaled_icons(png, sides.data(), sides.size(), error);
}

// A Mac icon file holding those of `icons` whose side is one of kMacIconSides, as PNGs.
bytes::Bytes icns_file(const IconSet& icons);

}  // namespace redgem
