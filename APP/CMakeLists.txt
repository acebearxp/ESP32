cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS ../components)

set(COMPONENTS main)
list(APPEND COMPONENTS CS100A) # 暂时主工程中没用到,这里只是为了让CS100A组件编译

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(APP)