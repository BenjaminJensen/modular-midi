#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include "shared/render/color_palette.h"
#include "shared/render/font_size.h"

// Content of one display's Label Slot (see CONTEXT.md). Fixed-size, no dynamic
// allocation - make() truncates rather than growing to fit, per docs/adr/0001.
struct Label {
    static constexpr size_t MAX_TEXT_LENGTH = 24;

    std::array<char, MAX_TEXT_LENGTH + 1> text{};
    ColorPalette color = ColorPalette::Default;
    FontSize font = FontSize::Large;

    [[nodiscard]] const char* text_cstr() const { return text.data(); }

    static Label make(std::string_view requested_text, ColorPalette color, FontSize font) {
        Label label;
        label.color = color;
        label.font = font;
        size_t copy_len = requested_text.size() < MAX_TEXT_LENGTH ? requested_text.size() : MAX_TEXT_LENGTH;
        for (size_t i = 0; i < copy_len; ++i) {
            label.text[i] = requested_text[i];
        }
        label.text[copy_len] = '\0';
        return label;
    }

    friend bool operator==(const Label& a, const Label& b) {
        return a.text == b.text && a.color == b.color && a.font == b.font;
    }
};
