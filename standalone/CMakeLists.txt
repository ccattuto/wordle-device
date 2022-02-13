cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS
    $ENV{IDF_PATH}/examples/common_components/led_strip
    components/lwjson)

project(main)
