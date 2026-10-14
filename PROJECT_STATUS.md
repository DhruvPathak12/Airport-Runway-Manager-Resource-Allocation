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
 
 ## CURRENT MILESTONE
 Day 6 — COMPLETE (Verified & Hardened)
 
 ## NEXT MILESTONE
 Day 7 — Flight Management


