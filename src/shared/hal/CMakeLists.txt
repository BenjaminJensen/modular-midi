# shared/hal/CMakeLists.txt

# Platform-portable, concept-based peripheral drivers (no SDK dependency).
add_subdirectory(drivers)

# You can add logic here later if you support multiple chips (e.g., if(RP2040)...)
add_subdirectory(rp2350)