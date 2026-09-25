#include "diskscope/report.hpp"

#include "diskscope/text.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace diskscope {

std::string make_json_report(const ScanResult& result, std::string_view mode) {
    std::ostringstream output;
    output << "{\n"
           << "  \"mode\": \"" << json_escape(mode) << "\",\n"
           << "  \"read_only\": true,\n"
           << "  \"files_scanned\": " << result.files_scanned << ",\n"
           << "  \"files_skipped\": " << result.files_skipped << ",\n"
           << "  \"bytes_scanned\": " << result.bytes_scanned << ",\n"
           << "  \"findings\": [\n";
    for (std::size_t i = 0; i < result.findings.size(); ++i) {
        const auto& finding = result.findings[i];
        output << "    {\"source\": \"" << json_escape(finding.source.u8string())
               << "\", \"container_member\": \"" << json_escape(finding.container_member)
               << "\", \"keyword\": \"" << json_escape(finding.keyword)
               << "\", \"offset\": " << finding.offset
               << ", \"encoding\": \"" << json_escape(finding.encoding) << "\"}"
               << (i + 1 == result.findings.size() ? "\n" : ",\n");
    }
    output << "  ],\n  \"errors\": [";
    for (std::size_t i = 0; i < result.errors.size(); ++i) {
        output << (i == 0 ? "\n" : ",\n") << "    \"" << json_escape(result.errors[i]) << "\"";
    }
    if (!result.errors.empty()) {
        output << '\n';
    }
    output << "  ]\n}\n";
    return output.str();
}

void write_json_report(const std::filesystem::path& path,
                       const ScanResult& result,
                       std::string_view mode) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("cannot create report");
    }
    output << make_json_report(result, mode);
}

}  // namespace diskscope

