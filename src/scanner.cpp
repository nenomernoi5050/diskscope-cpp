#include "diskscope/scanner.hpp"

#include "diskscope/matcher.hpp"
#include "diskscope/ooxml.hpp"
#include "diskscope/text.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <fstream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <unordered_set>

namespace diskscope {
namespace {

const std::unordered_set<std::string> kTextExtensions{
    ".txt", ".csv", ".tsv", ".log", ".json", ".xml", ".md", ".reg"
};
const std::unordered_set<std::string> kOfficeExtensions{".docx", ".xlsx", ".pptx"};

std::string lower_extension(const std::filesystem::path& path) {
    auto value = path.extension().string();
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open file");
    }
    input.seekg(0, std::ios::end);
    const auto length = input.tellg();
    input.seekg(0, std::ios::beg);
    if (length < 0) {
        throw std::runtime_error("cannot determine file size");
    }
    std::vector<std::uint8_t> output(static_cast<std::size_t>(length));
    input.read(reinterpret_cast<char*>(output.data()), length);
    if (!input && !output.empty()) {
        throw std::runtime_error("cannot read file");
    }
    return output;
}

std::size_t worker_count(const ScanOptions& options) {
    const auto requested = options.thread_count == 0 ? std::thread::hardware_concurrency()
                                                      : options.thread_count;
    return std::clamp<std::size_t>(requested == 0 ? 2 : requested, 1, 16);
}

template <typename ByteContainer>
std::size_t find_bytes(const ByteContainer& haystack,
                       const std::vector<std::uint8_t>& needle,
                       std::size_t start) {
    if (needle.empty() || haystack.size() < needle.size()) {
        return std::string::npos;
    }
    const auto begin = std::search(haystack.begin() + static_cast<std::ptrdiff_t>(start),
                                   haystack.end(), needle.begin(), needle.end());
    return begin == haystack.end()
               ? std::string::npos
               : static_cast<std::size_t>(std::distance(haystack.begin(), begin));
}

}  // namespace

ScanResult scan_directory(const std::filesystem::path& root,
                          const std::vector<std::string>& keywords,
                          const ScanOptions& options) {
    ScanResult result;
    Matcher matcher(keywords);
    std::vector<std::filesystem::path> files;
    std::error_code error;
    if (!std::filesystem::is_directory(root, error)) {
        result.errors.push_back("scan root is not a readable directory");
        return result;
    }

    const auto flags = std::filesystem::directory_options::skip_permission_denied;
    for (std::filesystem::recursive_directory_iterator iterator(root, flags, error), end;
         iterator != end; iterator.increment(error)) {
        if (error) {
            result.errors.push_back(error.message());
            error.clear();
            continue;
        }
        if (!iterator->is_regular_file(error)) {
            continue;
        }
        const auto extension = lower_extension(iterator->path());
        if (kTextExtensions.count(extension) || kOfficeExtensions.count(extension)) {
            files.push_back(iterator->path());
        }
    }

    std::atomic<std::size_t> next{0};
    std::mutex result_mutex;
    std::vector<std::thread> workers;
    const auto run_worker = [&] {
        while (true) {
            const auto index = next.fetch_add(1);
            if (index >= files.size()) {
                return;
            }
            const auto& path = files[index];
            std::error_code size_error;
            const auto size = std::filesystem::file_size(path, size_error);
            if (size_error || size > options.max_file_size) {
                std::lock_guard lock(result_mutex);
                ++result.files_skipped;
                continue;
            }
            try {
                std::vector<Finding> local_findings;
                const auto extension = lower_extension(path);
                if (kOfficeExtensions.count(extension)) {
                    for (const auto& [member, text] : extract_ooxml_text(path, options.max_file_size)) {
                        for (const auto& match : matcher.match(text)) {
                            local_findings.push_back(
                                {path, member, match.keyword, match.character_offset, "Unicode text"});
                        }
                    }
                } else {
                    const auto bytes = read_bytes(path);
                    const auto text = decode_text_bytes(bytes);
                    for (const auto& match : matcher.match(text)) {
                        local_findings.push_back(
                            {path, {}, match.keyword, match.character_offset, "Unicode text"});
                    }
                }
                std::lock_guard lock(result_mutex);
                ++result.files_scanned;
                result.bytes_scanned += size;
                for (auto& finding : local_findings) {
                    if (result.findings.size() < options.max_findings) {
                        result.findings.push_back(std::move(finding));
                    }
                }
            } catch (const std::exception& exception) {
                std::lock_guard lock(result_mutex);
                ++result.files_skipped;
                result.errors.push_back(path.u8string() + ": " + exception.what());
            }
        }
    };

    for (std::size_t i = 0; i < worker_count(options); ++i) {
        workers.emplace_back(run_worker);
    }
    for (auto& worker : workers) {
        worker.join();
    }
    std::sort(result.findings.begin(), result.findings.end(), [](const auto& left, const auto& right) {
        return std::tie(left.source, left.offset, left.keyword) <
               std::tie(right.source, right.offset, right.keyword);
    });
    return result;
}

ScanResult scan_image(const std::filesystem::path& image,
                      const std::vector<std::string>& keywords,
                      const ScanOptions& options) {
    ScanResult result;
    std::ifstream input(image, std::ios::binary);
    if (!input) {
        result.errors.push_back("cannot open image in read-only mode");
        return result;
    }

    struct Pattern {
        std::string keyword;
        std::string encoding;
        std::vector<std::uint8_t> bytes;
    };
    std::vector<Pattern> patterns;
    std::size_t overlap = 0;
    for (const auto& keyword : keywords) {
        Pattern utf8{keyword, "UTF-8", {keyword.begin(), keyword.end()}};
        Pattern utf16{keyword, "UTF-16LE", encode_utf16le(decode_utf8(keyword))};
        overlap = std::max({overlap, utf8.bytes.size(), utf16.bytes.size()});
        patterns.push_back(std::move(utf8));
        patterns.push_back(std::move(utf16));
    }
    overlap = overlap > 0 ? overlap - 1 : 0;

    constexpr std::size_t chunk_size = 4ULL * 1024ULL * 1024ULL;
    std::vector<std::uint8_t> carry;
    std::vector<std::uint8_t> chunk(chunk_size);
    std::uint64_t consumed = 0;
    std::set<std::tuple<std::uint64_t, std::string, std::string>> emitted;
    while (input && result.findings.size() < options.max_findings) {
        input.read(reinterpret_cast<char*>(chunk.data()), static_cast<std::streamsize>(chunk.size()));
        const auto read = static_cast<std::size_t>(input.gcount());
        if (read == 0) {
            break;
        }
        std::vector<std::uint8_t> window;
        window.reserve(carry.size() + read);
        window.insert(window.end(), carry.begin(), carry.end());
        window.insert(window.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(read));
        const auto base = consumed - static_cast<std::uint64_t>(carry.size());
        for (const auto& pattern : patterns) {
            std::size_t position = 0;
            while ((position = find_bytes(window, pattern.bytes, position)) != std::string::npos) {
                const auto absolute = base + position;
                const auto key = std::make_tuple(absolute, pattern.keyword, pattern.encoding);
                if (emitted.insert(key).second) {
                    result.findings.push_back(
                        {image, {}, pattern.keyword, absolute, pattern.encoding});
                    if (result.findings.size() >= options.max_findings) {
                        break;
                    }
                }
                position += std::max<std::size_t>(1, pattern.bytes.size());
            }
        }
        consumed += read;
        const auto keep = std::min(overlap, window.size());
        carry.assign(window.end() - static_cast<std::ptrdiff_t>(keep), window.end());
    }
    result.files_scanned = 1;
    result.bytes_scanned = consumed;
    return result;
}

}  // namespace diskscope
