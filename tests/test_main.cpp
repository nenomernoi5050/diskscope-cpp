#include "diskscope/matcher.hpp"
#include "diskscope/report.hpp"
#include "diskscope/scanner.hpp"
#include "diskscope/text.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_unicode_matcher() {
    diskscope::Matcher matcher({"конфиденциально", "internal-only"});
    const auto matches = matcher.match("Пометка: КоНфИдЕнЦиАлЬнО. INTERNAL-ONLY.");
    require(matches.size() == 2, "Unicode case-insensitive matching failed");
}

void test_directory_scan(const std::filesystem::path& root) {
    const auto target = root / "sample.txt";
    {
        std::ofstream output(target, std::ios::binary);
        output << "public header\nДСП\n";
    }
    diskscope::ScanOptions options;
    options.thread_count = 2;
    const auto result = diskscope::scan_directory(root, {"дсп"}, options);
    require(result.errors.empty(), "directory scan returned an error");
    require(result.files_scanned == 1, "directory scan count is wrong");
    require(result.findings.size() == 1, "directory finding was not produced");
}

void test_image_scan(const std::filesystem::path& root) {
    const auto image = root / "sample.img";
    const std::string marker = "internal-only";
    const auto utf16 = diskscope::encode_utf16le(diskscope::decode_utf8(marker));
    {
        std::ofstream output(image, std::ios::binary);
        output << "prefix" << marker << "middle";
        output.write(reinterpret_cast<const char*>(utf16.data()),
                     static_cast<std::streamsize>(utf16.size()));
    }
    const auto result = diskscope::scan_image(image, {marker});
    require(result.errors.empty(), "image scan returned an error");
    require(result.findings.size() == 2, "UTF-8 and UTF-16LE signatures were not found");
    require(result.findings[0].offset == 6, "image byte offset is wrong");
}

void test_report() {
    diskscope::ScanResult result;
    result.files_scanned = 1;
    result.findings.push_back({"sample.txt", {}, "internal-only", 4, "Unicode text"});
    const auto report = diskscope::make_json_report(result, "directory");
    require(report.find("\"read_only\": true") != std::string::npos,
            "report does not state its read-only mode");
    require(report.find("internal-only") != std::string::npos,
            "report does not include a finding");
}

}  // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "diskscope-tests";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root);
    try {
        test_unicode_matcher();
        test_directory_scan(root);
        test_image_scan(root);
        test_report();
        std::filesystem::remove_all(root, error);
        std::cout << "All DiskScope tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::filesystem::remove_all(root, error);
        std::cerr << exception.what() << '\n';
        return 1;
    }
}

