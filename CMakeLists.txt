# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Keep this machine's Wi-Fi credentials outside tracked project defaults.
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.wifi.local")
    if(NOT DEFINED SDKCONFIG_DEFAULTS)
        set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
    endif()
    list(APPEND SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.wifi.local")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(e-paper-weatherdisplay)
