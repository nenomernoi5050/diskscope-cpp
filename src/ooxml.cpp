#include "diskscope/ooxml.hpp"

#include "diskscope/text.hpp"

#include <miniz.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace diskscope {
namespace {

constexpr std::uintmax_t kMaxExpandedPart = 32ULL * 1024ULL * 1024ULL;

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool relevant_part(const std::string& name) {
    const auto lower = lower_ascii(name);
    if (lower.size() < 4 || lower.substr(lower.size() - 4) != ".xml") {
        return false;
    }
    return lower.rfind("word/", 0) == 0 || lower.rfind("xl/", 0) == 0 ||
           lower.rfind("ppt/slides/", 0) == 0 ||
           lower.rfind("ppt/notes", 0) == 0;
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path,
                                    std::uintmax_t max_size) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > max_size) {
        throw std::runtime_error("archive is unavailable or exceeds the configured limit");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open archive");
    }
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!input && !data.empty()) {
        throw std::runtime_error("cannot read archive");
    }
    return data;
}

}  // namespace

std::vector<ExtractedPart> extract_ooxml_text(const std::filesystem::path& path,
                                              std::uintmax_t max_archive_size) {
    auto archive_data = read_file(path, max_archive_size);
    mz_zip_archive archive{};
    if (!mz_zip_reader_init_mem(&archive, archive_data.data(), archive_data.size(), 0)) {
        throw std::runtime_error("invalid OOXML ZIP container");
    }

    std::vector<ExtractedPart> output;
    const auto count = mz_zip_reader_get_num_files(&archive);
    for (mz_uint index = 0; index < count; ++index) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&archive, index, &stat) || stat.m_is_directory) {
            continue;
        }
        const std::string name = stat.m_filename;
        if (!relevant_part(name) || stat.m_uncomp_size > kMaxExpandedPart) {
            continue;
        }
        std::size_t size = 0;
        void* raw = mz_zip_reader_extract_to_heap(&archive, index, &size, 0);
        if (raw == nullptr) {
            continue;
        }
        std::string xml(static_cast<const char*>(raw), size);
        mz_free(raw);
        output.emplace_back(name, strip_xml(xml));
    }
    mz_zip_reader_end(&archive);
    return output;
}

}  // namespace diskscope

