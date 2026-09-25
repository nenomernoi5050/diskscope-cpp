#include "diskscope/matcher.hpp"

#include "diskscope/text.hpp"

#include <algorithm>

namespace diskscope {

Matcher::Matcher(std::vector<std::string> keywords) : keywords_(std::move(keywords)) {
    folded_keywords_.reserve(keywords_.size());
    for (const auto& keyword : keywords_) {
        folded_keywords_.push_back(fold_case(decode_utf8(keyword)));
    }
}

std::vector<TextMatch> Matcher::match(std::string_view utf8_text) const {
    const auto text = fold_case(decode_utf8(utf8_text));
    std::vector<TextMatch> matches;
    for (std::size_t i = 0; i < folded_keywords_.size(); ++i) {
        const auto& keyword = folded_keywords_[i];
        if (keyword.empty()) {
            continue;
        }
        std::size_t position = 0;
        while ((position = text.find(keyword, position)) != std::u32string::npos) {
            matches.push_back({keywords_[i], position});
            position += std::max<std::size_t>(1, keyword.size());
        }
    }
    std::sort(matches.begin(), matches.end(), [](const auto& left, const auto& right) {
        return left.character_offset < right.character_offset;
    });
    return matches;
}

const std::vector<std::string>& Matcher::keywords() const noexcept {
    return keywords_;
}

}  // namespace diskscope

