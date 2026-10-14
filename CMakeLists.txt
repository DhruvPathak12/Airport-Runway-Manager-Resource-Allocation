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
    ui/LoginView.h
    ui/LoginView.cpp
    ui/FlightDialog.h
    ui/FlightDialog.cpp
    ui/FlightsView.h
    ui/FlightsView.cpp
    ui/AircraftDialog.h
    ui/AircraftDialog.cpp
    ui/AircraftView.h
    ui/AircraftView.cpp

    # Service layer (src/services/)
    src/services/ApplicationService.h
    src/services/ApplicationService.cpp

    # Database layer (src/database/)
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp

    # Scheduler layer (src/scheduler/)
    src/scheduler/Scheduler.h
    src/scheduler/Scheduler.cpp
    src/scheduler/ReadyQueue.h
    src/scheduler/ReadyQueue.cpp

    # Resource layer (src/resource/)
    src/resource/ResourceManager.h
    src/resource/ResourceManager.cpp

    # Utils (src/utils/)
    src/utils/PasswordUtils.h
    src/utils/PasswordUtils.cpp

    # Models (src/models/)
    src/models/Flight.h
    src/models/Aircraft.h
    src/models/Runway.h
    src/models/User.h
    src/models/Session.h
    src/models/Session.cpp
    src/models/Process.h
    src/models/Process.cpp
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

add_executable(TestAuthAndRBAC
    tests/TestAuthAndRBAC.cpp
    src/utils/PasswordUtils.h
    src/utils/PasswordUtils.cpp
    src/models/Session.h
    src/models/Session.cpp
    src/models/Process.h
    src/models/Process.cpp
    src/services/ApplicationService.h
    src/services/ApplicationService.cpp
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/scheduler/Scheduler.h
    src/scheduler/Scheduler.cpp
    src/scheduler/ReadyQueue.h
    src/scheduler/ReadyQueue.cpp
    src/resource/ResourceManager.h
    src/resource/ResourceManager.cpp
)

target_link_libraries(TestAuthAndRBAC
    PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)

target_include_directories(TestAuthAndRBAC
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME TestAuthAndRBAC COMMAND TestAuthAndRBAC)

add_executable(TestFlightManagement
    tests/TestFlightManagement.cpp
    src/utils/PasswordUtils.h
    src/utils/PasswordUtils.cpp
    src/models/Session.h
    src/models/Session.cpp
    src/models/Flight.h
    src/models/Process.h
    src/models/Process.cpp
    src/services/ApplicationService.h
    src/services/ApplicationService.cpp
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/scheduler/Scheduler.h
    src/scheduler/Scheduler.cpp
    src/scheduler/ReadyQueue.h
    src/scheduler/ReadyQueue.cpp
    src/resource/ResourceManager.h
    src/resource/ResourceManager.cpp
)

target_link_libraries(TestFlightManagement
    PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)

target_include_directories(TestFlightManagement
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME TestFlightManagement COMMAND TestFlightManagement)

add_executable(TestAircraftManagement
    tests/TestAircraftManagement.cpp
    src/utils/PasswordUtils.h
    src/utils/PasswordUtils.cpp
    src/models/Session.h
    src/models/Session.cpp
    src/models/Aircraft.h
    src/models/Flight.h
    src/models/Process.h
    src/models/Process.cpp
    src/services/ApplicationService.h
    src/services/ApplicationService.cpp
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/scheduler/Scheduler.h
    src/scheduler/Scheduler.cpp
    src/scheduler/ReadyQueue.h
    src/scheduler/ReadyQueue.cpp
    src/resource/ResourceManager.h
    src/resource/ResourceManager.cpp
)

target_link_libraries(TestAircraftManagement
    PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)

target_include_directories(TestAircraftManagement
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME TestAircraftManagement COMMAND TestAircraftManagement)

add_executable(TestProcessModel
    tests/TestProcessModel.cpp
    src/utils/PasswordUtils.h
    src/utils/PasswordUtils.cpp
    src/models/Session.h
    src/models/Session.cpp
    src/models/Aircraft.h
    src/models/Flight.h
    src/models/Process.h
    src/models/Process.cpp
    src/services/ApplicationService.h
    src/services/ApplicationService.cpp
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/scheduler/Scheduler.h
    src/scheduler/Scheduler.cpp
    src/scheduler/ReadyQueue.h
    src/scheduler/ReadyQueue.cpp
    src/resource/ResourceManager.h
    src/resource/ResourceManager.cpp
)

target_link_libraries(TestProcessModel
    PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)

target_include_directories(TestProcessModel
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME TestProcessModel COMMAND TestProcessModel)

add_executable(TestReadyQueueAndProcessStates
    tests/TestReadyQueueAndProcessStates.cpp
    src/utils/PasswordUtils.h
    src/utils/PasswordUtils.cpp
    src/models/Session.h
    src/models/Session.cpp
    src/models/Aircraft.h
    src/models/Flight.h
    src/models/Process.h
    src/models/Process.cpp
    src/services/ApplicationService.h
    src/services/ApplicationService.cpp
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/scheduler/Scheduler.h
    src/scheduler/Scheduler.cpp
    src/scheduler/ReadyQueue.h
    src/scheduler/ReadyQueue.cpp
    src/resource/ResourceManager.h
    src/resource/ResourceManager.cpp
)

target_link_libraries(TestReadyQueueAndProcessStates
    PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)

target_include_directories(TestReadyQueueAndProcessStates
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME TestReadyQueueAndProcessStates COMMAND TestReadyQueueAndProcessStates)

add_executable(TestPriorityScheduler
    tests/TestPriorityScheduler.cpp
    src/utils/PasswordUtils.h
    src/utils/PasswordUtils.cpp
    src/models/Session.h
    src/models/Session.cpp
    src/models/Aircraft.h
    src/models/Flight.h
    src/models/Process.h
    src/models/Process.cpp
    src/services/ApplicationService.h
    src/services/ApplicationService.cpp
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/scheduler/Scheduler.h
    src/scheduler/Scheduler.cpp
    src/scheduler/ReadyQueue.h
    src/scheduler/ReadyQueue.cpp
    src/resource/ResourceManager.h
    src/resource/ResourceManager.cpp
)

target_link_libraries(TestPriorityScheduler
    PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)

target_include_directories(TestPriorityScheduler
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME TestPriorityScheduler COMMAND TestPriorityScheduler)

add_executable(TestSchedulingMetrics
    tests/TestSchedulingMetrics.cpp
    src/utils/PasswordUtils.h
    src/utils/PasswordUtils.cpp
    src/models/Session.h
    src/models/Session.cpp
    src/models/Aircraft.h
    src/models/Flight.h
    src/models/Process.h
    src/models/Process.cpp
    src/services/ApplicationService.h
    src/services/ApplicationService.cpp
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/scheduler/Scheduler.h
    src/scheduler/Scheduler.cpp
    src/scheduler/ReadyQueue.h
    src/scheduler/ReadyQueue.cpp
    src/resource/ResourceManager.h
    src/resource/ResourceManager.cpp
)

target_link_libraries(TestSchedulingMetrics
    PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)

target_include_directories(TestSchedulingMetrics
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME TestSchedulingMetrics COMMAND TestSchedulingMetrics)

