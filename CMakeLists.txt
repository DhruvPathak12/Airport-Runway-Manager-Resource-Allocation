cmake_minimum_required(VERSION 3.19)

project(SmartATC
    VERSION 0.1.0
    DESCRIPTION "Smart Air Traffic Control Management System"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6 detection — requires Qt6_DIR or CMAKE_PREFIX_PATH to be set
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql)

add_executable(SmartATC
    src/main.cpp
)

target_link_libraries(SmartATC
    PRIVATE
    Qt6::Widgets
    Qt6::Sql
)

target_include_directories(SmartATC
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
