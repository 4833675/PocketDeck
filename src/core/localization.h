#pragma once

#include <cstdint>

namespace pd {

enum class UiLanguage : uint8_t {
    English = 0,
    SimplifiedChinese = 1,
};

constexpr bool validUiLanguage(UiLanguage language) {
    return language == UiLanguage::English ||
           language == UiLanguage::SimplifiedChinese;
}

constexpr bool isSimplifiedChinese(UiLanguage language) {
    return language == UiLanguage::SimplifiedChinese;
}

constexpr UiLanguage toggledUiLanguage(UiLanguage language) {
    return isSimplifiedChinese(language) ? UiLanguage::English
                                         : UiLanguage::SimplifiedChinese;
}

constexpr const char* localized(UiLanguage language, const char* english,
                                const char* simplifiedChinese) {
    return isSimplifiedChinese(language) ? simplifiedChinese : english;
}

}  // namespace pd
