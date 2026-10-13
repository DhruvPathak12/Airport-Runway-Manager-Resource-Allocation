cmake_minimum_required(VERSION 3.19)

project(SmartATC
    VERSION 0.2.0
    DESCRIPTION "Smart Air Traffic Control Management System"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6 — requires CMAKE_PREFIX_PATH pointing to the Qt kit (set at configure time)
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql)

# ─────────────────────────────────────────────────────────────────────────────
# Source files
# ─────────────────────────────────────────────────────────────────────────────

set(APP_SOURCES
    # Entry point
    src/main.cpp

    # UI layer (ui/)
    ui/MainWindow.h
    ui/MainWindow.cpp

    # Service layer (src/services/)
    src/services/ApplicationService.h
    src/services/ApplicationService.cpp

    # Database layer (src/database/)
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp

    # Scheduler layer (src/scheduler/)
    src/scheduler/Scheduler.h
    src/scheduler/Scheduler.cpp

    # Resource layer (src/resource/)
    src/resource/ResourceManager.h
    src/resource/ResourceManager.cpp

    # Utils (src/utils/)
    src/utils/PasswordUtils.h
    src/utils/PasswordUtils.cpp

    # Models (src/models/) — header-only structs, included via target_include_directories
    src/models/Flight.h
    src/models/Aircraft.h
    src/models/Runway.h
    src/models/User.h
)

add_executable(SmartATC ${APP_SOURCES})

target_link_libraries(SmartATC
    PRIVATE
    Qt6::Widgets
    Qt6::Sql
)

# Include paths — allow #include "models/Flight.h", "ui/MainWindow.h", etc.
target_include_directories(SmartATC
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/ui
    ${CMAKE_SOURCE_DIR}
)

# ─────────────────────────────────────────────────────────────────────────────
# Testing
# ─────────────────────────────────────────────────────────────────────────────
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(TestDatabaseConnectivity
    tests/TestDatabaseConnectivity.cpp
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
)

target_link_libraries(TestDatabaseConnectivity
    PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)

target_include_directories(TestDatabaseConnectivity
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME TestDatabaseConnectivity COMMAND TestDatabaseConnectivity)

