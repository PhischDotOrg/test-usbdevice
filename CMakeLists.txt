cmake_minimum_required(VERSION 3.12)
project(test-usbdevice)

###############################################################################
# Find LibUSB.
#
# This requires the cmake-modules/ subdirectory to contain a LibUSB.cmake file.
#
# See https://github.com/PhischDotOrg/cmake-modules
###############################################################################
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake-modules)
find_package(LibUSB REQUIRED)

###############################################################################
# Global Compile Options
###############################################################################
include_directories(/usr/local/include)

add_compile_options("-Wall")
add_compile_options("-Werror")

###############################################################################
# Executable
###############################################################################
set(TARGET_NAME ${CMAKE_PROJECT_NAME})
set(TARGET_SRC
    main.cpp
)
add_executable(${TARGET_NAME} ${TARGET_SRC})
target_include_directories(${TARGET_NAME} PRIVATE
    ${LIBUSB_1_INCLUDE_DIRS}
)
target_compile_definitions(${TARGET_NAME} PRIVATE
    ${LIBUSB_1_DEFINITIONS}
)
target_link_libraries(${TARGET_NAME}
    ${LIBUSB_1_LIBRARIES}
)