#pragma once

#include <filesystem>
#include <string>

namespace redgem::android_stage {

bool ensure_cache(const std::filesystem::path& cache, std::string& error);

}  // namespace redgem::android_stage
