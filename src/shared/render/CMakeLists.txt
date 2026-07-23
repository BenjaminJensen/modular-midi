# shared/render/CMakeLists.txt

# Label/Mailbox are header-only templates, so there is no .cpp to add as a source here.

# This allows files in 'shared' or 'render' to #include "mailbox.h"
# without needing the full relative path.
target_include_directories(shared PUBLIC ${CMAKE_CURRENT_LIST_DIR})
