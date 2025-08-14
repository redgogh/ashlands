# modules/volk/CMakeLists.txt
include_directories(include/volk)
add_library(volk STATIC volk.c)
target_include_directories(volk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)