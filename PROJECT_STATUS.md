# PROJECT STATUS

---

## DAY 1 — Repository + CMake + Qt Foundation ✅ COMPLETE

### Files Created
- CMakeLists.txt
- src/main.cpp
- README.md
- .gitignore
- db.conf.example
- AGENTS.md
- PROJECT_SPEC.md
- docs/.gitkeep, sql/.gitkeep, tests/.gitkeep, ui/.gitkeep
- src/models/.gitkeep, src/scheduler/.gitkeep, src/resource/.gitkeep
- src/database/.gitkeep, src/services/.gitkeep, src/utils/.gitkeep

### Tests
1. CMake configure — PASS
2. CMake build — PASS
3. Application launch — PASS
4. .gitignore verification — PASS
5. Git verification — PASS

### Git
- Branch: main
- Commits: c196c39, 4908d64

### GitHub Repository
- Repository: Airport Runway Manager & Resource Allocation (PRIVATE)
- Link: https://github.com/DhruvPathak12/Airport-Runway-Manager-Resource-Allocation
- Push Status: PUSHED SUCCESSFULLY

---

## DAY 2 — Technical Architecture + Project Skeleton ✅ COMPLETE

### Files Created/Modified
- CMakeLists.txt (updated — all Day 2 sources added)
- src/main.cpp (updated — uses ApplicationService + MainWindow)
- ui/MainWindow.h (NEW)
- ui/MainWindow.cpp (NEW)
- src/services/ApplicationService.h (NEW)
- src/services/ApplicationService.cpp (NEW)
- src/database/DatabaseManager.h (NEW)
- src/database/DatabaseManager.cpp (NEW)
- src/scheduler/Scheduler.h (NEW)
- src/scheduler/Scheduler.cpp (NEW)
- src/resource/ResourceManager.h (NEW)
- src/resource/ResourceManager.cpp (NEW)
- src/utils/PasswordUtils.h (NEW)
- src/utils/PasswordUtils.cpp (NEW)
- src/models/Flight.h (NEW)
- src/models/Aircraft.h (NEW)
- src/models/Runway.h (NEW)
- src/models/User.h (NEW)
- sql/schema.sql (placeholder)
- sql/seed.sql (placeholder)
- sql/transactions.sql (placeholder)
- docs/ARCHITECTURE.md (NEW)

### Architecture Established
- GUI Layer: MainWindow with sidebar navigation + QStackedWidget (8 placeholder pages)
- Service Layer: ApplicationService coordinator between UI and sub-systems
- OS Simulation Layer: Scheduler (future priority scheduling) + ResourceManager (future mutex)
- DBMS Layer: DatabaseManager singleton (future Qt SQL/MySQL connection)
- Model Layer: Flight, Aircraft, Runway, User (C++17 structs)
- Utils: PasswordUtils (SHA-256, implemented Day 6)

### Tests
1. CMake configure (clean) — PASS
2. CMake build (clean, 10/10 units) — PASS
3. Application launch — PASS (PID confirmed running)
4. .gitignore verification — NOT TESTED (no new db.conf risk)
5. Future-feature audit — PASS (no Day 3+ code implemented)

### Known Issues
None.

### Git
- Commit: feat: establish Day 2 application architecture
- Branch: main → origin/main

---

## DAY 3 — MySQL Schema + Relationships ✅ COMPLETE

### Files Created/Modified
- `sql/schema.sql` (Implemented complete 8-table DDL)
- `sql/seed.sql` (Documented deferral to Day 4)
- `sql/transactions.sql` (Documented deferral to Days 21–22)
- `docs/DATABASE_DESIGN.md` (NEW — full database design, 1NF/2NF/3NF rationale, cardinalities, invariants)
- `PROJECT_STATUS.md` (Updated)

### Schema Implemented
- Eight relational tables under database `smartatc`:
  1. `roles` (PK `role_id`, UNIQUE `role_name`)
  2. `users` (PK `user_id`, UNIQUE `username`, FK `role_id`)
  3. `aircraft` (PK `aircraft_id`, UNIQUE `registration_number`)
  4. `flights` (PK `flight_id`, FK `aircraft_id`, non-unique `flight_number`, separate `is_emergency`, CHECK priority 1–5)
  5. `runways` (PK `runway_id`, UNIQUE `runway_number`)
  6. `scheduling_queue` (PK `queue_id`, FK `flight_id` UNIQUE enforcing 1:0..1, CHECK priorities 1–5, CHECK service_time > 0)
  7. `runway_assignments` (PK `assignment_id`, FK `flight_id`, FK `runway_id`, FK `assigned_by`, history preserved)
  8. `operation_logs` (PK `log_id`, FK `user_id`, FK `flight_id`, FK `runway_id`, ON DELETE SET NULL)
- Character set: `utf8mb4`, Collation: `utf8mb4_unicode_ci`, Engine: `InnoDB`

### Documentation Created
- `docs/DATABASE_DESIGN.md`: Detailed breakdown of 8 tables, 1NF/2NF/3NF normalization rationale, decoupling justifications, and concurrency invariants for future transactions.

### Tests
1. Static review of `sql/schema.sql` — PASS
2. Verify eight core tables defined — PASS
3. Primary keys verification — PASS
4. Foreign keys & deletion rules verification — PASS
5. Unique constraints & indexes verification — PASS
6. Priority convention consistency (1–5 scale) — PASS
7. Emergency decoupled from operational status — PASS
8. Historical runway assignments supported without blocking uniqueness — PASS
9. Premature seed data deferred — PASS
10. Premature transaction scripts deferred — PASS
11. Live MySQL execution — NOT TESTED (MySQL 8.0.46 running locally; password required for root access without configured db.conf)
12. C++ application build safety — PASS (10/10 units, 0 errors)
13. Git status and diff verification — PASS

### Known Issues
None.

### Git
- Commit: `feat: implement Day 3 MySQL schema and relationships`
- Branch: `main` → `origin/main`

---

## DAY 4 — Normalization Validation + Seed Data ✅ COMPLETE

### Normalization Review
- **1NF Review**: PASS — All columns across all 8 tables store atomic values. Explicit surrogate PKs on every table.
- **2NF Review**: PASS — Single-attribute surrogate PKs preclude partial key dependencies. Natural candidate keys analyzed and verified.
- **3NF Review**: PASS — No transitive dependencies. Roles, aircraft specs, scheduling metrics, and runway assignments isolated into dedicated tables.
- **Schema Modifications**: None required. Day 3 schema adheres cleanly to 3NF and project specifications.

### Seed Data Implemented (`sql/seed.sql`)
- Database: `smartatc`
- **Table Coverage (8/8 tables)**:
  1. `roles`: 3 rows (`Admin`, `ATC`, `Airport Operator`)
  2. `users`: 3 demo accounts (`admin_demo`, `atc_demo`, `operator_demo`) with placeholder SHA-256 hashes
  3. `aircraft`: 4 aircraft (`VT-AT1`, `VT-AT2`, `VT-AT3`, `VT-AT4` with `Active`/`Maintenance` statuses)
  4. `flights`: 6 flights (`AI-101`, `6E-202`, `AI-303`, `SG-404`, `UK-505`, `QP-606`) covering priorities 1–5, emergency flag (`AI-101`), and varied lifecycle statuses
  5. `runways`: 3 runways (`09L` Occupied, `09R` Available, `27` Maintenance)
  6. `scheduling_queue`: 4 PCB records spanning `Running`, `Ready`, and `Completed` process states
  7. `runway_assignments`: 2 assignment records (1 `Released` historical assignment, 1 `Active` non-conflicting assignment on `09L`)
  8. `operation_logs`: 4 audit records covering `SYSTEM_INIT`, `RUNWAY_RELEASE`, `EMERGENCY_DECLARED`, and `RUNWAY_ASSIGN`

### Repeatability & Safety Design
- Idempotent upserts (`ON DUPLICATE KEY UPDATE` and `INSERT ... SELECT ... WHERE NOT EXISTS`).
- Dynamic foreign key resolution based on natural keys (usernames, registration numbers, runway numbers, flight number + date).
- No hardcoded `AUTO_INCREMENT` IDs.
- Zero destructive statements (`DROP TABLE` and `TRUNCATE` strictly prohibited).
- Safe re-execution preserves existing records and prevents duplicate entries.

### Tests
1. Normalization review completed — PASS
2. All eight expected tables considered — PASS
3. Three required roles seeded — PASS
4. At least four aircraft included — PASS (4 aircraft)
5. At least six flights included — PASS (6 flights)
6. At least three runways included — PASS (3 runways)
7. Scheduling entries satisfy schema constraints — PASS
8. Assignment references valid — PASS
9. No conflicting active assignment seeded — PASS (Only 1 active assignment for 09L)
10. Operation-log references valid — PASS
11. Seed script designed for repeatable execution — PASS
12. Seed script executed successfully against MySQL — NOT TESTED (MySQL 8.0.46 running locally; password required for root access without configured db.conf)
13. Seed script executed a second time without duplicates — NOT TESTED (MySQL auth blocker)
14. Referential integrity verified against live MySQL — NOT TESTED (MySQL auth blocker)
15. Existing schema and data preserved — PASS
16. Qt project build safety — PASS (Build unimpacted, static schema validation)
17. Git diff and status reviewed — PASS

### Known Issues
- Live MySQL testing remains blocked until authorized credentials are provided in a local `db.conf`.

### Git
- Commit: `feat: validate normalization and add Day 4 seed data`
- Branch: `main` → `origin/main`

---

## DAY 5 — C++ to MySQL Connectivity ✅ COMPLETE

### Files Created/Modified
- `src/database/DatabaseManager.h` (Enhanced singleton interface, config accessors, driver checking, read-only test query)
- `src/database/DatabaseManager.cpp` (Implemented INI config parsing, QMYSQL driver verification, safe connection management, error sanitization, read-only test queries)
- `src/services/ApplicationService.h` (Added `checkDatabaseConnection()` signature)
- `src/services/ApplicationService.cpp` (Integrated DatabaseManager startup/shutdown without crashing or looping)
- `db.conf.example` (Updated to standard INI `[database]` syntax with clear instructions)
- `tests/TestDatabaseConnectivity.cpp` (NEW — 15 automated test cases validating configuration parsing, missing fields, invalid ports, driver detection, connection lifecycle, and error sanitization)
- `CMakeLists.txt` (Added `Qt6::Test` component and `TestDatabaseConnectivity` test executable target)
- `docs/DATABASE_DESIGN.md` (Added Section 8: C++ to MySQL Connectivity Architecture, config guide, driver analysis)
- `PROJECT_STATUS.md` (Updated)

### Connection Layer Architecture
- **Safe INI Configuration**: Loads credentials from `db.conf` via `QSettings` with deterministic path resolution (working dir, executable dir, parent project dir).
- **Security & Protection**: `.gitignore` strictly protects `db.conf`. Sanitized error reporting strips passwords from log/error outputs.
- **Connection Management**: Uses unique named connection `SmartATCConnection` with safe scoped close and removal, preventing duplicate named connection leaks.
- **Graceful Degradation**: Application starts cleanly without crashing when driver or database is unavailable.
- **Read-Only Verification**: Minimal `testConnectivity()` method executes `SELECT VERSION(), DATABASE();` without any state mutation.

### Tests
1. Valid configuration parsing — PASS
2. Missing configuration file handling — PASS
3. Missing required host validation — PASS
4. Missing required database validation — PASS
5. Missing required username validation — PASS
6. Invalid port range validation — PASS
7. Default config lookup path resolution — PASS
8. QMYSQL driver availability detection — PASS (Official Qt 6.11.2 QMYSQL plugin built & verified available)
9. Connection error handling without driver — PASS (Returns sanitized error, does not crash)
10. Sanitized error reporting (password scrubbing) — PASS (Passwords scrubbed from error output)
11. No duplicate named connection leaks — PASS (Handles multiple connect/disconnect cycles cleanly)
12. Safe disconnect behavior — PASS (Connection closed and removed cleanly)
13. Read-only query behavior when disconnected — PASS (Returns descriptive error, no crash)
14. Qt application build — PASS (15/15 units, 0 errors, SmartATC.exe and TestDatabaseConnectivity.exe)
15. Desktop application launch without database connection — PASS (Process starts, responds, and exits cleanly)
16. Live MySQL connection execution — PASS (Real connection established to database 'smartatc' on MySQL 8.0.46)
17. Live SELECT 1 test query execution — PASS (Returned value: 1)
18. Live active database validation — PASS (Confirmed active schema 'smartatc')

### Resolved Blockers
- **QMYSQL Plugin**: Built cleanly using official Qt 6.11.2 source code against MinGW 13.1.0 64-bit and local MySQL 8.0 client development libraries/headers, and installed into `C:\Qt\6.11.2\mingw_64\plugins\sqldrivers\qsqlmysql.dll`.
- **MySQL Runtime Dependencies**: Deployed `libmysql.dll` to `C:\Qt\6.11.2\mingw_64\bin\` and application `build\` directory.
- **Local Credentials**: Configured in local `db.conf` (untracked, preserved by `.gitignore`).
- **UI Diagnostics**: MainWindow displays real-time connection state badge (Connected, Driver Missing, Server Unreachable, Auth Failed, Schema Missing, Not Configured) in the title bar.

### Git
- Branch: `main` → `origin/main`

---

---
 
---
 
 ## DAY 6 — Login + Role-Based Access Control ✅ COMPLETE
 
 ### Files Created/Modified
 - `src/utils/PasswordUtils.h` (Enhanced API documentation, validation methods)
 - `src/utils/PasswordUtils.cpp` (Implemented SHA-256 password hashing and verification using `QCryptographicHash::Sha256`)
 - `src/models/Session.h` (NEW — In-memory session model with user identity and RBAC role helpers)
 - `src/models/Session.cpp` (NEW — Permission resolution logic and nav access rules for Admin, ATC, and Operator)
 - `src/services/ApplicationService.h` (Added `login()`, `logout()`, `currentSession()`, `hasPermission()`, action boundary methods `executeSchedulerCycle()`, `allocateRunwayResource()`, `setRunwayMaintenanceStatus()`)
 - `src/services/ApplicationService.cpp` (Database-backed authentication flow, parameterized SQL query, account status checks, session lifecycle, action-level RBAC checks)
 - `ui/LoginView.h` (NEW — Clean, dedicated login screen widget)
 - `ui/LoginView.cpp` (NEW — Masked password input, form validation, error display, DB status badge, Enter key submission)
 - `ui/MainWindow.h` (Integrated `QStackedWidget` for authentication state toggling, user badge, logout button)
 - `ui/MainWindow.cpp` (Enforced RBAC on sidebar navigation, hides unauthorized views, dynamic user display)
 - `src/main.cpp` (Added developer CLI flags `--hash-password` and `--provision-account` for secure, reproducible local testing)
 - `tests/TestAuthAndRBAC.cpp` (NEW — 19 automated tests for password utilities, session management, RBAC matrix, service action boundaries, and live authentication)
 - `CMakeLists.txt` (Registered new model, UI, and test suite targets)
 - `docs/ARCHITECTURE.md` (Updated Section 8 with authentication, RBAC details, and developer provisioning instructions)
 - `PROJECT_STATUS.md` (Updated)
 
 ### Security & Password Limitation
 - **Academic SHA-256**: SHA-256 is fast and lacks salt/work factor. It meets the frozen academic specification but is explicitly documented as non-production standard.
 - **Sanitization**: Plaintext passwords are never stored in session objects or logged. Database errors and user existence are scrubbed on the login UI.
 
 ### Role-Based Access Control Summary
 - **Admin**: Dashboard, Users (Exclusive), Flights (Read), Aircraft (Read/Manage), Runways (Read), Scheduling (Read/Override), Transactions (Read/Audit), Logs (All).
 - **ATC**: Dashboard, Flights (Operational), Runways (Allocation), Scheduling (Queue/Run), Logs (ATC Ops). Restricted from Users.
 - **Airport Operator**: Dashboard, Aircraft (Operational), Runways (Maintenance), Logs (Aircraft Ops). Restricted from Scheduling and Users.
 - **Service-Layer Action Enforcement**: Permissions checked at backend service methods (`ApplicationService`), not merely in the GUI.
 
 ### Demo-Account Provisioning Workflow (Hardened)
 Developers can securely provision or update local demo accounts via the compiled binary without transmitting passwords in command-line arguments:
 ```powershell
 ./SmartATC.exe --provision-user <username> --role <Admin|ATC|"Airport Operator"> [--fullname "<Display Name>"]
 ```
 Prompts interactively with non-echoing console input for Password and Confirm Password. Passwords containing colons, spaces, and special characters are fully supported.
 
 ### Tests
 1. SHA-256 digest generation — PASS
 2. Correct password verification — PASS
 3. Incorrect password rejection — PASS
 4. Empty password handling — PASS
 5. Plaintext password not stored in session — PASS
 6. Session creation on simulated success — PASS
 7. No session creation on failed authentication — PASS
 8. Logout clears session state — PASS
 9. Unknown/empty role denied access — PASS
 10. Admin role permission policy — PASS
 11. ATC role permission policy — PASS
 12. Airport Operator role permission policy — PASS
 13. Protected navigation denied when unauthenticated — PASS
 14. Authentication query uses bound parameters — PASS
 15. UI starts without database connection and does not grant access — PASS
 16. Existing application build — PASS (27 compilation units, 0 errors)
 17. Application launch and graceful shutdown — PASS
 18. Live database authentication (Admin, ATC, Operator, Ephemeral) — PASS
 19. Action-level service boundary RBAC enforcement — PASS
 20. Inactive / suspended account rejection — PASS
 21. Account metadata input validation (empty/long fields, role validity) — PASS
 22. Passwords with colons, spaces, and special characters — PASS
 23. Production environment provisioning restriction (`SMARTATC_ENV=production`) — PASS
 
 ### Git
 - Branch: `main` → `origin/main`
 
 ---

## DAY 7 — Flight Management (CRUD + MySQL Integration) ✅ COMPLETE

### Files Created/Modified
- `src/models/Flight.h` (Extended flight model with `source`, `origin` compatibility, `flightType`, `priority` (1–5), `isEmergency`, `operationalStatus`, `scheduledTime`, `createdAt`, `aircraftRegistration`, and robust client-side validation logic)
- `src/services/ApplicationService.h` (Added `getAllFlights()`, `getFlightById()`, `createFlight()`, `updateFlight()`, `deleteFlight()`, `canDeleteFlight()`, `getAvailableAircraft()`, and `flightsChanged()` signal)
- `src/services/ApplicationService.cpp` (Implemented flight data access with parameterized queries, role checks, safe deletion with foreign-key constraint protection, and aircraft lookup)
- `ui/FlightDialog.h` (NEW — Dialog header for modal Add/Edit flight form)
- `ui/FlightDialog.cpp` (NEW — Form dialog implementation with inputs, validation warnings, and aircraft selection)
- `ui/FlightsView.h` (NEW — Flight Management view header with table, toolbar, and state switching)
- `ui/FlightsView.cpp` (NEW — Interactive flight operations table, instant search, status filtering, badge formatting, and safe deletion workflow)
- `ui/MainWindow.h` (Added `m_flightsView` member and forward declaration)
- `ui/MainWindow.cpp` (Integrated `FlightsView` replacing placeholder page 1, wired login/role permission updates and automatic refreshing)
- `tests/TestFlightManagement.cpp` (NEW — Comprehensive test suite with 23 unit/integration tests covering validation, service-layer RBAC, and live MySQL CRUD)
- `CMakeLists.txt` (Registered new UI files in `APP_SOURCES` and added `TestFlightManagement` test target)
- `docs/ARCHITECTURE.md` (Updated with Day 7 Flight Management architecture details)
- `PROJECT_STATUS.md` (Updated)

### Flight CRUD Capabilities
1. **View / Read**: Display persisted flights in a structured Qt table showing Flight ID, Flight #, Airline, Flight Type, Route (`source → destination`), Scheduled Time, Priority, Emergency indicator, Operational Status, and Assigned Aircraft.
2. **Search & Filter**: Real-time multi-column search (`flight_number`, `airline`, `source`, `destination`, `registration_number`) and status filtering (`All`, `Scheduled`, `Taxiing`, `Cleared`, `Landed`, `Departed`, `Cancelled`).
3. **Create**: Modal dialog allowing authorized users to create flight records with client and service validation, persisting directly to MySQL `flights` table.
4. **Edit**: Full record modification for existing flights with bound parameters, verifying record existence.
5. **Safe Delete & Referential Integrity**: Delete flight records safely. If foreign key constraints (e.g. `fk_ra_flight_id` in `runway_assignments` with `ON DELETE RESTRICT`) prevent deletion, the operation is blocked gracefully, diagnostic explanations are provided, and users are offered the safe operational alternative to set status to `Cancelled`.
6. **Aircraft Assignment**: Fetches available aircraft records dynamically from the `aircraft` table for assignment dropdowns.

### Form Validation Rules
- **Flight Number**: Required, non-empty, 1–10 characters.
- **Airline**: Required, non-empty, 1–100 characters.
- **Source & Destination**: Required, non-empty, 1–100 characters each; source and destination cannot be identical (case-insensitive check).
- **Flight Type**: Must be either `'Arrival'` or `'Departure'`.
- **Priority**: Enforced integer range between 1 (highest) and 5 (lowest).
- **Emergency Flag**: Boolean flag (0 or 1); independent from operational status.
- **Operational Status**: Must be one of `'Scheduled'`, `'Taxiing'`, `'Cleared'`, `'Landed'`, `'Departed'`, `'Cancelled'`.
- **Scheduled Time**: Must be a valid date and time.
- **Aircraft ID**: Non-negative integer (0 denotes unassigned / NULL in DB; >0 must exist in `aircraft` table).

### Role Permissions & Service-Boundary Enforcement
- **Admin**: Read-only access to Flights page (view and refresh). Add, Edit, Delete buttons hidden/disabled in GUI; calls to mutate flights are rejected at service boundary (`ApplicationService`).
- **ATC**: Full flight operations management (View, Refresh, Add, Edit, Delete/Cancel). Authorized for `page:Flights` and `action:ManageFlights`.
- **Airport Operator**: Read-only access to Flights page (view and refresh). Mutate calls rejected at service boundary.
- **Unauthenticated**: All flight queries and mutations rejected.

### Safe Deletion & Referential Integrity
- Checked against foreign keys before deletion (`runway_assignments` has `ON DELETE RESTRICT`).
- When deletion is blocked by existing runway assignments, MySQL error 1451 is caught gracefully.
- The GUI presents a prompt allowing the ATC controller to update status to `'Cancelled'` instead of destructive hard delete, preserving audit history.

### Tests Executed
1. `TestFlightManagement::initTestCase` — PASS
2. `TestFlightManagement::testValidFlightValidation` — PASS
3. `TestFlightManagement::testEmptyFlightNumberRejection` — PASS
4. `TestFlightManagement::testExcessiveFlightNumberLength` — PASS
5. `TestFlightManagement::testEmptyAirlineRejection` — PASS
6. `TestFlightManagement::testEmptySourceAndDestinationRejection` — PASS
7. `TestFlightManagement::testIdenticalSourceAndDestinationRejection` — PASS
8. `TestFlightManagement::testPriorityRangeValidation` — PASS
9. `TestFlightManagement::testInvalidFlightTypeRejection` — PASS
10. `TestFlightManagement::testInvalidOperationalStatusRejection` — PASS
11. `TestFlightManagement::testInvalidScheduledTimeRejection` — PASS
12. `TestFlightManagement::testNegativeAircraftIdRejection` — PASS
13. `TestFlightManagement::testOptionalAircraftIdAllowed` — PASS
14. `TestFlightManagement::testUnauthenticatedCannotCreateOrModify` — PASS
15. `TestFlightManagement::testAdminRoleDeniedFlightModifications` — PASS
16. `TestFlightManagement::testOperatorRoleDeniedFlightModifications` — PASS
17. `TestFlightManagement::testATCRolePermittedFlightModifications` — PASS
18. `TestFlightManagement::testLiveGetAllFlights` — PASS
19. `TestFlightManagement::testLiveFlightSearchAndStatusFilter` — PASS
20. `TestFlightManagement::testLiveCreateReadUpdateDeleteCycle` — PASS
21. `TestFlightManagement::testLiveReferentialIntegritySafeDelete` — PASS
22. `TestFlightManagement::testLiveAvailableAircraftOptions` — PASS
23. `TestFlightManagement::cleanupTestCase` — PASS

All 3 CTest targets pass 100%:
- `TestDatabaseConnectivity`: Passed
- `TestAuthAndRBAC`: Passed
- `TestFlightManagement`: Passed (23/23 tests passed)

### Live MySQL Verification Status
- Verified against live MySQL 8.0 on localhost with `smartatc` database.
- Read operations returned seed flights (`AI-101`, `6E-202`, etc.) joined with aircraft registrations.
- Create/Read/Update/Delete cycle executed with test flight `TEST-701`, verifying persistence and post-delete verification.
- Referential integrity verified against `AI-101` (blocked by runway assignment, preserved in DB).
- Test records cleanly removed; seed data fully preserved.

### Known Limitations
- Scheduling queue integration and PCB process mapping will be implemented in Days 9–12.
- Aircraft CRUD management is scheduled for Day 8 (Aircraft Management); currently aircraft records are read-only for assignment selection.
- Full multi-statement ACID transaction monitor will be implemented in Days 21–23.

### Git
- Branch: `main` → `origin/main`

---

## DAY 8 — Aircraft Management (CRUD + MySQL Integration) ✅ COMPLETE

### Files Created/Modified
- `src/models/Aircraft.h` (Extended with validation, normalization, and flight count tracking)
- `src/models/Session.cpp` (Centralized RBAC: `action:AdministerAircraft` for Admin, `action:ManageAircraft` for Operator/Admin, `page:Aircraft` for Admin/Operator/ATC)
- `src/services/ApplicationService.h` (Extended with aircraft CRUD methods, status management, referential integrity check)
- `src/services/ApplicationService.cpp` (Implemented `getAllAircraft`, `getAircraftById`, `getAircraftByRegistration`, `createAircraft`, `updateAircraft`, `updateAircraftStatus`, `canDeleteAircraft`, `deleteAircraft`)
- `ui/AircraftDialog.h` (NEW — Modal dialog for registering and editing aircraft with role-aware field controls)
- `ui/AircraftDialog.cpp` (NEW — Dialog form implementation with validation and operator status-only enforcement)
- `ui/AircraftView.h` (NEW — Aircraft fleet management GUI view with search, filter, status badge, toolbar)
- `ui/AircraftView.cpp` (NEW — Table rendering, status color badges, safe deletion dialog with retirement fallback)
- `ui/MainWindow.h` (Integrated `AircraftView` pointer)
- `ui/MainWindow.cpp` (Replaced placeholder at navigation row 2 with live `AircraftView`, wired up permissions and refreshes)
- `CMakeLists.txt` (Added `AircraftDialog` and `AircraftView` to `APP_SOURCES`, created `TestAircraftManagement` test target)
- `tests/TestAircraftManagement.cpp` (NEW — 25 automated Qt tests covering validation, RBAC, live MySQL CRUD, safe deletion)
- `docs/ARCHITECTURE.md` (Updated with Aircraft Management module specifications)
- `PROJECT_STATUS.md` (Updated with Day 8 report)

### Aircraft CRUD Capabilities
- **Fleet List View**: Dynamic table displaying Aircraft ID, Registration #, Aircraft Type / Model, Passenger Capacity, Operational Status, and Linked Flights count.
- **Search & Filtering**: Live search by registration or aircraft type combined with operational status filtering (`All`, `Active`, `Maintenance`, `Retired`).
- **Registration (Add Aircraft)**: Admin-only modal form validating registration uniqueness, aircraft type, capacity (> 0), and status.
- **Fleet Editing**: Admin can edit all attributes; Airport Operators can update operational status only (registration, type, capacity locked).
- **Safe Deletion**: Two-phase deletion with referential integrity verification preventing deletion of aircraft referenced by flights. Prompts for safe retirement (`Retired`) when deletion is blocked.
- **Double-click Inspection**: Authorized users open edit dialog; view-only users view details dialog.

### Validation & Status Rules
- **Registration Number**: Required, trimmed, normalized to upper case, maximum 20 characters, checked for uniqueness in MySQL.
- **Aircraft Type**: Required, trimmed, maximum 50 characters.
- **Passenger Capacity**: Unsigned integer, strictly positive (> 0), maximum 65535 (`SMALLINT UNSIGNED`).
- **Operational Status**: Strictly validated against schema `ENUM('Active', 'Maintenance', 'Retired')`.

### Role-Based Access Control (RBAC)
- **Admin**: Full aircraft fleet administration (`page:Aircraft`, `action:ManageAircraft`, `action:AdministerAircraft`). Can add, edit all attributes, and delete unreferenced aircraft.
- **Airport Operator**: Operational status management (`page:Aircraft`, `action:ManageAircraft`). Can view fleet and update status (`Active`, `Maintenance`, `Retired`). Tampering with registration, model, or capacity is prevented in the UI and rejected at the service boundary. Administrative deletion is blocked.
- **ATC**: Operational viewing (`page:Aircraft`). Permitted to view fleet for flight assignment; denied all aircraft management and deletion actions.
- **Unauthenticated**: All aircraft access and mutation rejected.

### Safe Deletion & Referential Integrity
- Checked against foreign keys before deletion (`flights.aircraft_id`).
- When deletion is blocked because active or historical flights reference the aircraft, deletion is blocked without altering foreign-key checks.
- Explanatory feedback is displayed, offering the administrator the option to update the status to `'Retired'` instead of deletion, preserving historical flight data.

### Automated Test Results (25 / 25 Passed)
1. `TestAircraftManagement::initTestCase` — PASS
2. `TestAircraftManagement::testValidAircraftValidation` — PASS
3. `TestAircraftManagement::testEmptyRegistrationRejection` — PASS
4. `TestAircraftManagement::testExcessiveRegistrationLength` — PASS
5. `TestAircraftManagement::testEmptyAircraftTypeRejection` — PASS
6. `TestAircraftManagement::testExcessiveAircraftTypeLength` — PASS
7. `TestAircraftManagement::testZeroAndNegativeCapacityRejection` — PASS
8. `TestAircraftManagement::testValidCapacityAccepted` — PASS
9. `TestAircraftManagement::testInvalidStatusRejection` — PASS
10. `TestAircraftManagement::testValidStatusesAccepted` — PASS
11. `TestAircraftManagement::testTrimmingAndNormalization` — PASS
12. `TestAircraftManagement::testUnauthenticatedCannotAccessOrModify` — PASS
13. `TestAircraftManagement::testATCRoleViewOnlyDeniedModifications` — PASS
14. `TestAircraftManagement::testOperatorRoleStatusUpdateOnlyDeniedRegistration` — PASS
15. `TestAircraftManagement::testOperatorRoleDeniedDeletion` — PASS
16. `TestAircraftManagement::testAdminRolePermittedFullAircraftAdministration` — PASS
17. `TestAircraftManagement::testLiveGetAllAircraft` — PASS
18. `TestAircraftManagement::testLiveAircraftSearchAndStatusFilter` — PASS
19. `TestAircraftManagement::testLiveGetAircraftById` — PASS
20. `TestAircraftManagement::testLiveCreateReadUpdateDeleteCycle` — PASS
21. `TestAircraftManagement::testLiveRejectDuplicateRegistration` — PASS
22. `TestAircraftManagement::testLiveOperatorStatusUpdateOnly` — PASS
23. `TestAircraftManagement::testLiveReferentialIntegrityPreventsReferencedDeletion` — PASS
24. `TestAircraftManagement::testLiveAvailableAircraftForFlightAssignment` — PASS
25. `TestAircraftManagement::cleanupTestCase` — PASS

All 4 CTest targets pass 100%:
- `TestDatabaseConnectivity`: Passed
- `TestAuthAndRBAC`: Passed
- `TestFlightManagement`: Passed
- `TestAircraftManagement`: Passed (25/25 tests passed)

### Live MySQL Verification Status
- Verified against live MySQL 8.0 on localhost with `smartatc` database.
- Seed records verified (`VT-AT1` Boeing 737-800, `VT-AT2` Airbus A320neo, `VT-AT3` Boeing 777-300ER, `VT-AT4` Bombardier Q400).
- CRUD cycle verified live with test aircraft `VT-TEST100`.
- Referential integrity verified live on `VT-AT1` (blocked due to flight reference `AI-101`).
- Operator status-only update and attribute tampering rejection verified live.
- Test records cleanly removed; seed data fully preserved.

### Known Limitations
- Process model creation (PCB) and ready queue insertion are completed in Day 9.
- Runway assignment workflows are deferred to Day 14.
- Priority scheduling and aging algorithms are deferred to Days 11–12.

### Git
- Branch: `main` → `origin/main`

---

## DAY 9 — Process Model (PCB) and Flight-to-Process Mapping ✅ COMPLETE

### Files Created/Modified
- `src/models/Process.h` (NEW — strongly-typed Process Control Block model, ProcessState enum, validation, flight-to-process mapping)
- `src/models/Process.cpp` (NEW — implementation of PCB validation, string/DB state conversions, eligibility checks, mapping)
- `src/services/ApplicationService.h` (Added `createProcessForFlight`, `createProcess`, `hasProcessForFlight`, `getProcessByFlightId`, `getProcessById`, `getAllProcesses`, and `processesChanged` signal)
- `src/services/ApplicationService.cpp` (Implemented process creation, duplicate prevention on `flight_id`, flight eligibility check, and queries)
- `CMakeLists.txt` (Added `src/models/Process.h` and `Process.cpp` to `APP_SOURCES` and test suites, added `TestProcessModel` test target)
- `tests/TestProcessModel.cpp` (NEW — 19 automated Qt unit & live integration tests)
- `PROJECT_STATUS.md` (Updated with Day 9 report)

### Process Control Block (PCB) Design
- **Identity & Identification**:
  - `id` / `queueId`: Integer primary key mapping to `scheduling_queue.queue_id` (`0` indicates unsaved/new record, `> 0` indicates persisted record).
  - `flightId` / `processId()`: Integer foreign key linking the PCB to `flights.flight_id`. Acts as the unique simulated process ID (PID).
- **Scheduling Attributes**:
  - `arrivalTime`: `QDateTime` recording entry into the scheduling queue.
  - `basePriority`: Static priority integer (`1` highest … `5` lowest, default `3`).
  - `effectivePriority`: Dynamic priority integer (`1` highest … `5` lowest, default `3`). Strictly separated from base priority to support Phase 2 aging without data mutation of the static flight priority.
  - `serviceTime` / `burstTime()`: Simulated CPU burst duration in seconds (`SMALLINT UNSIGNED`, default `60s`, user-configurable).
- **Execution State**:
  - Type-safe `ProcessState` enum: `New`, `Ready`, `Running`, `Blocked`, `Completed`.
  - Stored in MySQL `scheduling_queue.process_state` ENUM(`Ready`, `Running`, `Completed`, `Blocked`). Conceptual `New` maps to `Ready` when queued.
- **Performance Metrics**:
  - `waitingTime`: Seconds spent waiting in ready queue (`-1` = uncalculated/NULL).
  - `turnaroundTime`: Elapsed duration from arrival to completion (`-1` = uncalculated/NULL).
  - `completionTime`: `QDateTime` timestamp when process finished execution (invalid = NULL).
  - Metrics remain uncalculated during Day 9, adhering strictly to the frozen scope.

### Flight-to-Process Mapping & Duplicate Prevention
- **Eligibility Rules**:
  - Eligible operational statuses: `Scheduled`, `Taxiing`, `Cleared`.
  - Excluded terminal / cancelled statuses: `Landed`, `Departed`, `Cancelled`.
  - Unsaved flights (`id <= 0`) or flights with invalid priority are rejected.
- **Mapping Logic**:
  - `flight_id` maps to PCB `flightId`.
  - `priority` maps to `basePriority` and initial `effectivePriority`.
  - User-specified service time is preserved (defaults to 60s if not specified).
  - `arrivalTime` set to current timestamp; `process_state` initialized to `Ready`.
- **Duplicate Prevention**:
  - Checked at application service level via parameterized query `SELECT queue_id FROM scheduling_queue WHERE flight_id = ?`.
  - Enforced at schema level via `UNIQUE KEY uq_sq_flight_id (flight_id)` ensuring a 1:0..1 relationship.
  - Attempted duplicates return descriptive error without crashing or silent replacement.

### Database Impact
- **Schema Modification**: NONE. `sql/schema.sql` was NOT modified.
- **Table Reused**: The existing `scheduling_queue` table (8-table schema) was fully reused.
- No 9th table was added.

### Operating Systems Concepts Established
- **Process Abstraction**: Flights modelled as simulated OS processes via C++ objects (not kernel processes).
- **Process Control Block (PCB)**: Explicit data structure holding PID, priority, burst time, arrival time, state, and accounting metrics.
- **Priority Separation**: Independent base and effective priority to prepare for dynamic aging algorithms.
- **Process Lifecycle States**: Model representation of process lifecycle (`Ready`, `Running`, `Blocked`, `Completed`).

### DBMS Concepts Established
- **Referential Integrity**: `scheduling_queue.flight_id` enforced via foreign key to `flights.flight_id`.
- **Cardinality Constraint**: `UNIQUE KEY uq_sq_flight_id` enforcing 1:0..1 flight-to-process queue relationship.
- **Parameterized SQL**: All database operations use named parameter binding preventing SQL injection.
- **Normalization (2NF/3NF)**: Queue attributes separated from flights table avoiding sparse NULLs on unscheduled flights.

### Automated Test Results (19 / 19 Passed in TestProcessModel; 5 / 5 Test Suites Passed)
1. `TestProcessModel::initTestCase` — PASS
2. `TestProcessModel::testDefaultConstructionAndInitialization` — PASS
3. `TestProcessModel::testSetAndGetIdentities` — PASS
4. `TestProcessModel::testValidPriorityBoundaries` — PASS
5. `TestProcessModel::testPriorityOutOfRangeRejection` — PASS
6. `TestProcessModel::testServiceTimeValidation` — PASS
7. `TestProcessModel::testBaseAndEffectivePrioritySeparation` — PASS
8. `TestProcessModel::testProcessStateRepresentationAndMapping` — PASS
9. `TestProcessModel::testFlightToProcessMapping` — PASS
10. `TestProcessModel::testFlightEligibilityRules` — PASS
11. `TestProcessModel::testMetricsRemainUncalculated` — PASS
12. `TestProcessModel::testUnauthenticatedCannotEnqueueFlight` — PASS
13. `TestProcessModel::testOperatorRoleDeniedProcessCreation` — PASS
14. `TestProcessModel::testATCRolePermittedProcessCreation` — PASS
15. `TestProcessModel::testAdminRolePermittedProcessCreation` — PASS
16. `TestProcessModel::testLiveProcessCreationAndRetrieval` — PASS
17. `TestProcessModel::testLiveDuplicateProcessRejection` — PASS
18. `TestProcessModel::testLiveIneligibleFlightProcessCreationRejection` — PASS
19. `TestProcessModel::cleanupTestCase` — PASS

All 5 CTest suites pass 100%:
- `TestDatabaseConnectivity`: Passed
- `TestAuthAndRBAC`: Passed
- `TestFlightManagement`: Passed
- `TestAircraftManagement`: Passed
- `TestProcessModel`: Passed (19/19 tests passed)

### Scope Boundaries Confirmed (What was NOT implemented)
- No scheduling algorithm (Day 11).
- No ready queue ordering, priority queue, or state transition engine (Day 10).
- No metrics calculation for WT, TAT, CT (Day 12).
- No aging engine or starvation prevention (Days 17–18).
- No emergency priority overrides (Day 16).
- No runway mutexes, synchronization, or threads (Days 14–20).
- No new database tables.

---

## DAY 10 — Process States and Ready Queue Lifecycle ✅ COMPLETE

### Files Created/Modified
- `src/models/Process.h` (updated — state transitions `canTransitionTo()`, `transitionTo()`, convenience constructors and accessors, database state mapping)
- `src/models/Process.cpp` (updated — state transition validation logic, state string mappings)
- `src/scheduler/ReadyQueue.h` (NEW — modular ready queue managing in-memory processes in strict FIFO order, duplicate prevention, lookup/removal helpers, state consistency enforcement)
- `src/scheduler/ReadyQueue.cpp` (NEW — implementations for enqueue, dequeue, removeByFlightId, removeByQueueId, transitionProcessState)
- `src/scheduler/Scheduler.h` (updated — owns `SchedulerComponents::ReadyQueue` instance, accessor methods)
- `src/scheduler/Scheduler.cpp` (updated — scheduler ready queue integration)
- `src/services/ApplicationService.h` (updated — process state update, in-memory ready queue access, enqueue/dequeue helpers, syncReadyQueueFromDatabase)
- `src/services/ApplicationService.cpp` (updated — `updateProcessState` with RBAC + DB persistence + queue sync, `syncReadyQueueFromDatabase`, deletion cleanup)
- `CMakeLists.txt` (updated — added ReadyQueue sources to APP_SOURCES and all test targets, registered `TestReadyQueueAndProcessStates`)
- `tests/TestReadyQueueAndProcessStates.cpp` (NEW — 24 comprehensive unit and integration tests)

### Process Lifecycle and Validated Transitions
Supported state transitions:
- `New -> Ready`: flight accepted into the in-memory ready queue.
- `Ready -> Running`: process dispatched for runway execution (Day 11/14).
- `Running -> Completed`: process finished simulated runway execution.
- `Running -> Blocked`: process blocked on runway availability or holding.
- `Blocked -> Ready`: process unblocked and returned to ready state.

Explicitly rejected invalid transitions:
- `New -> Running` (cannot bypass ready queue)
- `New -> Blocked` or `New -> Completed`
- `Ready -> Blocked` or `Ready -> Completed` (must be running first)
- `Blocked -> Running` or `Blocked -> Completed` (must return to ready first)
- `Completed -> *` (terminal state: no further transitions permitted)
- `State -> State` (same-state transition rejected as redundant)

All rejected transitions leave the original process state completely unaltered without side effects.

### ReadyQueue Architecture & Identity Rules
- **Component**: `SchedulerComponents::ReadyQueue` under `src/scheduler/`.
- **Identity Distinction**: `flightId` is the unique simulated PID; `queueId` / `id` is the primary key in `scheduling_queue`. Lookups and removals are supported on both.
- **Ordering**: Strict FIFO insertion order using `QList<Models::Process>` (`takeFirst()`).
- **NO Sorting**: No priority-based sorting, effective priority sorting, or shortest-job-first ordering in this milestone (deferred to Day 11).
- **Duplicate Prevention**: `enqueue()` rejects duplicates on `flightId` or `queueId` before insertion.
- **State Consistency**: Only processes in `ProcessState::Ready` may enter `ReadyQueue`. When a process transitions out of `Ready` via `transitionProcessState()` or `ApplicationService::updateProcessState()`, it is atomically removed from `ReadyQueue` so no stale entries remain.

### Database Layer Compatibility
- In-memory `ProcessState::New` maps to MySQL ENUM `'Ready'` via `Process::stateToDbString()` to preserve strict compatibility with MySQL `scheduling_queue.process_state ENUM('Ready','Running','Completed','Blocked')`.
- All SQL updates are executed via parameterized queries with bound variables.
- The 8-table relational database schema remains intact with zero schema modifications.

### Automated Test Results (24 / 24 Passed in TestReadyQueueAndProcessStates; 6 / 6 Test Suites Passed 100%)
1. `TestReadyQueueAndProcessStates::initTestCase` — PASS
2. `TestReadyQueueAndProcessStates::testValidStateInitialization` — PASS
3. `TestReadyQueueAndProcessStates::testSupportedValidTransitions` — PASS
4. `TestReadyQueueAndProcessStates::testRejectionOfInvalidTransitions` — PASS
5. `TestReadyQueueAndProcessStates::testOriginalStateUnchangedAfterRejectedTransition` — PASS
6. `TestReadyQueueAndProcessStates::testCppStateToDbStringMapping` — PASS
7. `TestReadyQueueAndProcessStates::testDbStringToCppStateMappingAndInvalidHandling` — PASS
8. `TestReadyQueueAndProcessStates::testAddValidReadyProcess` — PASS
9. `TestReadyQueueAndProcessStates::testFifoInsertionOrderPreserved` — PASS
10. `TestReadyQueueAndProcessStates::testFindingProcessByIdentifiers` — PASS
11. `TestReadyQueueAndProcessStates::testRemovingProcessSuccessfully` — PASS
12. `TestReadyQueueAndProcessStates::testRemovingNonexistentProcess` — PASS
13. `TestReadyQueueAndProcessStates::testRejectingDuplicateInsertion` — PASS
14. `TestReadyQueueAndProcessStates::testRejectingInvalidProcessData` — PASS
15. `TestReadyQueueAndProcessStates::testRejectingNonReadyStateInsertion` — PASS
16. `TestReadyQueueAndProcessStates::testQueueSizeAndEmptyBehavior` — PASS
17. `TestReadyQueueAndProcessStates::testQueueMembershipAfterRemoval` — PASS
18. `TestReadyQueueAndProcessStates::testProcessTransitionOutOfReadyRemovesFromQueue` — PASS
19. `TestReadyQueueAndProcessStates::testNoQueueSortingByPriorityOrServiceTime` — PASS
20. `TestReadyQueueAndProcessStates::testServiceUpdateProcessStateRBAC` — PASS
21. `TestReadyQueueAndProcessStates::testServiceEnqueueAndDequeueIntegration` — PASS
22. `TestReadyQueueAndProcessStates::testServiceSyncReadyQueueFromDatabase` — PASS
23. `TestReadyQueueAndProcessStates::testServiceFlightDeletionRemovesFromReadyQueue` — PASS
24. `TestReadyQueueAndProcessStates::cleanupTestCase` — PASS

All 6 CTest suites pass 100%:
- `TestDatabaseConnectivity`: Passed
- `TestAuthAndRBAC`: Passed
- `TestFlightManagement`: Passed
- `TestAircraftManagement`: Passed
- `TestProcessModel`: Passed
- `TestReadyQueueAndProcessStates`: Passed (24/24 tests passed)

### Scope Boundaries Confirmed (What was NOT implemented)
- No non-preemptive priority scheduling or queue sorting (Day 11).
---

## DAY 11 — Non-Preemptive Priority Scheduling Algorithm ✅ COMPLETE

### Files Created/Modified
- `src/scheduler/Scheduler.h` (updated — `DispatchResult`, `selectNextProcess`, `dispatchNextProcess`, `runningProcess`, `hasRunningProcess`, signals)
- `src/scheduler/Scheduler.cpp` (updated — non-preemptive selection, deterministic tie-breaking by arrival time and flight ID, single-process dispatch, state transition enforcement)
- `src/services/ApplicationService.h` (updated — `dispatchNextProcess` coordination, action RBAC enforcement)
- `src/services/ApplicationService.cpp` (updated — `dispatchNextProcess` implementation with MySQL `scheduling_queue` state persistence and in-memory queue synchronization)
- `CMakeLists.txt` (updated — registered `TestPriorityScheduler`)
- `tests/TestPriorityScheduler.cpp` (NEW — 25 comprehensive unit and regression tests)

### Operating System Scheduling Rules Enforced
1. **Priority Model**: Numeric scale 1–5 where lower number = higher priority (1 is Highest, 5 is Lowest).
2. **Deterministic Tie-Breaking**: Equal priority broken by earlier arrival time; identical arrival time broken by smaller flight ID (PID).
3. **Strict Non-Preemption**: Dispatch is rejected if any process is already in RUNNING state. A newly arriving higher-priority process never preempts a currently running process.
4. **Single-Process Dispatch**: Exactly one process transitions from READY to RUNNING per dispatch operation and leaves the ready queue.
5. **State Safety**: Empty queues, completed processes, and illegal transitions are rejected safely without side effects or queue corruption.

### Automated Test Results (25 / 25 Passed in TestPriorityScheduler; 7 / 7 Test Suites Passed 100%)
- All 25 unit, state transition, non-preemption, and live persistence tests passed without errors.

---

## DAY 12 — Scheduling Metrics & Lifecycle Integration ✅ COMPLETE

### Files Created/Modified
- `src/models/Process.h` (updated — `calculateMetrics`, `isCompleted`, timing validation methods)
- `src/models/Process.cpp` (updated — Turnaround Time (TAT = CT - AT) and Waiting Time (WT = TAT - Service Time) formulas and invariant validation)
- `src/scheduler/Scheduler.h` (updated — `PerformanceMetrics`, `CompletionResult`, `completeRunningProcess`, `completeProcess`, `calculateAggregateMetrics`, `completedProcesses`)
- `src/scheduler/Scheduler.cpp` (updated — metric computation, aggregation across completed processes, completed process collection management)
- `src/services/ApplicationService.h` (updated — `completeRunningProcess`, `getAggregateSchedulingMetrics`, `getCompletedProcesses`)
- `src/services/ApplicationService.cpp` (updated — `completeRunningProcess` with action RBAC and MySQL `scheduling_queue` persistence of CT, TAT, WT)
- `CMakeLists.txt` (updated — registered `TestSchedulingMetrics`)
- `tests/TestSchedulingMetrics.cpp` (NEW — 22 comprehensive unit and regression tests)

### Scheduling Metric Formulas Implemented
- **Completion Time (CT)**: Simulated or wall-clock completion timestamp.
- **Turnaround Time (TAT)**: Total elapsed seconds from process arrival to execution completion: `TAT = CT - arrival_time`.
- **Waiting Time (WT)**: Total idle seconds spent waiting in ready queue: `WT = TAT - service_time`.
- **Aggregate Metrics**: Mean turnaround time and mean waiting time computed over completed process sets; zero-safe for empty sets.

### Automated Test Results (22 / 22 Passed in TestSchedulingMetrics; 8 / 8 Test Suites Passed 100%)
- All 22 calculation, boundary condition, lifecycle exclusion, and live persistence tests passed without errors.

---

## DAY 13 — Phase 1 End-to-End Verification & Hardening ✅ COMPLETE

### Scope & Verification Summary
- **Verification Pass**: Full build, runtime GUI execution, MySQL live connectivity, security audit, and all 8 registered CTest suites (177 tests total) verified.
- **Defects Confirmed / Fixed**: 0 defects found. All existing Phase 1 implementations conform to frozen architectural specifications.
- **Documentation**: Comprehensive report created in `docs/PHASE1_VERIFICATION_REPORT.md`.
- **Phase 1 Status**: Complete and frozen. Ready for human review and approval.

---

## DAY 14 — Runway Management Implementation ✅ COMPLETE

### Files Created/Modified
- `src/models/Runway.h` (updated — mapped to MySQL `runways` schema (`runway_id`, `runway_number`, `runway_type`, `status`), added enums `RunwayStatus`, `RunwayType`, validation, normalization, and string conversion helpers)
- `src/services/ApplicationService.h` (updated — declared `getAllRunways`, `getRunwayById`, `getRunwayByNumber`, `updateRunwayStatus`, `setRunwayMaintenanceStatus`, `createRunway`, `updateRunway`, and `runwaysChanged` signal)
- `src/services/ApplicationService.cpp` (updated — implemented runway querying with active assignment joining, role-based authorization for Admin/Operator, parameterized SQL, and error reporting)
- `ui/RunwayDialog.h` (NEW — modal dialog for runway details, creation, and maintenance toggle with role enforcement)
- `ui/RunwayDialog.cpp` (NEW — dialog implementation with validation and permission checks)
- `ui/RunwaysView.h` (NEW — comprehensive runway management view with status badges, search filter, and maintenance controls)
- `ui/RunwaysView.cpp` (NEW — view implementation interacting strictly with ApplicationService)
- `ui/MainWindow.h` (updated — added `RunwaysView* m_runwaysView` and `refreshRunways` slot)
- `ui/MainWindow.cpp` (updated — integrated `RunwaysView` at page index 3 and wired up navigation/permissions)
- `tests/TestRunwayManagement.cpp` (NEW — 25 unit and integration tests covering model validation, RBAC, and live MySQL operations)
- `CMakeLists.txt` (updated — included runway UI files in `SmartATC` and registered `TestRunwayManagement` test executable)
- `scripts/deploy_runtime.ps1` (updated — added `Qt6Test.dll` deployment to resolve Windows runtime error `0xc0000135`)
- `PROJECT_STATUS.md` (updated — documented Day 14 completion)
- `docs/DAY14_RUNWAY_MANAGEMENT_REPORT.md` (NEW — comprehensive Day 14 report)

### Architecture Established
- **Model**: `Runway` model rigorously validates runway numbers (1–10 chars), status (`Available`, `Occupied`, `Maintenance`), and types (`Landing`, `Takeoff`, `Both`).
- **Application Service Layer**: Single point of truth for business logic. Enforces RBAC permissions:
  - `Admin`: Full runway administration (view, create, modify, toggle maintenance).
  - `Airport Operator`: Operational status viewing and maintenance toggling (`setRunwayMaintenanceStatus`).
  - `ATC`: Operational status viewing only.
- **Database Layer**: All operations use parameterized SQL queries against MySQL `smartatc.runways` table. Read queries join `runway_assignments` and `flights` to display current assigned flight information without implementing future allocation algorithms.
- **GUI Layer**: Integrated `RunwaysView` into `MainWindow` navigation stack at index 3. Displays status badges with clear text labels, live counts, search/filter controls, and role-appropriate action buttons.
- **OS Resource Foundation**: Established runway as a shared operating system resource with distinct availability states (`Available`, `Occupied`, `Maintenance`).

### Automated Test Results (25 / 25 Passed in TestRunwayManagement; 9 / 9 Test Suites Passed 100%)
- All 9 CTest suites pass 100% (202 total tests across suite):
  1. `TestDatabaseConnectivity`: Passed
  2. `TestAuthAndRBAC`: Passed
  3. `TestFlightManagement`: Passed
  4. `TestAircraftManagement`: Passed
  5. `TestProcessModel`: Passed
  6. `TestReadyQueueAndProcessStates`: Passed
  7. `TestPriorityScheduler`: Passed
  8. `TestSchedulingMetrics`: Passed
  9. `TestRunwayManagement`: Passed (25/25 tests passed)

### Scope Boundaries Confirmed (What was NOT implemented)
- No flight-to-runway allocation algorithm or priority scheduling tie-ins (Day 15).
- No dynamic emergency priority adjustments (Day 16).
- No mutual exclusion, mutexes, condition variables, or critical section locks (Days 19–20).
- No MySQL transaction management, COMMIT/ROLLBACK, or ACID guarantees (Days 21–22).

---

## DAY 15 — Runway Resource Allocation ✅ COMPLETE

### Files Created/Modified
- `src/models/RunwayAssignment.h` (NEW — mapped to MySQL `runway_assignments` schema, added enums `AssignmentStatus`, `AllocationResult`, helper `allocationResultToString()`, and active/released state accessors)
- `src/services/ApplicationService.h` (updated — declared `allocateRunway`, `allocateRunwayResource`, `releaseRunway`, `releaseRunwayResource`, `getEligibleFlightsForRunwayAllocation`, `getAvailableRunwaysForAllocation`, `getActiveAssignmentForRunway`, and `getActiveAssignmentForFlight`)
- `src/services/ApplicationService.cpp` (updated — implemented comprehensive allocation validation: checking runway existence and Available status, rejecting Occupied or Maintenance runways, verifying non-terminal flight status, rejecting conflicts and duplicate active assignments, verifying scheduling queue state, inserting to `runway_assignments`, updating runway to Occupied, updating flight to Cleared, inserting audit logs to `operation_logs`, and emitting UI notification signals)
- `src/resource/ResourceManager.cpp` (updated — hooked `runwayStatus` query to MySQL via DatabaseManager, added clear scope annotations for Days 19/21)
- `ui/RunwayAllocationDialog.h` (NEW — modal dialog interface for runway allocation with runway and flight selectors)
- `ui/RunwayAllocationDialog.cpp` (NEW — dialog implementation with live available runway dropdown, eligible flight table with emergency highlighting, operational compatibility checks, and validation error presentation)
- `ui/RunwaysView.h` (updated — added `m_allocateButton`, `m_releaseButton`, and slots `onAllocateClicked()`, `onReleaseClicked()`)
- `ui/RunwaysView.cpp` (updated — integrated Allocate Flight and Release Runway buttons in toolbar, added action-level RBAC enforcement for ATC, and wired modal allocation and release confirmation workflows)
- `tests/TestRunwayResourceAllocation.cpp` (NEW — 18 unit and integration tests covering model validation, RBAC at service boundary, live MySQL allocation lifecycle, conflict rejections, terminal status rejection, type compatibility, and runway release lifecycle)
- `CMakeLists.txt` (updated — registered `RunwayAllocationDialog.h/.cpp`, `RunwayAssignment.h`, and `TestRunwayResourceAllocation` executable and CTest test)
- `docs/DAY15_RUNWAY_RESOURCE_ALLOCATION_REPORT.md` (NEW — comprehensive Day 15 verification report)
- `PROJECT_STATUS.md` (updated — marked Day 15 complete)

### Architecture Established
- **Model**: `RunwayAssignment` entity capturing temporal assignment data (`assigned_at`, `released_at`, `status = 'Active'|'Released'|'Cancelled'`, `assigned_by`, `runway_id`, `flight_id`).
- **Application Service Layer**: Enforces action-level RBAC:
  - `action:RunwayAllocation`: Strictly restricted to ATC role. Rejects unauthenticated calls, Admins, and Operators.
  - `releaseRunway`: Permitted for ATC controllers, Airport Operators, and Admins.
- **Validation Engine**:
  1. Runway must exist and be in `Available` status. `Occupied` or `Maintenance` runways are rejected.
  2. Runway must not already possess an active assignment in `runway_assignments`.
  3. Flight must exist and be in non-terminal status (`Scheduled`, `Taxiing`, `Cleared`). Terminal statuses (`Landed`, `Departed`, `Cancelled`) are rejected.
  4. Runway operational type compatibility enforced (Landing runways only accept Arrival flights; Takeoff runways only accept Departure flights).
  5. Flight must not already possess an active assignment on any runway.
  6. Flight process in `scheduling_queue` must not be in `Completed` state.
- **Database Layer**: All operations use parameterized SQL queries across `runway_assignments`, `runways`, `flights`, and `operation_logs`. Audit logging records `RUNWAY_ASSIGN` and `RUNWAY_RELEASE` events.
- **GUI Layer**: Integrated `RunwayAllocationDialog` accessible via "🛬 Allocate Flight" button in `RunwaysView`. Shows real-time available runways, filterable compatible flights, and status feedback. "🛫 Release Runway" safely returns occupied runways to `Available` mode with user confirmation.

### Automated Test Results (18 / 18 Passed in TestRunwayResourceAllocation; 10 / 10 Test Suites Passed 100%)
- All 10 CTest suites pass 100% (220 total tests across suite):
  1. `TestDatabaseConnectivity`: Passed
  2. `TestAuthAndRBAC`: Passed
  3. `TestFlightManagement`: Passed
  4. `TestAircraftManagement`: Passed
  5. `TestProcessModel`: Passed
  6. `TestReadyQueueAndProcessStates`: Passed
  7. `TestPriorityScheduler`: Passed
  8. `TestSchedulingMetrics`: Passed
  9. `TestRunwayManagement`: Passed
  10. `TestRunwayResourceAllocation`: Passed (18/18 tests passed)

### Scope Boundaries Confirmed (What was NOT implemented)
- No dynamic emergency priority adjustments (Day 16).
- No starvation prevention or aging algorithms (Days 17–18).
- No mutual exclusion, mutexes, condition variables, or critical section locks (Days 19–20).
- No MySQL multi-statement ACID transaction management (`SELECT FOR UPDATE`, `COMMIT/ROLLBACK`) (Days 21–22).
- No multithreaded dispatching or background OS threads (Day 23).

---

## CURRENT MILESTONE
Phase 2 / Day 15 — Runway Resource Allocation — COMPLETE & VERIFIED

## NEXT MILESTONE (PENDING EXPLICIT USER APPROVAL)
Phase 2 / Day 16 — Dynamic Emergency Priority Changes
