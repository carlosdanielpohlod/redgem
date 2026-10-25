// Inspector rows of a material's texture slots: the picture each one shows, chosen from the game's
// images/ folder (or dragged there from the Project panel), or none.

#include <imgui.h>

#include <string>

#include "core/config.hpp"
#include "core/filled_texts.hpp"
#include "core/log.hpp"
#include "core/translations.hpp"
#include "editor/document/material_textures.hpp"
#include "editor/editor_config.hpp"
#include "editor/ui/inspector_cards.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui::inspector {
namespace {

using namespace editor_config;
using material_textures::Slot;

constexpr const char* kPicturesPopup = "pictures";
constexpr const char* kSlotButtonId = "##picture";
constexpr int kNoPicture = kNoIndex;

// Each slot's row: the key of its label and of its tooltip, in material_textures::kSlots' order.
struct SlotText {
  const char* label;
  const char* tooltip;
};
constexpr SlotText kSlotTexts[] = {
    {"editor.inspector.material.slot.base_color", "editor.inspector.material.slot.base_color_tooltip"},
    {"editor.inspector.material.slot.metallic_roughness", "editor.inspector.material.slot.metallic_roughness_tooltip"},
    {"editor.inspector.material.slot.normal", "editor.inspector.material.slot.normal_tooltip"},
    {"editor.inspector.material.slot.occlusion", "editor.inspector.material.slot.occlusion_tooltip"},
    {"editor.inspector.material.slot.emission", "editor.inspector.material.slot.emission_tooltip"},
};

// What a slot's button says: the picture's name, its number, or that there is none.
const char* picture_label(const SceneDocument& doc, int image) {
  static FilledTexts numbered("editor.inspector.material.picture_numbered", "number");
  static std::string named;
  if (image == kNoPicture) return tr("editor.inspector.material.no_picture");
  named = doc.image_name(image);
  return named.empty() ? numbered.with(image) : named.c_str();
}

// The slot shows `image` from now on: one undo step.
void set_slot(PanelContext& context, int material, Slot slot, int image) {
  SceneDocument& doc = context.session.document();
  MaterialDesc edited = doc.material(material);
  if (material_textures::image_in(edited, slot) == image) return;
  material_textures::image_in(edited, slot) = image;
  doc.set_material(material, edited);
}

// A picture of the game's images/ folder goes into the scene and into the slot.
void use_picture(PanelContext& context, int material, Slot slot, const std::string& file) {
  std::string error;
  const std::filesystem::path path = context.session.game_dir() / config::kImagesDirName / file;
  const int image = context.session.document().add_image(path, error);
  if (image == kNoPicture) return log::error("cannot use " + file + ": " + error);
  set_slot(context, material, slot, image);
}

// What the slot's button offers: none, or each picture of images/.
void draw_picture_choices(PanelContext& context, int material, Slot slot) {
  if (ImGui::MenuItem(tr("editor.inspector.material.no_picture"))) set_slot(context, material, slot, kNoPicture);
  ImGui::Separator();
  if (context.files.images.empty()) ImGui::TextColored(muted_color(), "%s", tr("editor.inspector.material.no_images"));
  for (const std::string& file : context.files.images) {
    if (ImGui::MenuItem((std::string(ICON_FA_IMAGE "  ") + file).c_str())) {
      use_picture(context, material, slot, file);
    }
  }
}

// A picture dragged from the Project panel's images onto the slot's button.
void accept_dropped_picture(PanelContext& context, int material, Slot slot) {
  if (!ImGui::BeginDragDropTarget()) return;
  if (const char* file = dropped_text(kPayloadImage)) use_picture(context, material, slot, file);
  ImGui::EndDragDropTarget();
}

void draw_slot(PanelContext& context, int material, Slot slot) {
  const SlotText& text = kSlotTexts[static_cast<std::size_t>(slot)];
  const SceneDocument& doc = context.session.document();
  ImGui::PushID(static_cast<int>(slot));
  property_label(tr(text.label));
  tooltip(tr(text.tooltip));
  const char* shown = picture_label(doc, material_textures::image_in(doc.material(material), slot));
  if (ImGui::Button((std::string(shown) + kSlotButtonId).c_str(), ImVec2(kFillWidth, 0.0f))) {
    ImGui::OpenPopup(kPicturesPopup);
  }
  accept_dropped_picture(context, material, slot);
  popup(kPicturesPopup, [&context, material, slot] { draw_picture_choices(context, material, slot); });
  ImGui::PopID();
}

}  // namespace

void draw_texture_slots(PanelContext& context, int material) {
  for (const Slot slot : material_textures::kSlots) draw_slot(context, material, slot);
}

}  // namespace redgem::ui::inspector
