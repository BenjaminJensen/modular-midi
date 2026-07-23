#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "render/mailbox.h"

namespace {
    constexpr uint8_t DISPLAY_COUNT = 4;
    using TestMailbox = Mailbox<DISPLAY_COUNT>;
}

TEST_CASE("a freshly constructed Mailbox has nothing dirty") {
    TestMailbox mailbox;
    Label out;

    for (uint8_t display_id = 0; display_id < DISPLAY_COUNT; ++display_id) {
        CHECK_FALSE(mailbox.take_if_dirty(display_id, out));
    }
}

TEST_CASE("write() makes that display's entry dirty, and it reads back what was written") {
    TestMailbox mailbox;
    Label written = Label::make("preset 1", ColorPalette::Active, FontSize::Small);

    mailbox.write(2, written);

    Label out;
    REQUIRE(mailbox.take_if_dirty(2, out));
    CHECK(out == written);
}

TEST_CASE("take_if_dirty() clears the dirty flag") {
    TestMailbox mailbox;
    mailbox.write(0, Label::make("x", ColorPalette::Default, FontSize::Large));

    Label out;
    REQUIRE(mailbox.take_if_dirty(0, out));

    CHECK_FALSE(mailbox.take_if_dirty(0, out));
}

TEST_CASE("two writes before a read: take_if_dirty() yields only the latest value") {
    TestMailbox mailbox;
    Label first = Label::make("old", ColorPalette::Default, FontSize::Large);
    Label second = Label::make("new", ColorPalette::Active, FontSize::Small);

    mailbox.write(1, first);
    mailbox.write(1, second);

    Label out;
    REQUIRE(mailbox.take_if_dirty(1, out));
    CHECK(out == second);
    CHECK_FALSE(mailbox.take_if_dirty(1, out));
}

TEST_CASE("writes to one display_id don't dirty another") {
    TestMailbox mailbox;
    mailbox.write(3, Label::make("only display 3", ColorPalette::Default, FontSize::Large));

    Label out;
    CHECK_FALSE(mailbox.take_if_dirty(0, out));
    CHECK_FALSE(mailbox.take_if_dirty(1, out));
    CHECK_FALSE(mailbox.take_if_dirty(2, out));
    CHECK(mailbox.take_if_dirty(3, out));
}

TEST_CASE("a display_id can be written and read repeatedly across cycles") {
    TestMailbox mailbox;
    Label out;

    for (int i = 0; i < 3; ++i) {
        Label written = Label::make(i == 0 ? "a" : (i == 1 ? "b" : "c"), ColorPalette::Default, FontSize::Large);
        mailbox.write(0, written);
        REQUIRE(mailbox.take_if_dirty(0, out));
        CHECK(out == written);
    }
}
