# shared/services/CMakeLists.txt

target_sources(shared PRIVATE
    button_service.cpp
)

# This allows files in 'shared' or 'services' to #include "st7789.h" 
# without needing the full relative path.
target_include_directories(shared PUBLIC ${CMAKE_CURRENT_LIST_DIR})