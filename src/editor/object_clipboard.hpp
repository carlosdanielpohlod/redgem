#pragma once

// Copied objects as text on the system's clipboard, so that they paste into another editor window
// too: a line that says what the text is, then the .glb the objects were copied as (see
// SceneDocument::copy_nodes) in base64.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace redgem::object_clipboard {

// The most bytes of copied objects put on the system's clipboard as text; bigger copies stay in
// the editor's own clipboard (they paste into the window they were copied in).
inline constexpr std::size_t kMaxClipboardBytes = std::size_t{64} * 1024 * 1024;  // 64 MiB

// Whether copied objects this big go on the system's clipboard.
bool fits_system_clipboard(std::size_t bytes);

// The clipboard's text for objects copied as `glb`.
std::string to_text(const std::vector<std::uint8_t>& glb);
// The objects a clipboard's text holds; nullopt for any other text.
std::optional<std::vector<std::uint8_t>> from_text(const std::string& text);

// Base64 (RFC 4648, with padding), and back; nullopt for text that is not base64.
std::string base64_encode(const std::vector<std::uint8_t>& bytes);
std::optional<std::vector<std::uint8_t>> base64_decode(const std::string& text);

}  // namespace redgem::object_clipboard
