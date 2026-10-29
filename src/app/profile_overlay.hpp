#pragma once
namespace redgem {
struct ScriptContext;
// Uses the completed frame's statistics, shared by player, editor and recorded frames.
bool shows_profile_stats(const ScriptContext& context);
void append_profile_overlay(ScriptContext& context);
}  // namespace redgem
