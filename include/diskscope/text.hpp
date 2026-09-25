#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace diskscope {

std::u32string decode_utf8(std::string_view input);
std::u32string decode_utf16le(const std::vector<std::uint8_t>& input,
                             std::size_t start = 0);
std::string encode_utf8(std::u32string_view input);
std::vector<std::uint8_t> encode_utf16le(std::u32string_view input);
std::u32string fold_case(std::u32string_view input);
std::string decode_text_bytes(const std::vector<std::uint8_t>& input);
std::string strip_xml(std::string_view input);
std::string json_escape(std::string_view input);

}  // namespace diskscope

