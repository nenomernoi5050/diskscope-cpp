#pragma once

#include "diskscope/types.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace diskscope {

ScanResult scan_directory(const std::filesystem::path& root,
                          const std::vector<std::string>& keywords,
                          const ScanOptions& options = {});

ScanResult scan_image(const std::filesystem::path& image,
                      const std::vector<std::string>& keywords,
                      const ScanOptions& options = {});

}  // namespace diskscope

