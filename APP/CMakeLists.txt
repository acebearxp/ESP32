cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS ../components)

set(COMPONENTS main)
list(APPEND COMPONENTS CS100A)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(APP)