#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <string_view>
#include "services/button_payload.h"

TEST_CASE("pack() tags the event as EventType::Button") {
    Event e = ButtonPayload::pack(2, ButtonEventState::Pressed);

    CHECK(e.type() == EventType::Button);
}

TEST_CASE("unpack() round-trips the id packed in") {
    Event e = ButtonPayload::pack(3, ButtonEventState::Released);

    ButtonPayload payload = ButtonPayload::unpack(e.payload());

    CHECK(payload.id == 3);
}

TEST_CASE("unpack() round-trips each ButtonEventState") {
    std::array states = {
        ButtonEventState::Pressed,
        ButtonEventState::Released,
        ButtonEventState::LongPressed,
        ButtonEventState::DoubleTapped,
    };

    for (ButtonEventState state : states) {
        Event e = ButtonPayload::pack(1, state);
        ButtonPayload payload = ButtonPayload::unpack(e.payload());

        CHECK(payload.state == state);
    }
}

TEST_CASE("to_string() matches Button's own log wording") {
    CHECK(std::string_view(to_string(ButtonEventState::Pressed)) == "pressed");
    CHECK(std::string_view(to_string(ButtonEventState::Released)) == "released");
    CHECK(std::string_view(to_string(ButtonEventState::LongPressed)) == "long press");
    CHECK(std::string_view(to_string(ButtonEventState::DoubleTapped)) == "double tap");
}

TEST_CASE("to_label_string() renders the enum member name verbatim") {
    CHECK(std::string_view(to_label_string(ButtonEventState::Pressed)) == "Pressed");
    CHECK(std::string_view(to_label_string(ButtonEventState::Released)) == "Released");
    CHECK(std::string_view(to_label_string(ButtonEventState::LongPressed)) == "LongPressed");
    CHECK(std::string_view(to_label_string(ButtonEventState::DoubleTapped)) == "DoubleTapped");
}
