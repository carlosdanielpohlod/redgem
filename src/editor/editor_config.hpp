#pragma once

// Every number of the editor UI, named.

namespace redgem::editor_config {

inline constexpr int kWindowWidth = 1600;
inline constexpr int kWindowHeight = 900;
inline constexpr const char* kWindowTitle = "RedGem Editor";

// Fonts (pixels).
inline constexpr float kInterfaceFontSize = 15.0f;
inline constexpr float kCodeFontSize = 14.0f;
inline constexpr float kTitleFontScale = 1.6f;  // big headings (project hub)
inline constexpr float kTileIconScale = 2.4f;   // icons of the Project panel tiles

// Dock layout (fractions of the window).
inline constexpr float kHierarchyWidth = 0.17f;
inline constexpr float kInspectorWidth = 0.24f;
inline constexpr float kBottomHeight = 0.27f;

inline constexpr const char* kViewportWindow = "Scene";
inline constexpr const char* kHierarchyWindow = "Hierarchy";
inline constexpr const char* kInspectorWindow = "Inspector";
inline constexpr const char* kConsoleWindow = "Console";
inline constexpr const char* kProjectWindow = "Project";

// Bars around the dock space (pixels).
inline constexpr float kToolbarHeight = 42.0f;
inline constexpr float kStatusBarHeight = 26.0f;
inline constexpr float kToolbarGroupGap = 18.0f;
inline constexpr float kIconButtonWidth = 1.3f;  // icon buttons are this many frame heights wide

// Gizmo snapping (hold Ctrl, or toggle it in the toolbar).
inline constexpr float kTranslateSnap = 0.5f;
inline constexpr float kRotateSnapDegrees = 15.0f;
inline constexpr float kScaleSnap = 0.1f;

// New objects appear this far in front of the editor camera (meters).
inline constexpr float kSpawnDistance = 6.0f;

// Viewport overlays.
inline constexpr int kGridSlices = 60;
inline constexpr float kGridSpacing = 1.0f;
inline constexpr float kIconSize = 0.35f;
inline constexpr int kIconRings = 8;
inline constexpr int kIconSlices = 8;
inline constexpr float kSunArrowLength = 2.0f;
inline constexpr float kCameraFrustumLength = 0.8f;
inline constexpr int kMinViewportSize = 16;
inline constexpr float kPlayBorderThickness = 2.0f;
inline constexpr float kOverlayMargin = 10.0f;
inline constexpr float kOverlayPadding = 6.0f;
inline constexpr float kOverlayRounding = 6.0f;
inline constexpr float kOverlayAlpha = 0.8f;

// How often (in frames) the editor checks files changed outside (Blender exports, new assets).
inline constexpr int kReloadCheckFrames = 30;
inline constexpr int kAssetScanFrames = 60;

// Inspector widgets.
inline constexpr float kDragSpeedPosition = 0.05f;
inline constexpr float kDragSpeedRotation = 0.5f;
inline constexpr float kDragSpeedScale = 0.01f;
inline constexpr float kDragSpeedNumber = 0.1f;
inline constexpr float kDragSpeedIntensity = 1.0f;
inline constexpr float kMinScale = 0.001f;
inline constexpr float kMinMass = 0.001f;
inline constexpr float kMinFovDegrees = 5.0f;
inline constexpr float kMaxFovDegrees = 150.0f;
inline constexpr float kMinClipDistance = 0.001f;
inline constexpr int kTextBufferSize = 256;
inline constexpr float kLabelColumnWidth = 104.0f;

// Project panel.
inline constexpr float kFolderColumnWidth = 150.0f;
inline constexpr float kTileWidth = 112.0f;
inline constexpr float kTileHeight = 84.0f;

// Project hub window.
inline constexpr int kHubWidth = 1100;
inline constexpr int kHubHeight = 680;
inline constexpr const char* kHubTitle = "RedGem";
inline constexpr float kHubSidebarWidth = 300.0f;
inline constexpr float kHubCardHeight = 64.0f;
inline constexpr float kHubPadding = 24.0f;

// Folder next to the game where Build puts the standalone game ("<game>-build").
inline constexpr const char* kBuildSuffix = "-build";

// Drag and drop payloads.
inline constexpr const char* kPayloadNode = "REDGEM_NODE";
inline constexpr const char* kPayloadScript = "REDGEM_SCRIPT";
inline constexpr const char* kPayloadModel = "REDGEM_MODEL";

// Viewport colors (RGBA 0-255).
inline constexpr unsigned char kViewportBackground[4] = {28, 30, 36, 255};
inline constexpr unsigned char kSelectionColor[4] = {255, 160, 0, 255};
inline constexpr unsigned char kTriggerColor[4] = {60, 220, 120, 255};
inline constexpr unsigned char kIconColor[4] = {240, 240, 240, 255};
inline constexpr unsigned char kSunColor[4] = {255, 220, 80, 255};

}  // namespace redgem::editor_config
