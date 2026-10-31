#pragma once

#include <imgui.h>

// The editor's colors: neutral slate grays with a ruby accent.
namespace redgem::ui::palette {

inline constexpr ImVec4 kText{0.90f, 0.91f, 0.93f, 1.0f};
inline constexpr ImVec4 kTextDisabled{0.49f, 0.51f, 0.56f, 1.0f};
inline constexpr ImVec4 kWindow{0.106f, 0.114f, 0.133f, 1.0f};
inline constexpr ImVec4 kPanelDark{0.082f, 0.090f, 0.106f, 1.0f};
inline constexpr ImVec4 kPopup{0.133f, 0.145f, 0.169f, 0.98f};
inline constexpr ImVec4 kBorder{0.180f, 0.192f, 0.220f, 1.0f};
inline constexpr ImVec4 kFrame{0.165f, 0.176f, 0.204f, 1.0f};
inline constexpr ImVec4 kFrameHover{0.200f, 0.216f, 0.247f, 1.0f};
inline constexpr ImVec4 kFrameActive{0.231f, 0.247f, 0.282f, 1.0f};
inline constexpr ImVec4 kHeader{0.169f, 0.180f, 0.212f, 1.0f};
inline constexpr ImVec4 kHeaderHover{0.216f, 0.231f, 0.271f, 1.0f};
inline constexpr ImVec4 kAccent{0.878f, 0.239f, 0.302f, 1.0f};
inline constexpr ImVec4 kAccentHover{0.937f, 0.341f, 0.400f, 1.0f};
inline constexpr ImVec4 kAccentSoft{0.878f, 0.239f, 0.302f, 0.35f};
inline constexpr ImVec4 kAccentFaint{0.878f, 0.239f, 0.302f, 0.18f};
inline constexpr ImVec4 kScrollbar{0.231f, 0.247f, 0.282f, 1.0f};
inline constexpr ImVec4 kTransparent{0.0f, 0.0f, 0.0f, 0.0f};
inline constexpr ImVec4 kModalDim{0.0f, 0.0f, 0.0f, 0.55f};
inline constexpr ImVec4 kMuted{0.58f, 0.60f, 0.66f, 1.0f};
inline constexpr ImVec4 kError{1.0f, 0.42f, 0.42f, 1.0f};
inline constexpr ImVec4 kWarning{1.0f, 0.78f, 0.32f, 1.0f};
inline constexpr ImVec4 kSuccess{0.40f, 0.85f, 0.55f, 1.0f};
inline constexpr ImVec4 kSky{0.45f, 0.75f, 0.98f, 1.0f};
inline constexpr ImVec4 kLavender{0.73f, 0.62f, 0.98f, 1.0f};
inline constexpr ImVec4 kTeal{0.38f, 0.84f, 0.80f, 1.0f};
inline constexpr ImVec4 kOrange{0.98f, 0.62f, 0.38f, 1.0f};

// Ruby code in the script tabs, by what each word is.
inline constexpr ImVec4 kCodeKeyword = kAccentHover;
inline constexpr ImVec4 kCodeString = kSuccess;
inline constexpr ImVec4 kCodeInterpolation = kAccent;
inline constexpr ImVec4 kCodeSymbol = kSky;
inline constexpr ImVec4 kCodeRegex = kTeal;
inline constexpr ImVec4 kCodeComment = kTextDisabled;
inline constexpr ImVec4 kCodeNumber = kOrange;
inline constexpr ImVec4 kCodeConstant = kWarning;
inline constexpr ImVec4 kCodeVariable = kLavender;

}  // namespace redgem::ui::palette
