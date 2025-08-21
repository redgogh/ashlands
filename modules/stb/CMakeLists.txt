# modules/stb/CMakeLists.txt
include_directories(include/stb)
add_library(stb STATIC stb_image.cpp)
target_include_directories(stb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)