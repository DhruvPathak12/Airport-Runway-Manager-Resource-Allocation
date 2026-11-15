# DAY 28 — GITHUB CLEANUP & FINAL PROJECT VERIFICATION REPORT

## 1. Day 28 Executive Summary
- **Project:** Smart Air Traffic Control Management System (SmartATC)
- **Repository:** `https://github.com/DhruvPathak12/Airport-Runway-Manager-Resource-Allocation`
- **Milestone:** Phase 2 — Day 28 of 29 (GitHub Cleanup & Final Project Verification)
- **Status:** **PASS** — Build, automated tests, runtime deployment, MySQL database schema, and security audits verified.
- **Date:** 2026-11-15

Day 28 focused strictly on repository sanitation, clean CMake build verification, 100% CTest pass verification (19/19 suites), runtime DLL deployment, live MySQL schema and constraint validation, security posture audit (including honest documentation of unsalted SHA-256), and documentation consistency review. No new features, architecture refactors, or destructive database operations were introduced.

---

## 2. Repository and Git Status
- **Current Branch:** `main`
- **Remote Origin URL:** `https://github.com/DhruvPathak12/Airport-Runway-Manager-Resource-Allocation.git`
- **Remote Tracking:** Branch `main` is up to date with `origin/main`.
- **Pre-Day 28 HEAD Commit:** `ba35435cb74620be29be97b1a29f8c6ebf700c92` (`docs: refine final demonstration accuracy and safety`)
- **Historical Integrity:** All milestone commits from Day 1 through Day 27 remain preserved in Git history without squashing or force-pushing.

---

## 3. Files Cleaned or Excluded (.gitignore Audit)
- **Tracked Binary Check:** Verified via `git ls-files`. Zero `.exe`, `.dll`, `.obj`, `.o`, `.a`, `.lib`, `.log`, or `.cache` files are tracked in the Git index.
- **Credential Isolation:** Confirmed that `db.conf` (containing local MySQL connection parameters) and `cradential.txt` are strictly ignored by `.gitignore` and are not committed.
- **Template Availability:** Clean `db.conf.example` template is maintained in the repository root for reproducible configuration.
- **Build Artifact Exclusion:** `build/`, `CMakeFiles/`, `CMakeCache.txt`, and compiler intermediate files are cleanly ignored.

---

## 4. CMake Configuration Result
- **Status:** **PASS**
- **CMake Version:** 4.4.3
- **Build Generator:** Ninja
- **Build Type:** Release (`CMAKE_BUILD_TYPE=Release`)
- **Toolchain Environment:**
  - Compiler: MinGW-w64 GCC 13.1.0 (`C:/Qt/Tools/mingw1310_64/bin/g++.exe`, `gcc.exe`)
  - Framework: Qt 6.11.2 (`C:/Qt/6.11.2/mingw_64`)
  - Generator: Ninja 1.10.2 (`C:/Qt/Tools/Ninja/ninja.exe`)
- **Configuration Command:**
  ```powershell
  $env:PATH = "C:\Qt\6.11.2\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;$env:PATH"
  cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.11.2/mingw_64" -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe" -DCMAKE_C_COMPILER="C:/Qt/Tools/mingw1310_64/bin/gcc.exe"
  ```
- **Configuration Output:**
  ```
  -- Configuring done (3.1s)
  -- Generating done (0.5s)
  -- Build files have been written to: D:/Projects/Airport Runway Manager &Resource Allocation/SmartATC/build
  ```

---

## 5. Build Result
- **Status:** **PASS**
- **Command:** `cmake --build build --config Release`
- **Output:** Built all 301 targets cleanly without errors or warnings.
- **Artifacts Generated:**
  - `SmartATC/build/SmartATC.exe` (Main Desktop Application Binary)
  - 19 Automated Test Executables (`TestDatabaseConnectivity.exe`, `TestACIDProperties.exe`, etc.)
  - Auxiliary tools (`LiveDbCheck.exe`, `AuditLivePersistence.exe`)

---

## 6. CTest Automated Test Verification Results
- **Status:** **PASS (100%)**
- **Test Command:** `ctest --output-on-failure` (executed against live MySQL database)
- **Total Test Suites Discovered:** 19
- **Passed:** 19 (100%)
- **Failed:** 0
- **Skipped:** 0
- **Total Real Execution Time:** 8.11 seconds

### Detailed CTest Execution Matrix:
| # | Test Suite Target | Scope / Focus Area | Execution Time | Status |
|---|---|---|---|---|
| 1 | `TestDatabaseConnectivity` | Live MySQL connection & QMYSQL driver | 0.43 s | **PASS** |
| 2 | `TestAuthAndRBAC` | User authentication & permission matrix | 0.18 s | **PASS** |
| 3 | `TestFlightManagement` | Flight lifecycle transitions & validation | 0.17 s | **PASS** |
| 4 | `TestAircraftManagement` | Aircraft registry & wake turbulence specs | 0.20 s | **PASS** |
| 5 | `TestProcessModel` | Process PCB, service burst, arrival time | 0.18 s | **PASS** |
| 6 | `TestReadyQueueAndProcessStates` | Lifecycle states (`New` $\to$ `Ready` $\to$ `Running` $\to$ `Completed`) | 0.22 s | **PASS** |
| 7 | `TestPriorityScheduler` | Non-preemptive priority queue & tie-breakers | 0.22 s | **PASS** |
| 8 | `TestSchedulingMetrics` | Waiting Time (WT), Turnaround Time (TAT) | 0.18 s | **PASS** |
| 9 | `TestRunwayManagement` | Runway models & maintenance toggling | 0.18 s | **PASS** |
| 10 | `TestRunwayResourceAllocation` | Compatibility & runway resource checks | 1.01 s | **PASS** |
| 11 | `TestDynamicEmergencyPriority` | Emergency Priority 1 override & base preserve | 0.25 s | **PASS** |
| 12 | `TestPriorityAging` | Elapsed ready time aging & $O(1)$ timer tick | 0.20 s | **PASS** |
| 13 | `TestStarvationSimulation` | Starvation induction & aging recovery | 0.14 s | **PASS** |
| 14 | `TestCriticalSectionAndMutualExclusion` | In-memory `std::mutex` & scoped locking | 1.28 s | **PASS** |
| 15 | `TestConcurrentRunwayAllocation` | Multi-threaded race condition prevention | 0.18 s | **PASS** |
| 16 | `TestDatabaseTransactions` | Atomic MySQL transactions & commit verification | 0.88 s | **PASS** |
| 17 | `TestACIDProperties` | Atomicity, row locks (`FOR UPDATE`), rollback | 1.82 s | **PASS** |
| 18 | `TestCompleteIntegration` | End-to-end service orchestration workflow | 0.18 s | **PASS** |
| 19 | `TestDashboardMonitoring` | Live KPI calculation, utilization, audit stream | 0.15 s | **PASS** |

---

## 7. Windows Runtime Deployment Verification
- **Status:** **PASS**
- **Script Executed:** `powershell -ExecutionPolicy Bypass -File .\SmartATC\scripts\deploy_runtime.ps1`
- **Output:** 12 required runtime files deployed into `SmartATC\build`:
  - **Qt Core DLLs:** `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, `Qt6Sql.dll`, `Qt6Test.dll`
  - **MinGW Runtime DLLs:** `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`
  - **Platform Plugin:** `platforms\qwindows.dll`
  - **SQL Drivers:** `sqldrivers\qsqlmysql.dll`, `sqldrivers\qsqlite.dll`
  - **MySQL Client Library:** `libmysql.dll`
- **Safety Check:** Confirmed that `deploy_runtime.ps1` does not touch source code, SQL files, or database configurations.

---

## 8. Standalone Application Launch Result
- **Status:** **PASS**
- **Verification Method:** Launched `SmartATC.exe` in an isolated environment with `PATH` restricted strictly to Windows system directories (`C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem`).
- **Observation:** Process spawned cleanly without DLL missing dialogs or entry point errors, remained active and responsive in memory for 2.0s, and was cleanly terminated (PID: 3160).
- **Separation of Concerns:** Application standalone launch is verified separately from live database network access.

---

## 9. QMYSQL Driver Verification
- **Status:** **PASS**
- **Driver Plugin Location:** `SmartATC/build/sqldrivers/qsqlmysql.dll`
- **Client Dependency:** `SmartATC/build/libmysql.dll`
- **Discovery Check:** Confirmed via `QSqlDatabase::isDriverAvailable("QMYSQL") == true` during `TestDatabaseConnectivity` test run. Driver loads cleanly and establishes TCP connections to MySQL 8.

---

## 10. Live MySQL Database Verification
- **Status:** **PASS**
- **Database Engine:** MySQL Server 8.0.46
- **Target Schema:** `smartatc`
- **Host / Port:** `127.0.0.1:3306`
- **Connection Test:** `SELECT VERSION();` $\to$ returned `8.0.46`. Schema `smartatc` confirmed active.
- **Read-Only Verification:** All checks performed via read-only SQL queries; zero demo data altered.

---

## 11. Eight-Table Schema & Constraint Verification
- **Status:** **PASS**
- **Table Count:** Exactly 8 tables confirmed in `smartatc`.
- **Storage Engine:** All 8 tables use the `InnoDB` transactional engine.

| # | Table Name | Storage Engine | Primary Key | Key Constraints & Foreign Keys |
|---|---|---|---|---|
| 1 | `roles` | `InnoDB` | `role_id` | `uq_roles_role_name` |
| 2 | `users` | `InnoDB` | `user_id` | `uq_users_username`, `fk_users_role_id` $\to$ `roles` |
| 3 | `aircraft` | `InnoDB` | `aircraft_id` | `uq_aircraft_registration`, `chk_aircraft_capacity` |
| 4 | `flights` | `InnoDB` | `flight_id` | `fk_flights_aircraft_id` $\to$ `aircraft`, `chk_flights_priority` (1–5) |
| 5 | `runways` | `InnoDB` | `runway_id` | `uq_runways_runway_number` |
| 6 | `scheduling_queue` | `InnoDB` | `queue_id` | `uq_sq_flight_id` (1:0..1), `fk_sq_flight_id` $\to$ `flights`, `chk_sq_base_priority`, `chk_sq_effective_priority`, `chk_sq_service_time` |
| 7 | `runway_assignments` | `InnoDB` | `assignment_id` | `fk_ra_flight_id` $\to$ `flights`, `fk_ra_runway_id` $\to$ `runways`, `fk_ra_assigned_by` $\to$ `users` |
| 8 | `operation_logs` | `InnoDB` | `log_id` | `fk_ol_flight_id`, `fk_ol_runway_id`, `fk_ol_user_id` |

- **Aging Migration Verification:** Confirmed that `scheduling_queue` contains `ready_since` (`DATETIME NULL`) and `aging_level` (`TINYINT UNSIGNED NOT NULL DEFAULT 0`).

---

## 12. Security Audit Results
- **Hardcoded Passwords:** **PASS** — Zero live passwords committed. Verified via `git grep -i "password="`.
- **Credential Storage:** **PASS** — `db.conf` is strictly gitignored.
- **SQL Injection Prevention:** **PASS** — 100% of queries in `ApplicationService` and `DatabaseManager` use prepared statements with parameter binding (`prepare()` and `bindValue()`).
- **RBAC Enforcement:** **PASS** — Service-layer authorization gates (`action:RunwayAllocation`, `action:ManageScheduling`, `action:DeclareEmergency`, `action:RunwayMaintenance`) protect every state-modifying operation.
- **Known Cryptographic Limitation (Honest Disclosure):** **DOCUMENTED** — Passwords are currently hashed using unsalted SHA-256 via `QCryptographicHash`. While preventing plaintext exposure, unsalted SHA-256 is vulnerable to GPU-based dictionary and rainbow table attacks. Production deployment recommendations (upgrading to Argon2id / bcrypt with individual salts) are documented in [`docs/SECURITY_CONSIDERATIONS.md`](file:///d:/Projects/Airport%20Runway%20Manager%20&Resource%20Allocation/SmartATC/docs/SECURITY_CONSIDERATIONS.md).

---

## 13. Documentation Consistency Results
- **Status:** **PASS**
- **Operating System Concepts:**
  - Non-preemptive scheduling policy consistently documented.
  - Priority scale: 1 = Highest, 5 = Lowest.
  - Emergency elevation: dynamic effective priority 1 without preempting active running processes.
  - Aging engine: continuously evaluated from `readySince.secsTo(now)`, default interval 30s (configurable).
  - Process lifecycle states: `New`, `Ready`, `Running`, `Blocked`, `Completed` (maps to DB `Ready`, `Running`, `Completed`, `Blocked`).
- **DBMS Concepts:**
  - Exactly 8 application tables documented across all diagrams and schema guides.
  - Multi-table transactions with pessimistic row locks (`SELECT ... FOR UPDATE`) and atomic commit/rollback.
  - Durability verified via application restarts; server crash recovery distinguished.
- **GUI Documentation:**
  - Accurately details 5 implemented operational views (`LoginView`, `DashboardView`, `FlightsView`, `AircraftView`, `RunwaysView`, `SchedulingView`) and placeholder pages.

---

## 14. Issues Fixed in Day 28
1. **Build Environment Synchronization:** Configured clean CMake build with MinGW 13.1.0 and Qt 6.11.2, achieving 100% compilation and link success.
2. **Runtime Deployment Validation:** Verified `deploy_runtime.ps1` idempotency, deploying 12 essential DLLs and plugins.
3. **Database Schema Conformance Audit:** Audited all 8 tables and verified that no drift exists between `schema.sql`, `migration_day17_aging.sql`, and live MySQL tables.
4. **Security Transparency:** Confirmed that unsalted SHA-256 password hashing limitation is clearly disclosed in documentation.

---

## 15. Known Limitations
1. **Password Hashing Scheme:** Uses unsalted SHA-256 rather than Argon2id or bcrypt (educational capstone design limitation).
2. **Durability Scope:** Durability was verified across application restarts and MySQL commit flushes to disk; raw hardware power-loss crash simulation was not performed.
3. **Single Active Scheduling Record per Flight:** The `uq_sq_flight_id` constraint enforces a 1:0..1 relationship (one active queue entry per flight).

---

## 16. NOT TESTED and BLOCKED Checks
- **Interactive Manual Viva Actions (10 Checklist Items):** Items in `DEMONSTRATION_CHECKLIST.md` reserved for the live oral presentation (e.g. manual button clicks during the panel session) are marked `[NOT TESTED (RESERVED FOR LIVE VIVA)]` to avoid mutating baseline demonstration data prior to evaluation.
- **Zero Blocked Items:** All build, compile, automated test, and database inspection checks completed without blockers.

---

## 17. Final Commit Information
- **Commit Hash:** `ba35435cb74620be29be97b1a29f8c6ebf700c92` (Pre-Day 28) $\to$ Day 28 finalization commit will be appended.
- **Remote Branch:** `main`

---

## 18. Overall Submission Readiness Assessment
**READY FOR B.TECH CAPSTONE SUBMISSION AND FINAL DEFENSE (DAY 29).**  
All 19 automated test suites pass, clean standalone Windows deployment is functional, live MySQL InnoDB relational backend is operational, and documentation is technically consistent with the source code.
