#include "diskscope/report.hpp"
#include "diskscope/scanner.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Arguments {
    std::filesystem::path root;
    std::filesystem::path image;
    std::filesystem::path keyword_file;
    std::filesystem::path report{"report.json"};
    diskscope::ScanOptions options;
};

void print_usage() {
    std::cout
        << "DiskScope 1.0 — authorized read-only content audit\n\n"
        << "Directory: diskscope --root PATH --keywords FILE [--report FILE]\n"
        << "Image:     diskscope --image FILE --keywords FILE [--report FILE]\n"
        << "Options:   --threads N --max-file-mb N --max-findings N\n";
}

Arguments parse_arguments(int argc, char** argv) {
    Arguments arguments;
    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];
        const auto value = [&](const char* name) -> std::string {
            if (++i >= argc) {
                throw std::runtime_error(std::string("missing value for ") + name);
            }
            return argv[i];
        };
        if (option == "--root") {
            arguments.root = std::filesystem::u8path(value("--root"));
        } else if (option == "--image") {
            arguments.image = std::filesystem::u8path(value("--image"));
        } else if (option == "--keywords") {
            arguments.keyword_file = std::filesystem::u8path(value("--keywords"));
        } else if (option == "--report") {
            arguments.report = std::filesystem::u8path(value("--report"));
        } else if (option == "--threads") {
            arguments.options.thread_count = std::stoull(value("--threads"));
        } else if (option == "--max-file-mb") {
            arguments.options.max_file_size = std::stoull(value("--max-file-mb")) * 1024ULL * 1024ULL;
        } else if (option == "--max-findings") {
            arguments.options.max_findings = std::stoull(value("--max-findings"));
        } else if (option == "--help" || option == "-h") {
            print_usage();
            std::exit(0);
        } else {
            throw std::runtime_error("unknown option: " + option);
        }
    }
    if (arguments.keyword_file.empty() || (arguments.root.empty() == arguments.image.empty())) {
        throw std::runtime_error("choose exactly one of --root or --image and provide --keywords");
    }
    return arguments;
}

std::vector<std::string> load_keywords(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open keyword file");
    }
    std::vector<std::string> keywords;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto first = line.find_first_not_of(" \t");
        const auto last = line.find_last_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') {
            continue;
        }
        keywords.push_back(line.substr(first, last - first + 1));
    }
    if (keywords.empty()) {
        throw std::runtime_error("keyword file contains no active markers");
    }
    return keywords;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto arguments = parse_arguments(argc, argv);
        const auto keywords = load_keywords(arguments.keyword_file);
        const bool image_mode = !arguments.image.empty();
        const auto result = image_mode
                                ? diskscope::scan_image(arguments.image, keywords, arguments.options)
                                : diskscope::scan_directory(arguments.root, keywords, arguments.options);
        diskscope::write_json_report(arguments.report, result,
                                     image_mode ? "offline-image" : "directory");
        std::cout << "Scanned: " << result.files_scanned << " file(s), "
                  << result.bytes_scanned << " byte(s)\n"
                  << "Findings: " << result.findings.size() << "\n"
                  << "Errors: " << result.errors.size() << "\n"
                  << "Report: " << arguments.report.u8string() << '\n';
        return result.errors.empty() ? 0 : 1;
    } catch (const std::exception& exception) {
        std::cerr << "DiskScope error: " << exception.what() << "\n\n";
        print_usage();
        return 1;
    }
}

