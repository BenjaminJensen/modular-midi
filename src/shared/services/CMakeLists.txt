# shared/services/CMakeLists.txt

# ButtonService is a header-only template (parameterized on the pin type),
# so there is no .cpp to add as a source here.

# This allows files in 'shared' or 'services' to #include "st7789.h"
# without needing the full relative path.
target_include_directories(shared PUBLIC ${CMAKE_CURRENT_LIST_DIR})