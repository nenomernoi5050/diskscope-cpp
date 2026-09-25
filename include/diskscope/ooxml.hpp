#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace diskscope {

using ExtractedPart = std::pair<std::string, std::string>;

std::vector<ExtractedPart> extract_ooxml_text(const std::filesystem::path& path,
                                              std::uintmax_t max_archive_size);

}  // namespace diskscope

