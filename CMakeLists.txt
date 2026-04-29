cmake_minimum_required(VERSION 3.10)
project(origin_cam)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Find wxWidgets
find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

# 2. Find OpenCV (Manual Path)
set(OpenCV_DIR "/Users/abc/Documents/opencv-4.5.5/build") 
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# 3. Find IXWebSocket
find_library(IX_LIB ixwebsocket PATHS /usr/local/lib REQUIRED)
include_directories(/usr/local/include)

include_directories(include)

# 4. Find Curl
find_package(CURL REQUIRED)

# 5. Source Files
set(SOURCE_FILES
    src/app_core.cpp
    src/main_frame.cpp
    src/ui_login.cpp
    src/internal_bridge.cpp
    src/video_provider.cpp
    src/bridge_service.cpp
    src/output_processor.cpp
)

add_executable(origin_cam ${SOURCE_FILES})

# 6. Linking
target_link_libraries(origin_cam 
    ${wxWidgets_LIBRARIES} 
    ${OpenCV_LIBS} 
    ${CURL_LIBRARIES}
    ${IX_LIB}
    curl
    z
    "-framework CoreFoundation"
    "-framework Security"
)


set(CPACK_GENERATOR "DragNDrop")
set(CPACK_PACKAGE_NAME "Origin Cam")
set(CPACK_PACKAGE_VENDOR "Origin")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_FILE_NAME "OriginCam-macOS")
include(CPack)