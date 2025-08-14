cmake_minimum_required(VERSION 3.31)
project(ashlands)

set(CMAKE_CXX_STANDARD 26)

include_directories(SYSTEM thirdparty)

add_subdirectory(modules/volk)

link_directories(
  thirdparty/GLFW/libs/macOS/lib-x86_64
)

add_executable(${PROJECT_NAME}
  main.cpp
  driver/render_driver.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  "volk"
  "glfw3"
  "-framework Cocoa"
  "-framework CoreFoundation"
  "-framework IOKit"
  "-framework CoreVideo"
)