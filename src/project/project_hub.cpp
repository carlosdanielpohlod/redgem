#include "project/project_hub.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <raylib.h>
#include <rlImGui.h>

#include <algorithm>

#include "app/game_settings.hpp"
#include "core/files.hpp"
#include "project/game_templates.hpp"
#include "project/recent_games.hpp"
#include "core/platform.hpp"
#include "core/config.hpp"
#include "core/log.hpp"
#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "editor/ui/style.hpp"
#include "render/gpu_select.hpp"

namespace redgem {
namespace {

using namespace editor_config;

constexpr int kHubFps = 60;  // frames per second: the hub only redraws a form
constexpr const char* kDefaultTemplate = "platformer";
constexpr const char* kEmptyTemplate = "empty";
constexpr const char* kDefaultGameName = "My Game";
constexpr float kTemplateCardHeight = 58.0f;  // pixels
constexpr float kOpenButtonWidth = 90.0f;     // pixels
constexpr float kCardTextLines = 2.0f;        // title and detail
constexpr float kCardIconColumn = 2.0f;       // icon column width, in icon buttons
constexpr float kOpenFooterRows = 2.0f;       // "Open a game folder" heading and its path field
constexpr int kWindowStyleVars = 2;           // padding and rounding of the hub's only window
constexpr int kButtonColors = 2;              // normal and hovered color of the Create button
constexpr const char* kHubWindowId = "##hub";
constexpr const char* kNewGameId = "new";
constexpr const char* kGamesId = "games";
constexpr const char* kGameListId = "list";
constexpr const char* kCardId = "##card";
constexpr const char* kNameId = "##name";
constexpr const char* kPathId = "##path";

const char* template_icon(const std::string& name) {
  return name == kEmptyTemplate ? ICON_FA_CUBE : ICON_FA_PERSON_RUNNING;
}

std::string pretty_path(const std::filesystem::path& path) {
  return editor_logic::pretty_path(path.string(), platform::home_dir().string());
}

// What a card shows, over a selectable that starts at `start`: the icon, and next to it the two
// lines (with the gap between them) centered in the card's height.
void draw_card_text(ImVec2 start, float height, const char* icon, const std::string& title, const std::string& detail) {
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImGuiStyle& style = ImGui::GetStyle();
  const float line = ImGui::GetTextLineHeight();
  const float text_top = start.y + editor_logic::centered_start(height - line * kCardTextLines, style.ItemSpacing.y);
  const float icon_width = ImGui::GetFontSize() * kIconButtonWidth * kCardIconColumn;
  draw->AddText(ImVec2(start.x + style.FramePadding.x, start.y + editor_logic::centered_start(height, line)),
                ImGui::GetColorU32(ui::accent_color()), icon);
  draw->AddText(ImVec2(start.x + icon_width, text_top), ImGui::GetColorU32(ImGuiCol_Text), title.c_str());
  draw->AddText(ImVec2(start.x + icon_width, text_top + line + style.ItemSpacing.y),
                ImGui::GetColorU32(ui::muted_color()), detail.c_str());
}

// What a card is: its id, and what it shows.
struct CardLabel {
  const char* id;
  const char* icon;
  const std::string& title;
  const std::string& detail;  // a muted second line
};

// A full-width selectable card with an icon, a title and a muted second line.
bool card(const CardLabel& label, bool selected, float height) {
  ImGui::PushID(label.id);
  const ImVec2 start = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::Selectable(kCardId, selected, ImGuiSelectableFlags_AllowDoubleClick,
                                         ImVec2(ImGui::GetContentRegionAvail().x, height));
  draw_card_text(start, height, label.icon, label.title, label.detail);
  ImGui::PopID();
  return clicked;
}

// The hub's only window: it fills the whole of the system's window, without decorations.
void begin_hub_window() {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kHubPadding, kHubPadding));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::Begin(kHubWindowId, nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::PopStyleVar(kWindowStyleVars);
}

// The button that stands out: in the accent color.
bool accent_button(const char* label, float width) {
  ImGui::PushStyleColor(ImGuiCol_Button, ui::accent_color());
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ui::accent_color());
  const bool pressed = ImGui::Button(label, ImVec2(width, 0.0f));
  ImGui::PopStyleColor(kButtonColors);
  return pressed;
}

}  // namespace

ProjectHub::ProjectHub(Options options) : options_(std::move(options)), template_(kDefaultTemplate) {
  prefer_discrete_gpu(options_.gpu);
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
  InitWindow(kHubWidth, kHubHeight, kHubTitle);
  SetExitKey(KEY_NULL);  // Esc leaves text fields; only the close button quits
  SetTargetFPS(kHubFps);
  rlImGuiSetLoadFontsCallback(ui::load_fonts);
  rlImGuiSetup(true);
  ui::apply_theme();
  ImGui::GetIO().IniFilename = nullptr;
  new_name_ = kDefaultGameName;
  refresh_games();
}

ProjectHub::~ProjectHub() {
  rlImGuiShutdown();
  CloseWindow();
}

void ProjectHub::refresh_games() {
  games_ = read_recent_games(options_.recent_file);
  for (const auto& game : find_games(options_.games_folder)) {
    const auto same = [&game](const std::filesystem::path& known) {
      std::error_code error;
      return std::filesystem::equivalent(known, game, error);
    };
    if (std::none_of(games_.begin(), games_.end(), same)) games_.push_back(game);
  }
  titles_.clear();
  for (const auto& game : games_) titles_.push_back(read_game_settings(game).title);
}

std::optional<std::filesystem::path> ProjectHub::run() {
  if (!IsWindowReady()) {
    log::error("cannot open a window for the project hub");
    return std::nullopt;
  }
  while (!chosen_ && !WindowShouldClose()) frame();
  return chosen_;
}

bool ProjectHub::create_game(const std::string& name, const std::string& template_name) {
  if (!valid_game_name(name)) {
    error_ = "Use letters, digits, spaces, - and _ for the name.";
    return false;
  }
  const std::filesystem::path dir = options_.games_folder / name;
  std::string error;
  if (!redgem::create_game(dir, template_name, error)) {
    error_ = error;
    return false;
  }
  log::info("created " + dir.string());
  return open_game(dir);
}

bool ProjectHub::open_game(const std::filesystem::path& typed) {
  const std::filesystem::path game_dir = normalized_folder(editor_logic::expand_home(typed, platform::home_dir()));
  std::error_code fs_error;
  if (!std::filesystem::exists(game_dir / config::kSceneFileName, fs_error)) {
    error_ = pretty_path(game_dir) + " is not a game (it has no " + config::kSceneFileName + ")";
    return false;
  }
  remember_game(options_.recent_file, game_dir);
  error_.clear();
  chosen_ = game_dir;
  return true;
}

void ProjectHub::frame() {
  BeginDrawing();
  const ImVec4 background = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
  ClearBackground(ColorFromNormalized({background.x, background.y, background.z, background.w}));
  rlImGuiBegin();
  draw();
  rlImGuiEnd();
  EndDrawing();
}

void ProjectHub::draw() {
  begin_hub_window();
  draw_title();
  ImGui::BeginChild(kNewGameId, ImVec2(kHubSidebarWidth, 0.0f));
  draw_new_game(kHubSidebarWidth);
  ImGui::EndChild();
  ImGui::SameLine(0.0f, kHubPadding);
  ImGui::BeginChild(kGamesId);
  draw_games();
  ImGui::EndChild();
  ImGui::End();
}

void ProjectHub::draw_title() {
  ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kTitleFontScale);
  ImGui::TextColored(ui::accent_color(), ICON_FA_GEM);
  ImGui::SameLine();
  ImGui::TextUnformatted(kHubTitle);
  ImGui::PopFont();
  ImGui::SameLine();
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y);
  ImGui::TextColored(ui::muted_color(), "Make 3D games in Ruby");
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
}

void ProjectHub::draw_new_game(float width) {
  ImGui::SeparatorText(ICON_FA_FILE_CIRCLE_PLUS "  New game");
  ImGui::TextColored(ui::muted_color(), "Name");
  ImGui::SetNextItemWidth(kFillWidth);
  const bool entered = ImGui::InputText(kNameId, &new_name_, ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::Spacing();
  draw_templates();
  ImGui::Spacing();
  ImGui::TextColored(ui::muted_color(), "%s", pretty_path(options_.games_folder / new_name_).c_str());
  ImGui::Spacing();
  const bool pressed = accent_button(ICON_FA_PLUS "  Create game", width);
  if (pressed || entered) create_game(new_name_, template_);
  draw_error();
}

// What a new game starts from: one card per template, the chosen one selected.
void ProjectHub::draw_templates() {
  ImGui::TextColored(ui::muted_color(), "Start from");
  for (const std::string& name : game_templates()) {
    const std::string title = editor_logic::capitalized(name);
    const std::string description = template_description(name);
    const CardLabel label{name.c_str(), template_icon(name), title, description};
    if (card(label, template_ == name, kTemplateCardHeight)) template_ = name;
  }
}

void ProjectHub::draw_error() {
  if (error_.empty()) return;
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, ui::error_color());
  ImGui::TextWrapped(ICON_FA_CIRCLE_XMARK "  %s", error_.c_str());
  ImGui::PopStyleColor();
}

void ProjectHub::draw_games() {
  ImGui::SeparatorText(ICON_FA_GAMEPAD "  Games");
  const float footer = ImGui::GetFrameHeightWithSpacing() * kOpenFooterRows + ImGui::GetStyle().ItemSpacing.y;
  ImGui::BeginChild(kGameListId, ImVec2(0.0f, -footer));
  draw_game_list();
  ImGui::EndChild();
  draw_open_folder();
}

void ProjectHub::draw_game_list() {
  if (games_.empty()) ImGui::TextColored(ui::muted_color(), "No games yet. Create one on the left.");
  for (std::size_t i = 0; i < games_.size(); ++i) {
    const std::string id = games_[i].string();
    const std::string path = pretty_path(games_[i]);
    const bool clicked = card({id.c_str(), ICON_FA_GAMEPAD, titles_[i], path}, false, kHubCardHeight);
    if (clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) open_game(games_[i]);
    ui::tooltip("Double click to open");
  }
}

// A field for the path of a game that is not in the list.
void ProjectHub::draw_open_folder() {
  ImGui::SeparatorText(ICON_FA_FOLDER_OPEN "  Open a game folder");
  ImGui::SetNextItemWidth(-kOpenButtonWidth - ImGui::GetStyle().ItemSpacing.x);
  const bool entered =
      ImGui::InputTextWithHint(kPathId, "/path/to/game", &open_path_, ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::SameLine();
  if (ImGui::Button("Open", ImVec2(kOpenButtonWidth, 0.0f)) || entered) open_game(open_path_);
}

}  // namespace redgem
