#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

TEST_CASE("failing_case") {
    CHECK(1 + 1 == 3);
}
