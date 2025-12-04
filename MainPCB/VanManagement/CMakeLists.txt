# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "common_includes"
    "communications"
    "peripherals_devices"
    "utils"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Removed MINIMAL_BUILD to include all BLE components
project(VanManagement)
idf_build_set_property(COMPILE_OPTIONS "-DHAS_ETH_W5500" APPEND)