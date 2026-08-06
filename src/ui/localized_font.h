#pragma once

#include <M5Cardputer.h>

#include "core/localization.h"

namespace pd {

inline bool containsUtf8(const char* text) {
    if (text == nullptr) return false;
    while (*text != '\0') {
        if ((static_cast<unsigned char>(*text) & 0x80u) != 0) return true;
        ++text;
    }
    return false;
}

inline void setUiFont(M5Canvas& canvas, UiLanguage language) {
    if (isSimplifiedChinese(language)) {
        canvas.setFont(&fonts::efontCN_14);
    } else {
        canvas.setFont(&fonts::Font0);
    }
    canvas.setTextSize(1);
}

inline void setFontForText(M5Canvas& canvas, const char* text) {
    if (containsUtf8(text)) {
        canvas.setFont(&fonts::efontCN_14);
    } else {
        canvas.setFont(&fonts::Font0);
    }
    canvas.setTextSize(1);
}

inline void setTechnicalFont(M5Canvas& canvas) {
    canvas.setFont(&fonts::Font0);
    canvas.setTextSize(1);
}

}  // namespace pd
