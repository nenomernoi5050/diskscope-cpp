#include "diskscope/text.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace diskscope {
namespace {

char32_t folded(char32_t value) {
    if (value >= U'A' && value <= U'Z') {
        return value + 0x20;
    }
    if (value >= U'А' && value <= U'Я') {
        return value + 0x20;
    }
    if (value == U'Ё') {
        return U'ё';
    }
    return value;
}

void append_utf8(std::string& output, char32_t value) {
    if (value <= 0x7F) {
        output.push_back(static_cast<char>(value));
    } else if (value <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (value >> 6)));
        output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
    } else if (value <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (value >> 12)));
        output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | (value >> 18)));
        output.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
    }
}

std::string replace_all(std::string value, std::string_view from, std::string_view to) {
    std::size_t position = 0;
    while ((position = value.find(from, position)) != std::string::npos) {
        value.replace(position, from.size(), to);
        position += to.size();
    }
    return value;
}

}  // namespace

std::u32string decode_utf8(std::string_view input) {
    std::u32string output;
    output.reserve(input.size());
    for (std::size_t i = 0; i < input.size();) {
        const auto first = static_cast<unsigned char>(input[i]);
        char32_t value = 0;
        std::size_t length = 0;
        if (first < 0x80) {
            value = first;
            length = 1;
        } else if ((first & 0xE0) == 0xC0) {
            value = first & 0x1F;
            length = 2;
        } else if ((first & 0xF0) == 0xE0) {
            value = first & 0x0F;
            length = 3;
        } else if ((first & 0xF8) == 0xF0) {
            value = first & 0x07;
            length = 4;
        } else {
            ++i;
            continue;
        }
        if (i + length > input.size()) {
            break;
        }
        bool valid = true;
        for (std::size_t j = 1; j < length; ++j) {
            const auto byte = static_cast<unsigned char>(input[i + j]);
            if ((byte & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            value = (value << 6) | (byte & 0x3F);
        }
        if (valid && value <= 0x10FFFF) {
            output.push_back(value);
            i += length;
        } else {
            ++i;
        }
    }
    return output;
}

std::u32string decode_utf16le(const std::vector<std::uint8_t>& input, std::size_t start) {
    std::u32string output;
    for (std::size_t i = start; i + 1 < input.size(); i += 2) {
        const auto first = static_cast<std::uint16_t>(input[i]) |
                           (static_cast<std::uint16_t>(input[i + 1]) << 8);
        if (first >= 0xD800 && first <= 0xDBFF && i + 3 < input.size()) {
            const auto second = static_cast<std::uint16_t>(input[i + 2]) |
                                (static_cast<std::uint16_t>(input[i + 3]) << 8);
            if (second >= 0xDC00 && second <= 0xDFFF) {
                output.push_back(0x10000 + ((first - 0xD800) << 10) + (second - 0xDC00));
                i += 2;
                continue;
            }
        }
        if (first < 0xD800 || first > 0xDFFF) {
            output.push_back(first);
        }
    }
    return output;
}

std::string encode_utf8(std::u32string_view input) {
    std::string output;
    for (const auto value : input) {
        append_utf8(output, value);
    }
    return output;
}

std::vector<std::uint8_t> encode_utf16le(std::u32string_view input) {
    std::vector<std::uint8_t> output;
    for (auto value : input) {
        if (value <= 0xFFFF) {
            const auto unit = static_cast<std::uint16_t>(value);
            output.push_back(static_cast<std::uint8_t>(unit & 0xFF));
            output.push_back(static_cast<std::uint8_t>(unit >> 8));
        } else {
            value -= 0x10000;
            const auto high = static_cast<std::uint16_t>(0xD800 + (value >> 10));
            const auto low = static_cast<std::uint16_t>(0xDC00 + (value & 0x3FF));
            for (const auto unit : {high, low}) {
                output.push_back(static_cast<std::uint8_t>(unit & 0xFF));
                output.push_back(static_cast<std::uint8_t>(unit >> 8));
            }
        }
    }
    return output;
}

std::u32string fold_case(std::u32string_view input) {
    std::u32string output;
    output.reserve(input.size());
    std::transform(input.begin(), input.end(), std::back_inserter(output), folded);
    return output;
}

std::string decode_text_bytes(const std::vector<std::uint8_t>& input) {
    if (input.size() >= 2 && input[0] == 0xFF && input[1] == 0xFE) {
        return encode_utf8(decode_utf16le(input, 2));
    }
    if (input.size() >= 2 && input[0] == 0xFE && input[1] == 0xFF) {
        std::u32string decoded;
        for (std::size_t i = 2; i + 1 < input.size(); i += 2) {
            decoded.push_back((static_cast<char32_t>(input[i]) << 8) | input[i + 1]);
        }
        return encode_utf8(decoded);
    }
    const std::size_t sample = std::min<std::size_t>(input.size(), 4096);
    std::size_t odd_zeroes = 0;
    for (std::size_t i = 1; i < sample; i += 2) {
        odd_zeroes += input[i] == 0;
    }
    if (sample >= 8 && odd_zeroes > sample / 6) {
        return encode_utf8(decode_utf16le(input));
    }
    return std::string(input.begin(), input.end());
}

std::string strip_xml(std::string_view input) {
    std::string output;
    output.reserve(input.size());
    bool inside_tag = false;
    for (const char value : input) {
        if (value == '<') {
            inside_tag = true;
            output.push_back(' ');
        } else if (value == '>') {
            inside_tag = false;
        } else if (!inside_tag) {
            output.push_back(value);
        }
    }
    output = replace_all(std::move(output), "&amp;", "&");
    output = replace_all(std::move(output), "&lt;", "<");
    output = replace_all(std::move(output), "&gt;", ">");
    output = replace_all(std::move(output), "&quot;", "\"");
    output = replace_all(std::move(output), "&apos;", "'");
    return output;
}

std::string json_escape(std::string_view input) {
    std::ostringstream output;
    for (const unsigned char value : input) {
        switch (value) {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (value < 0x20) {
                    constexpr char hex[] = "0123456789abcdef";
                    output << "\\u00" << hex[value >> 4] << hex[value & 0x0F];
                } else {
                    output << static_cast<char>(value);
                }
        }
    }
    return output.str();
}

}  // namespace diskscope

