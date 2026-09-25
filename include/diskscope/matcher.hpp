#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace diskscope {

struct TextMatch {
    std::string keyword;
    std::size_t character_offset{};
};

class Matcher {
public:
    explicit Matcher(std::vector<std::string> keywords);

    [[nodiscard]] std::vector<TextMatch> match(std::string_view utf8_text) const;
    [[nodiscard]] const std::vector<std::string>& keywords() const noexcept;

private:
    std::vector<std::string> keywords_;
    std::vector<std::u32string> folded_keywords_;
};

}  // namespace diskscope

