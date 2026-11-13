#pragma once

#include <filesystem>
#include <string>

namespace redgem::android_stage {

std::filesystem::path ensure_cache(const std::filesystem::path& requested, std::string& error);

}  // namespace redgem::android_stage
