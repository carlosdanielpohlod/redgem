#pragma once

#include <string>

// The cards the project hub lists templates and games as.
namespace redgem::hub {

// What a card is: its id, and what it shows.
struct CardLabel {
  const char* id;
  const char* icon;
  const std::string& title;
  const std::string& detail;  // a muted second line
};

// A full-width selectable card with an icon, a title and a muted second line. True when clicked.
bool card(const CardLabel& label, bool selected, float height);

// The icon a template asks for by name (its template.json); the usual one for any other name.
const char* template_icon(const std::string& name);

}  // namespace redgem::hub
