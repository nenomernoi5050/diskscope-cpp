#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace diskscope {

struct Finding {
    std::filesystem::path source;
    std::string container_member;
    std::string keyword;
    std::uint64_t offset{};
    std::string encoding;
};

struct ScanOptions {
    std::size_t thread_count{};
    std::uintmax_t max_file_size{64ULL * 1024ULL * 1024ULL};
    std::size_t max_findings{10'000};
};

struct ScanResult {
    std::vector<Finding> findings;
    std::uint64_t files_scanned{};
    std::uint64_t files_skipped{};
    std::uint64_t bytes_scanned{};
    std::vector<std::string> errors;
};

}  // namespace diskscope

