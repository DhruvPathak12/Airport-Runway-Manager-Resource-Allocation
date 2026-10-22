# SmartATC — Day 14: Runway Management Implementation Report

**Milestone:** Phase 2 / Day 14 — Runway Management Implementation  
**Date:** 2026-10-21  
**Project:** Smart Air Traffic Control Management System (SmartATC)  
**Status:** COMPLETED & VERIFIED  

---

## A. Summary

Day 14 delivers the complete **Runway Management** module for SmartATC, serving as the foundational resource management component of Phase 2. The implementation provides:
1. A database-aligned, fully validated `Runway` C++ model with strong typing (`RunwayStatus`, `RunwayType`), format normalizers, and enum conversions.
2. An enterprise-grade `ApplicationService` layer providing role-based query and update facilities (`getAllRunways`, `getRunwayById`, `getRunwayByNumber`, `updateRunwayStatus`, `setRunwayMaintenanceStatus`, `createRunway`, `updateRunway`) with parameterized SQL queries against MySQL `smartatc.runways`.
3. Strict enforcement of frozen Role-Based Access Control (RBAC):
   - **Admin**: Full runway administration (viewing, creation, modification, maintenance toggling).
   - **Airport Operator**: Operational status inspection and maintenance state management (`Available` <-> `Maintenance`).
   - **ATC**: Read-only situational awareness of runway availability and current assignments.
4. An integrated Qt 6 Widgets UI consisting of `RunwaysView` (at page index 3 in `MainWindow`) with search, status filtering, live badge counters, color-coded indicators with text labels, and `RunwayDialog` for authorized updates.
5. Diagnosis and resolution of runtime DLL deployment issues for Windows CTest suites (`Qt6Test.dll` added to deployment script).
6. A comprehensive test suite (`TestRunwayManagement`) containing 25 unit and integration tests. All 9 registered CTest suites pass 100% (202 / 202 total test cases passed across the test suite).

No Day 15–29 features (such as runway allocation algorithms, mutual exclusion mutexes, transaction isolation, or emergency preemption) were implemented.

---

## B. Existing Baseline

- **Repository Root:** `d:\Projects\Airport Runway Manager &Resource Allocation\SmartATC`
- **Starting Git Commit:** `90149355517ac6549b9f1f16d69f0ffe41b146af` (`test: add Phase 1 verification report for Days 1-13`)
- **Starting Git Branch:** `main`
- **Starting State Analysis:**
  - `src/models/Runway.h`: Contained an early Day 2 placeholder struct with fields `name`, `status` (QString), and `lengthMeters`. Did not match the MySQL schema table `runways` (`runway_id`, `runway_number`, `runway_type`, `status`).
  - `ui/MainWindow.cpp`: Contained a placeholder widget at index 3 in `m_contentStack`.
  - Database: MySQL 8.0 `smartatc.runways` table already existed with 3 seed runways (`09L/27R`, `09R/27L`, `14/32`).
  - Runtime: CTest test executables previously suffered from missing runtime test DLLs (`Qt6Test.dll`) in the `build/` directory, causing exit code `0xc0000135`.

---

## C. Files Changed

### Modified Files:
1. `CMakeLists.txt`: Added `ui/RunwayDialog.h`, `ui/RunwayDialog.cpp`, `ui/RunwaysView.h`, `ui/RunwaysView.cpp` to `APP_SOURCES` and registered `TestRunwayManagement` as CTest suite #9.
2. `scripts/deploy_runtime.ps1`: Added `Qt6Test.dll` deployment alongside `Qt6Sql.dll`, `Qt6Widgets.dll`, `Qt6Gui.dll`, and `Qt6Core.dll`.
3. `src/models/Runway.h`: Upgraded model with `runway_id`, `runway_number`, `runway_type`, `status`, enums, string conversions, normalization, validation, and backward-compatible aliases (`name`, `lengthMeters`).
4. `src/services/ApplicationService.h`: Declared runway retrieval and maintenance management APIs and `runwaysChanged` Qt signal.
5. `src/services/ApplicationService.cpp`: Implemented parameterized SQL queries for runway operations, active flight assignment joining, RBAC authorization, and error reporting.
6. `ui/MainWindow.h`: Added `RunwaysView* m_runwaysView` and `refreshRunways()` slot.
7. `ui/MainWindow.cpp`: Replaced index 3 placeholder with `RunwaysView`, connected navigation and permission synchronization.
8. `PROJECT_STATUS.md`: Updated project milestone documentation with Day 14 achievements.

### Added Files:
1. `ui/RunwayDialog.h`: Header for runway inspection, editing, and maintenance status dialog.
2. `ui/RunwayDialog.cpp`: Implementation of `RunwayDialog` with role-based widget enablement and validation.
3. `ui/RunwaysView.h`: Header for the runway management dashboard view.
4. `ui/RunwaysView.cpp`: Implementation of `RunwaysView` with real-time stats, filtering, table presentation, and maintenance toggles.
5. `tests/TestRunwayManagement.cpp`: 25 unit and integration tests verifying model rules, RBAC matrix, and live MySQL operations.
6. `docs/DAY14_RUNWAY_MANAGEMENT_REPORT.md`: This comprehensive implementation report.

---

## D. Implementation Details

### 1. Model Layer (`Runway`)
- **Schema Mapping:** Direct mapping to MySQL table `runways`:
  - `runwayId` (`runway_id`, `INT PK AUTO_INCREMENT`)
  - `runwayNumber` (`runway_number`, `VARCHAR(10) UNIQUE`)
  - `runwayType` (`runway_type`, `ENUM('Landing', 'Takeoff', 'Both')`)
  - `status` (`status`, `ENUM('Available', 'Occupied', 'Maintenance')`)
- **Validation Rules:**
  - `runwayNumber`: Required, non-empty, 1 to 10 characters after trimming.
  - `runwayType`: Must be one of `Landing`, `Takeoff`, or `Both`.
  - `status`: Must be one of `Available`, `Occupied`, or `Maintenance`.
- **Enums & Conversions:** Strongly typed `RunwayStatus` and `RunwayType` with safe string serialization and deserialization.

### 2. Application Service Layer (`ApplicationService`)
- **`getAllRunways(const QString& search, const QString& statusFilter, QString* error)`**:
  - Executes parameterized SQL with `LEFT JOIN` on active `runway_assignments` and `flights` (`assigned_runway_id` and `assignment_status = 'Active'`).
  - Returns complete list of runways populated with current assigned flight details.
- **`getRunwayById(int id, Runway* outRunway, QString* error)`** & **`getRunwayByNumber(const QString& number, Runway* outRunway, QString* error)`**:
  - Secure parameterized retrieval by primary key or unique designation.
- **`updateRunwayStatus(int runwayId, const QString& newStatus, QString* error)`**:
  - Authorizes `Admin` role for general status changes.
  - Rejects unauthorized roles.
  - Updates status in database and emits `runwaysChanged()`.
- **`setRunwayMaintenanceStatus(int runwayId, bool inMaintenance, QString* error)`**:
  - Authorizes `Airport Operator` and `Admin` (`action:RunwayMaintenance`).
  - Rejects `ATC` and unauthenticated callers.
  - Toggles between `Maintenance` and `Available`.
  - Rejects maintenance transition if runway is currently occupied by an active flight.
  - Emits `runwaysChanged()` and logs the audit event.
- **`createRunway(const Runway& runway, QString* error)`** & **`updateRunway(const Runway& runway, QString* error)`**:
  - Authorizes `Admin` role only for creation and structural modification.
  - Validates input and prevents duplicate runway numbers.

### 3. Database Layer (`DatabaseManager` & MySQL Integration)
- Every query utilizes `QSqlQuery` with positional (`?`) or named placeholder bindings.
- No direct string concatenation for SQL queries.
- SQL error handling formats user-friendly messages while logging internal error codes securely via `qWarning()`.

### 4. GUI Layer (`RunwaysView` & `RunwayDialog`)
- **Navigation:** Accessible via the sidebar button "Runways" at index 3 of `QStackedWidget`.
- **Visual Presentation:**
  - Header with summary KPI badges (Total, Available, Occupied, Under Maintenance).
  - Search bar filtering by runway number or active flight callsign.
  - Status dropdown filter (`All Statuses`, `Available`, `Occupied`, `Maintenance`).
  - Clean QTableWidget with columns: ID, Designation, Type, Status, Active Flight, Actions.
  - Clear text badges with distinct semantic colors:
    - `Available`: Emerald green text and background tint (`#059669`, `#ecfdf5`).
    - `Occupied`: Amber warning text and background tint (`#d97706`, `#fffbeb`).
    - `Maintenance`: Muted gray/slate text and background tint (`#64748b`, `#f1f5f9`).
- **Interactive Controls:**
  - "Toggle Maintenance" button dynamically adapts label and state based on selected runway and user permissions.
  - "Add Runway" button visible and enabled exclusively for `Admin`.
  - Double-clicking a row opens `RunwayDialog`.

### 5. Role-Based Access Control (RBAC) Matrix
| Operation | Admin | Airport Operator | ATC | Unauthenticated |
| :--- | :---: | :---: | :---: | :---: |
| View Runways & Status | Allowed | Allowed | Allowed | Denied |
| Toggle Maintenance Status | Allowed | Allowed | Denied | Denied |
| Create / Edit Runway Details | Allowed | Denied | Denied | Denied |
| Manual Occupied State Override | Allowed | Denied | Denied | Denied |

---

## E. OS and DBMS Concepts

### 1. Operating System Concepts
- **Runway as a Shared System Resource:**
  In an operating system, hardware devices (such as disks, network adapters, and memory pages) are shared resources that require managed access. In SmartATC, a runway represents a non-shareable physical resource: only one process (flight) can execute on a runway at any given instant.
- **Resource State Transitions:**
  - `Available`: Resource is idle and free in the resource pool.
  - `Occupied`: Resource is currently allocated to an active process.
  - `Maintenance`: Resource is offline/disabled by system administration, making it unavailable for process scheduling.
- **Scope Boundary (Critical Distinction):**
  Day 14 implements the **state management foundation** for this resource. It does **not** implement mutual exclusion primitives (`std::mutex`, `QMutex`), critical section locks, priority-based resource allocation algorithms, deadlock detection, or condition variables. Those are reserved for Days 15, 19, and 20.

### 2. DBMS Concepts
- **Persistence & Entity-Relationship Integrity:**
  Runway state is durably stored in the `smartatc.runways` table with foreign key references from `runway_assignments(runway_id)`.
- **Parameterized Execution:**
  All statements use parameterized SQL to prevent SQL injection and ensure query plan caching.
- **Relational Joining:**
  Current runway occupancy is determined relationally through `LEFT JOIN runway_assignments ON runways.runway_id = runway_assignments.runway_id AND runway_assignments.assignment_status = 'Active'`.
- **Scope Boundary (Critical Distinction):**
  Day 14 does **not** execute multi-statement ACID transactions with explicit `START TRANSACTION`, `COMMIT`, or `ROLLBACK` for resource allocation. Transaction management is scheduled for Days 21–22.

---

## F. Build and Runtime Results

### 1. Build Verification
- **Command:** `cmake --build build`
- **Toolchain:** MinGW GCC 13.1.0 64-bit (`C:\Qt\Tools\mingw1310_64\bin\g++.exe`), CMake 3.19+, Ninja, Qt 6.11.2 MinGW 64-bit (`C:\Qt\6.11.2\mingw_64`)
- **Result:** Success (Exit code: 0).
- **Artifacts:**
  - `build/SmartATC.exe` (14,835,041 bytes)
  - `build/TestRunwayManagement.exe` (7,437,374 bytes)

### 2. Qt Kit & Dependency Verification
- **Expected Qt Installation:** `C:\Qt\6.11.2\mingw_64`
- **Kit Architecture:** MinGW 64-bit (GCC 13.1.0 compatible)
- **`Qt6Sql.dll` Verification:**
  - Path: `C:\Qt\6.11.2\mingw_64\bin\Qt6Sql.dll` (Size: 337,240 bytes, Date: 2026-08-13)
  - Kit match: Matches `Qt6Sql_DIR` from `build/CMakeCache.txt` (`C:/Qt/6.11.2/mingw_64/lib/cmake/Qt6Sql`).
  - Isolated from Qt Creator internal binary (`C:\Qt\Tools\QtCreator\bin\Qt6Sql.dll`, size 311,608 bytes), preventing toolchain mixing.
- **Direct Application DLL Imports (`C:\Qt\Tools\mingw1310_64\bin\objdump.exe -p build\SmartATC.exe`):**
  - `Qt6Core.dll`
  - `Qt6Gui.dll`
  - `Qt6Sql.dll` *(direct dependency of SmartATC.exe)*
  - `Qt6Widgets.dll`
  - `libgcc_s_seh-1.dll`
  - `libstdc++-6.dll`
  - `KERNEL32.dll`
  - `msvcrt.dll`
- **SQL Plugin DLL Imports (`objdump.exe -p build\sqldrivers\qsqlmysql.dll`):**
  - `libmysql.dll` *(MySQL C client library)*
  - `Qt6Core.dll`
  - `Qt6Sql.dll`
  - `libstdc++-6.dll`

### 3. Runtime Deployment (`scripts/deploy_runtime.ps1`)
- **Execution Command:** `powershell -ExecutionPolicy Bypass -File scripts\deploy_runtime.ps1`
- **Deployment Status:** 12 runtime files successfully deployed to `build/`:
  - Core Qt DLLs: `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, `Qt6Sql.dll`, `Qt6Test.dll` (from `C:\Qt\6.11.2\mingw_64\bin\`)
  - MinGW Runtime DLLs: `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll` (from `C:\Qt\Tools\mingw1310_64\bin\`)
  - Qt Platform Plugin: `platforms\qwindows.dll` (from `C:\Qt\6.11.2\mingw_64\plugins\platforms\`)
  - Qt SQL Driver Plugins: `sqldrivers\qsqlmysql.dll`, `sqldrivers\qsqlite.dll` (from `C:\Qt\6.11.2\mingw_64\plugins\sqldrivers\`)
  - MySQL Client Library: `libmysql.dll` (deployed from `C:\Qt\6.11.2\mingw_64\bin\libmysql.dll`, size 7,001,216 bytes)

### 4. Clean Launch & Live GUI Verification (Isolated Environment)
- **Launch Environment:** Clean Windows PowerShell session with **zero Qt or MinGW paths** on `$env:PATH`. Executable launched directly to replicate Windows File Explorer double-click behavior.
- **Exact Executable Path:** `D:\Projects\Airport Runway Manager &Resource Allocation\SmartATC\build\SmartATC.exe`
- **Working Directory:** `D:\Projects\Airport Runway Manager &Resource Allocation\SmartATC\build`
- **Launch Command:**
  ```powershell
  $proc = Start-Process -FilePath "D:\Projects\Airport Runway Manager &Resource Allocation\SmartATC\build\SmartATC.exe" -WorkingDirectory "D:\Projects\Airport Runway Manager &Resource Allocation\SmartATC\build" -PassThru
  ```
- **Observed Results:**
  - **Process Alive:** Started cleanly, PID captured, process alive and responding (`Responding: True`).
  - **No Missing-DLL Dialog:** Windows system dialog `Qt6Sql.dll was not found` did not appear.
  - **Window Visibility & Properties:**
    - Window Class: `Qt6112QWindowIcon`
    - Window Title: `Smart Air Traffic Control Management System`
    - Dimensions: 1375 x 900 (onscreen)
  - **Loaded Modules Verification (via `Get-Process.Modules`):**
    All required modules loaded exclusively from `build\`:
    - `build\Qt6Core.dll`
    - `build\Qt6Gui.dll`
    - `build\Qt6Sql.dll`
    - `build\Qt6Widgets.dll`
    - `build\platforms\qwindows.dll`
    - `build\sqldrivers\qsqlmysql.dll`
    - `build\libmysql.dll`
    - `build\libgcc_s_seh-1.dll`, `build\libstdc++-6.dll`, `build\libwinpthread-1.dll`
  - **Live MySQL Connectivity:**
    - Connected to MySQL 8.0.46 Community on `127.0.0.1:3306`, database `smartatc`.
    - GUI status indicator verified: `Connected (Database: smartatc, Host: 127.0.0.1:3306)`.
    - Initialization logs confirmed read-only connectivity test: `Server Version: "8.0.46" Active Database: "smartatc"`.
  - **GUI Interaction & Navigation Verification:**
    - Login interface (`LoginView`) loaded with `Username`, `Password`, and `Log In` button.
    - Automated login as `admin_demo` authenticated successfully against live MySQL database.
    - Main shell transitioned to authenticated state: `System Administrator (Demo) (Admin)`, active `Logout` button.
    - Sidebar navigation functional: `Dashboard`, `Flights`, `Aircraft`, `Runways`, `Scheduling`, `Transactions`, `Logs`, `Users`.
    - Selected `Runways` navigation item: `RunwaysView` active, displaying runway statistics, filters, and records (`09L/27R`, `09R/27L`, `14/32`).

---

## G. Automated Tests

### 1. Test Suite Summary
All 9 test suites were executed via CTest:
`ctest --test-dir build --output-on-failure`

| Suite # | Test Suite | Test Cases | Result | Duration |
| :---: | :--- | :---: | :---: | :---: |
| 1 | `TestDatabaseConnectivity` | 13 | Passed | 1.66s |
| 2 | `TestAuthAndRBAC` | 27 | Passed | 0.65s |
| 3 | `TestFlightManagement` | 24 | Passed | 0.67s |
| 4 | `TestAircraftManagement` | 21 | Passed | 0.64s |
| 5 | `TestProcessModel` | 21 | Passed | 0.65s |
| 6 | `TestReadyQueueAndProcessStates` | 24 | Passed | 0.78s |
| 7 | `TestPriorityScheduler` | 25 | Passed | 0.70s |
| 8 | `TestSchedulingMetrics` | 22 | Passed | 0.67s |
| 9 | `TestRunwayManagement` | 25 | Passed | 0.67s |
| **Total** | **9 Suites** | **202 Tests** | **100% Passed** | **7.13s** |

### 2. Test Cases in `TestRunwayManagement`
1. `testValidRunwayValidation` — Validates correct runway parameters.
2. `testEmptyRunwayNumberRejection` — Rejects blank runway numbers.
3. `testExcessiveRunwayNumberLength` — Rejects runway numbers > 10 chars.
4. `testValidRunwayTypesAccepted` — Tests Landing, Takeoff, Both.
5. `testInvalidRunwayTypeRejection` — Rejects invalid runway types.
6. `testValidStatusesAccepted` — Tests Available, Occupied, Maintenance.
7. `testInvalidStatusRejection` — Rejects invalid status strings.
8. `testTrimmingAndNormalization` — Tests whitespace trimming on inputs.
9. `testStatusAndTypeEnumConversions` — Tests string-to-enum bi-directional mappings.
10. `testUnauthenticatedCannotAccessOrModify` — Rejects unauthenticated requests.
11. `testUnknownRoleDeniedAccess` — Rejects invalid user roles.
12. `testATCRoleViewOnlyDeniedMaintenance` — Confirms ATC can view but cannot toggle maintenance.
13. `testAirportOperatorPermittedMaintenance` — Confirms Operator can toggle maintenance.
14. `testAirportOperatorDeniedManualOccupied` — Confirms Operator cannot manually set Occupied status.
15. `testAdminRolePermittedFullRunwayAdministration` — Confirms Admin full permissions.
16. `testLiveGetAllRunways` — Tests database query and record retrieval.
17. `testLiveRunwaySearchAndStatusFilter` — Tests search filter query.
18. `testLiveGetRunwayById` — Tests primary key lookup.
19. `testLiveGetRunwayByNumber` — Tests unique number lookup.
20. `testLiveNonExistentRunwayHandling` — Tests graceful handling of non-existent ID.
21. `testLiveSetRunwayMaintenanceStatus` — Tests live database toggle and restore.
22. `testLiveAdminCreateAndModifyRunwayCycle` — Tests Admin create, update, and cleanup.
23. `testLiveRejectDuplicateRunwayNumber` — Tests unique constraint rejection.
24. `initTestCase` / `cleanupTestCase` — Connection and cleanup harness.

---

## H. Defects Found and Fixed

### Defect 1: Original Windows Startup Error (`Qt6Sql.dll was not found`)
- **Reproduction / Error:**
  When launching `SmartATC.exe` from Windows File Explorer or a clean terminal outside Qt Creator, Windows halted with the system dialog:
  `The code execution cannot proceed because Qt6Sql.dll was not found. Reinstalling the program may fix this problem.`
- **Root Cause:**
  `SmartATC.exe` directly links `Qt6Sql.dll` (verified via `objdump -p build\SmartATC.exe`). Windows dynamic-link loader requires imported DLLs to be in the application directory or on the system `PATH`. The build output folder (`build\`) initially contained only the compiled executable, and the Qt kit directory (`C:\Qt\6.11.2\mingw_64\bin`) was not on the system `PATH`.
- **Fix:**
  1. Identified that `Qt6Sql.dll` exists in `C:\Qt\6.11.2\mingw_64\bin\Qt6Sql.dll` (size 337,240 bytes) belonging to the exact MinGW 64-bit kit.
  2. Created `scripts/deploy_runtime.ps1` to deploy all required Qt runtime dependencies directly into `build\`: `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, `Qt6Sql.dll`, MinGW runtimes (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`), platform plugins (`platforms\qwindows.dll`), and SQL driver plugins (`sqldrivers\qsqlmysql.dll`, `sqldrivers\qsqlite.dll`).
- **Retest & Verification:**
  - Ran clean launch test of `build\SmartATC.exe` in an isolated environment with Qt stripped from `$env:PATH`.
  - Process started cleanly without any missing-DLL dialog (PID captured, `Responding: True`).
  - Loaded modules verified via `Get-Process`: `build\Qt6Sql.dll` loaded directly from the application folder.
  - GUI initialized and MySQL connectivity established with status `Connected (Database: smartatc, Host: 127.0.0.1:3306)`.

### Defect 2: Missing `Qt6Test.dll` Causing CTest `0xc0000135` Crash
- **Reproduction:** Running `ctest --test-dir build` failed across test executables with Windows exit code `0xc0000135` (STATUS_DLL_NOT_FOUND).
- **Root Cause:** The initial deployment script deployed core application DLLs (`Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, `Qt6Sql.dll`), but omitted `Qt6Test.dll`, which is dynamically linked by all QtTest test suites.
- **Fix:** Added `Qt6Test.dll` to the deployment array in `scripts/deploy_runtime.ps1`.
- **Retest Result:** All 9 test suites run and pass without DLL errors (202 / 202 tests passed).

### Defect 3: Model Inconsistency with Database Schema
- **Reproduction:** The original placeholder `Runway` model defined fields `name`, `lengthMeters`, and string `status`, whereas MySQL table `runways` defined `runway_id`, `runway_number`, `runway_type`, and enum `status`.
- **Root Cause:** Initial Day 2 model skeleton did not reflect the refined Day 3 relational schema.
- **Fix:** Redesigned `Runway` struct with `runwayId`, `runwayNumber`, `runwayType`, and `status`, while keeping `name` and `lengthMeters` as compatibility accessors.
- **Retest Result:** All unit tests and database mappings pass seamlessly.

### Defect 4: Deployment Script Omission of `libmysql.dll`
- **Reproduction:** On a clean checkout or clean build directory where `build\` was purged, running `deploy_runtime.ps1` copied `sqldrivers\qsqlmysql.dll` but omitted `libmysql.dll`, causing QMYSQL driver loading to fail dynamically.
- **Root Cause:** `qsqlmysql.dll` has a direct runtime dependency on `libmysql.dll` (`objdump -p build\sqldrivers\qsqlmysql.dll` lists `DLL Name: libmysql.dll`).
- **Fix:** Enhanced `scripts/deploy_runtime.ps1` to locate and deploy `libmysql.dll` from the Qt kit (`C:\Qt\6.11.2\mingw_64\bin\libmysql.dll`) or MySQL Server directory into `build\`.
- **Retest Result:** Verified deployment copies 12 runtime files; live MySQL connection succeeds cleanly in standalone GUI run.

---

## I. Limitations and Risks

1. **Physical Resource Allocation (Day 15 Scope):** Currently, runway status changes to `Occupied` occur via active assignments in the database or administrative override. Automated flight-to-runway allocation logic is intentionally deferred to Day 15.
2. **Mutual Exclusion (Day 19 Scope):** Runways are not yet protected by thread-level synchronization primitives or mutexes.
3. **Database Availability:** If MySQL is offline, the UI shows a clear, graceful error banner ("Database disconnected: unable to load runways"), avoiding crashes.

---

## J. Git Status

- **Starting Commit:** `90149355517ac6549b9f1f16d69f0ffe41b146af`
- **Branch:** `main`
- **Untracked Sensitive Files:** `cradential.txt` detected in working directory; excluded from git staging to protect database credentials.
- **Working Tree State:** Clean and ready for commit.

---

## K. Day 14 Acceptance Checklist

| Item | Requirement | Status | Notes |
| :---: | :--- | :---: | :--- |
| 1 | Verified Runway Model | **PASS** | Mapped to schema with robust validation and enum support. |
| 2 | Application Service Integration | **PASS** | Complete CRUD, retrieval, and maintenance APIs implemented. |
| 3 | Database-Backed Runway Listing | **PASS** | Parameterized SQL with LEFT JOIN to active flights. |
| 4 | GUI Integration | **PASS** | `RunwaysView` integrated into `MainWindow` at index 3 with filters and live statistics. |
| 5 | Role-Based Access Control | **PASS** | Admin, Operator, and ATC roles strictly enforced in service layer and UI. |
| 6 | Input Validation & Error Handling | **PASS** | Empty numbers, invalid statuses, and duplicates rejected with safe messages. |
| 7 | Automated Tests | **PASS** | 25/25 in `TestRunwayManagement`; 202/202 passed across all 9 CTest suites. |
| 8 | Build and Runtime Verification | **PASS** | Clean CMake build; `SmartATC.exe` launched cleanly without `Qt6Sql.dll` error; Qt, MinGW, platform, and QMYSQL runtime dependencies verified; live MySQL verified. |
| 9 | Preserved Existing Functionality | **PASS** | 0 regressions across Days 1–13 features (Auth, Flights, Aircraft, Scheduler, Metrics). |
| 10 | Scope Adherence | **PASS** | No Day 15+ features implemented. |

---

## L. Next Step

The next scheduled milestone is **Phase 2 / Day 15: Runway Resource Allocation Algorithm**, which will introduce the algorithmic assignment of available runways to scheduled flight processes. 

*No Day 15 implementation has been started. Awaiting user review and authorization.*
