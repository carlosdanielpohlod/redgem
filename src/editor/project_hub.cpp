#include "editor/project_hub.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <raylib.h>
#include <rlgl.h>
#include <rlImGui.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "app/game_project.hpp"
#include "core/config.hpp"
#include "core/log.hpp"
#include "editor/editor_config.hpp"
#include "editor/ui/style.hpp"
#include "render/gpu_select.hpp"

namespace redgem {
namespace {

using namespace editor_config;

constexpr int kHubFps = 60;
constexpr const char* kDefaultTemplate = "platformer";
constexpr const char* kDefaultGameName = "My Game";
constexpr float kTemplateCardHeight = 58.0f;
constexpr float kCenter = 0.5f;
constexpr float kOpenButtonWidth = 90.0f;
constexpr float kCardTextLines = 2.0f;     // title and detail
constexpr float kCardIconColumn = 2.0f;    // icon column width, in icon buttons
constexpr float kOpenFooterRows = 2.0f;    // "Open a game folder" heading and its path field
constexpr const char* kHomePrefix = "~/";

const char* template_icon(const std::string& name) { return name == "empty" ? ICON_FA_CUBE : ICON_FA_PERSON_RUNNING; }

// "~/RedGemGames/x" reads better than the full home path.
std::string pretty_path(const std::filesystem::path& path) {
  const char* home = std::getenv("HOME");
  const std::string text = path.string();
  if (home != nullptr && *home != '\0' && text.rfind(home, 0) == 0) return "~" + text.substr(std::string(home).size());
  return text;
}

// A full-width selectable card with an icon, a title and a muted second line.
bool card(const char* id, const char* icon, const std::string& title, const std::string& detail, bool selected,
          float height) {
  ImGui::PushID(id);
  const ImVec2 start = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::Selectable("##card", selected, ImGuiSelectableFlags_AllowDoubleClick,
                                         ImVec2(ImGui::GetContentRegionAvail().x, height));
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImGuiStyle& style = ImGui::GetStyle();
  const float line = ImGui::GetTextLineHeight();
  const float text_top = start.y + (height - line * kCardTextLines - style.ItemSpacing.y) * kCenter;
  const float icon_width = ImGui::GetFontSize() * kIconButtonWidth * kCardIconColumn;
  draw->AddText(ImVec2(start.x + style.FramePadding.x, start.y + (height - line) * kCenter),
                ImGui::GetColorU32(ui::accent_color()), icon);
  draw->AddText(ImVec2(start.x + icon_width, text_top), ImGui::GetColorU32(ImGuiCol_Text), title.c_str());
  draw->AddText(ImVec2(start.x + icon_width, text_top + line + style.ItemSpacing.y),
                ImGui::GetColorU32(ui::muted_color()), detail.c_str());
  ImGui::PopID();
  return clicked;
}

}  // namespace

ProjectHub::ProjectHub(Options options) : options_(std::move(options)), template_(kDefaultTemplate) {
  prefer_discrete_gpu();
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
  // "~/games/x" as typed in the path field.
  const char* home = std::getenv("HOME");
  const std::string text = typed.string();
  const std::string prefix = kHomePrefix;
  const std::filesystem::path game_dir = normalized_folder(
      home != nullptr && text.rfind(prefix, 0) == 0 ? std::filesystem::path(home) / text.substr(prefix.size()) : typed);
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
  if (screenshot_) {
    rlDrawRenderBatchActive();
    Image image = LoadImageFromScreen();
    std::filesystem::create_directories(screenshot_->parent_path());
    ExportImage(image, screenshot_->string().c_str());
    UnloadImage(image);
    screenshot_.reset();
  }
  EndDrawing();
}

void ProjectHub::draw() {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kHubPadding, kHubPadding));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::Begin("##hub", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::PopStyleVar(2);

  // Title.
  ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kTitleFontScale);
  ImGui::TextColored(ui::accent_color(), ICON_FA_GEM);
  ImGui::SameLine();
  ImGui::TextUnformatted("RedGem");
  ImGui::PopFont();
  ImGui::SameLine();
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y);
  ImGui::TextColored(ui::muted_color(), "Make 3D games in Ruby");
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::BeginChild("new", ImVec2(kHubSidebarWidth, 0.0f));
  draw_new_game(kHubSidebarWidth);
  ImGui::EndChild();
  ImGui::SameLine(0.0f, kHubPadding);
  ImGui::BeginChild("games");
  draw_games();
  ImGui::EndChild();
  ImGui::End();
}

void ProjectHub::draw_new_game(float width) {
  ImGui::SeparatorText(ICON_FA_FILE_CIRCLE_PLUS "  New game");
  ImGui::TextColored(ui::muted_color(), "Name");
  ImGui::SetNextItemWidth(-1.0f);
  const bool entered = ImGui::InputText("##name", &new_name_, ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::Spacing();
  ImGui::TextColored(ui::muted_color(), "Start from");
  for (const std::string& name : game_templates()) {
    std::string title = name;
    title[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(title[0])));
    if (card(name.c_str(), template_icon(name), title, template_description(name), template_ == name,
             kTemplateCardHeight)) {
      template_ = name;
    }
  }
  ImGui::Spacing();
  ImGui::TextColored(ui::muted_color(), "%s", pretty_path(options_.games_folder / new_name_).c_str());
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Button, ui::accent_color());
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ui::accent_color());
  const bool pressed = ImGui::Button(ICON_FA_PLUS "  Create game", ImVec2(width, 0.0f));
  ImGui::PopStyleColor(2);
  if (pressed || entered) create_game(new_name_, template_);
  if (!error_.empty()) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ui::error_color());
    ImGui::TextWrapped(ICON_FA_CIRCLE_XMARK "  %s", error_.c_str());
    ImGui::PopStyleColor();
  }
}

void ProjectHub::draw_games() {
  ImGui::SeparatorText(ICON_FA_GAMEPAD "  Games");
  const float footer = ImGui::GetFrameHeightWithSpacing() * kOpenFooterRows + ImGui::GetStyle().ItemSpacing.y;
  ImGui::BeginChild("list", ImVec2(0.0f, -footer));
  if (games_.empty()) {
    ImGui::TextColored(ui::muted_color(), "No games yet. Create one on the left.");
  }
  for (std::size_t i = 0; i < games_.size(); ++i) {
    const std::filesystem::path& game = games_[i];
    if (card(game.string().c_str(), ICON_FA_GAMEPAD, titles_[i], pretty_path(game), false, kHubCardHeight) &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      open_game(game);
    }
    ui::tooltip("Double click to open");
  }
  ImGui::EndChild();
  ImGui::SeparatorText(ICON_FA_FOLDER_OPEN "  Open a game folder");
  ImGui::SetNextItemWidth(-kOpenButtonWidth - ImGui::GetStyle().ItemSpacing.x);
  const bool entered =
      ImGui::InputTextWithHint("##path", "/path/to/game", &open_path_, ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::SameLine();
  if (ImGui::Button("Open", ImVec2(kOpenButtonWidth, 0.0f)) || entered) open_game(open_path_);
}

}  // namespace redgem
