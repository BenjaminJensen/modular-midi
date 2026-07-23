#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <string_view>
#include "render/label.h"

TEST_CASE("make() stores text under the max length verbatim, null-terminated") {
    Label label = Label::make("hello", ColorPalette::Default, FontSize::Large);

    CHECK(std::string_view(label.text_cstr()) == "hello");
}

TEST_CASE("make() truncates text longer than MAX_TEXT_LENGTH") {
    std::string_view too_long = "this label text is much too long to fit";
    REQUIRE(too_long.size() > Label::MAX_TEXT_LENGTH);

    Label label = Label::make(too_long, ColorPalette::Default, FontSize::Large);

    CHECK(std::string_view(label.text_cstr()) == too_long.substr(0, Label::MAX_TEXT_LENGTH));
    CHECK(std::string_view(label.text_cstr()).size() == Label::MAX_TEXT_LENGTH);
}

TEST_CASE("make() stores the given color and font") {
    Label label = Label::make("x", ColorPalette::Active, FontSize::Small);

    CHECK(label.color == ColorPalette::Active);
    CHECK(label.font == FontSize::Small);
}

TEST_CASE("a default-constructed Label has empty text") {
    Label label;

    CHECK(std::string_view(label.text_cstr()).empty());
}

TEST_CASE("equal Labels compare equal, differing ones don't") {
    Label a = Label::make("preset 1", ColorPalette::Active, FontSize::Small);
    Label b = Label::make("preset 1", ColorPalette::Active, FontSize::Small);
    Label c = Label::make("preset 2", ColorPalette::Active, FontSize::Small);

    CHECK(a == b);
    CHECK_FALSE(a == c);
}
