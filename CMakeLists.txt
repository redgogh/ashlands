CMAKE_MINIMUM_REQUIRED(VERSION 3.28.1...3.31)
PROJECT(ashlands)

SET(CMAKE_CXX_STANDARD 26)

INCLUDE_DIRECTORIES(./)
INCLUDE_DIRECTORIES(SYSTEM "thirdparty" "include")

ADD_SUBDIRECTORY("modules/volk")

IF (APPLE)
    LINK_DIRECTORIES("thirdparty/GLFW/libs/macOS/lib-x86_64")
ENDIF()

IF (WIN32)
    LINK_DIRECTORIES("thirdparty/GLFW/libs/win32/lib-mingw-w64")
ENDIF()

ADD_EXECUTABLE(${PROJECT_NAME}
  "main.cpp"
  "driver/render_driver.cpp"
)

TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE
  "volk"
  "glfw3"
)

IF (APPLE)
    TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE
        "-framework Cocoa"
        "-framework CoreFoundation"
        "-framework IOKit"
        "-framework CoreVideo"
    )
ENDIF()