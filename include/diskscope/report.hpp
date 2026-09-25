#pragma once

#include "diskscope/types.hpp"

#include <filesystem>
#include <string>

namespace diskscope {

std::string make_json_report(const ScanResult& result, std::string_view mode);
void write_json_report(const std::filesystem::path& path,
                       const ScanResult& result,
                       std::string_view mode);

}  // namespace diskscope

