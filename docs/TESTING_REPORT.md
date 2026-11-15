# Comprehensive Testing and Verification Report — SmartATC

## 1. Testing Environment Specification

| Component | Specification |
|---|---|
| **Operating System** | Microsoft Windows 11 Enterprise (64-bit) |
| **Compiler** | MinGW-w64 GCC 13.1.0 (64-bit) |
| **C++ Standard** | ISO C++17 (`-std=c++17`) |
| **GUI & Core Framework** | Qt 6.11.2 (Core, Gui, Widgets, Sql, Test) |
| **Build Automation** | CMake 3.19+ with Ninja build engine |
| **Database Server** | MySQL Server 8.0.46 (InnoDB Engine) on `127.0.0.1:3306` |
| **Test Automation Tool** | QtTest / CTest |

---

## 2. Test Execution Summary

The SmartATC verification harness comprises **19 registered CTest test suites**.

### Overall Execution Results:
- **Registered CTest Suites:** **19**
- **Passed Suites:** **19 (100% Pass Rate)**
- **Failed Suites:** **0**
- **Total Test Assertions Executed:** **370**
  - **Passed Assertions:** **369**
  - **Failed Assertions:** **0**
  - **Skipped Assertions:** **1** (`testOfflineDatabaseHandling` skipped during live database run)

> [!NOTE]
> **Dual-Mode Testing:**
> All 19 test suites were verified under two distinct operating modes:
> 1. **Offline / Sandboxed Mode:** Verified graceful fallback, in-memory simulations, and exception-free error reporting when MySQL is unreachable (Total time: 0.77 sec).
> 2. **Live MySQL Integration Mode:** Verified persistent SQL execution, ACID transactions, row-level locking, and foreign-key integrity against local MySQL 8.0.46 (Total time: 14.62 sec).

---

## 3. Comprehensive Test Suite Catalog

| # | Test Suite Name | Focus Area | Checks | Status |
|---|---|---|:---:|:---:|
| 1 | `TestDatabaseConnectivity` | Connection management, configuration parsing, QMYSQL driver | 16 | **PASS** |
| 2 | `TestAuthAndRBAC` | SHA-256 verification, session permissions, role enforcement | 24 | **PASS** |
| 3 | `TestFlightManagement` | Flight CRUD, status transitions, priority validation, emergencies | 22 | **PASS** |
| 4 | `TestAircraftManagement` | Fleet registration, capacity validation, foreign key deletion rules | 18 | **PASS** |
| 5 | `TestProcessModel` | PCB instantiation, state transitions, validation rules | 19 | **PASS** |
| 6 | `TestReadyQueueAndProcessStates` | FIFO queue, duplicate PID rejection, lookup methods | 17 | **PASS** |
| 7 | `TestPriorityScheduler` | Non-preemptive priority, arrival-time tie breaking, dispatch | 20 | **PASS** |
| 8 | `TestSchedulingMetrics` | Completion time, turnaround time, waiting time formulas | 14 | **PASS** |
| 9 | `TestRunwayManagement` | Runway statuses, maintenance mode, compatibility checks | 21 | **PASS** |
| 10 | `TestRunwayResourceAllocation` | Exclusive allocation, duplicate assignment rejection, release | 18 | **PASS** |
| 11 | `TestDynamicEmergencyPriority` | Emergency priority override to 1, priority restoration | 22 | **PASS** |
| 12 | `TestPriorityAging` | Elapsed waiting time aging, interval timer, queue reordering | 35 | **PASS** |
| 13 | `TestStarvationSimulation` | Repeatable simulation proving starvation prevention | 12 | **PASS** |
| 14 | `TestCriticalSectionAndMutualExclusion` | `std::mutex` critical sections, scoped lock guards | 18 | **PASS** |
| 15 | `TestConcurrentRunwayAllocation` | Multi-threaded race conditions, start barrier, thread joining | 15 | **PASS** |
| 16 | `TestDatabaseTransactions` | Atomic allocation & release transactions, rollback integrity | 16 | **PASS** |
| 17 | `TestACIDProperties` | Atomicity, consistency, row locks (`FOR UPDATE`), durability | 31 | **PASS** |
| 18 | `TestCompleteIntegration` | End-to-end scheduling-to-runway allocation workflow | 16 | **PASS** |
| 19 | `TestDashboardMonitoring` | KPI telemetry, zero-runway protection, audit logging | 16 | **PASS** |

---

## 4. Day 25 Defects Discovered and Resolved

During live integration testing against MySQL 8.0.46, four defects were discovered and resolved:

### Defect 1: Migration DDL Incompatibility with MySQL 8.0
- **Problem:** `sql/migration_day17_aging.sql` failed with MySQL syntax error 1064 on `ALTER TABLE scheduling_queue ADD COLUMN IF NOT EXISTS ...`.
- **Root Cause:** MySQL 8.0 does not support `IF NOT EXISTS` for column additions.
- **Resolution:** Removed the invalid `IF NOT EXISTS` clause; successfully executed the migration against `smartatc` to provision `ready_since` and `aging_level`.

### Defect 2: Integer Range Overflow in `aging_level` Column
- **Problem:** `TestPriorityAging` produced MySQL warning `"Out of range value for column 'aging_level' at row 1"`.
- **Root Cause:** Historical flight records with arrival times in the past produced large elapsed second values, resulting in computed aging levels exceeding the 255 maximum of MySQL's `TINYINT UNSIGNED`.
- **Resolution:** Updated `Process::calculateAgingLevels` to clamp accrued aging levels to 255.

### Defect 3: Invalid Column Name in Test Fixture Queries
- **Problem:** `TestDatabaseTransactions` and `TestACIDProperties` failed with fixture flight ID `0`, causing transaction rollbacks and InnoDB lock timeouts.
- **Root Cause:** Fixture insertion SQL referenced draft column name `emergency_status` instead of the normalized `is_emergency`.
- **Resolution:** Corrected all fixture queries to `is_emergency`.

### Defect 4: In-Memory Process Dispatch Failure when MySQL Connected
- **Problem:** `TestCompleteIntegration::testScenario10_RoleBasedAccessRestrictions` failed with `dispatchNextProcess == false`.
- **Root Cause:** `ApplicationService::dispatchNextProcess` attempted an unconditional `UPDATE scheduling_queue ...` whenever MySQL was open. Since the test process was purely in-memory, `numRowsAffected == 0` triggered an erroneous dispatch rejection.
- **Resolution:** Guarded database synchronization with `hasProcessForFlight(candidate.flightId)` so that in-memory unit testing processes complete without SQL errors, while persistent processes continue to update MySQL.

---

## 5. Manual GUI Verification

In addition to automated CTest execution, the complete application was verified via standalone execution:
- **Deployment Script:** Executed `scripts/deploy_runtime.ps1`, deploying 12 required Qt6, MinGW, and MySQL client libraries into `build/`.
- **Standalone Launch:** Verified `SmartATC.exe` launches without missing dependency errors.
- **Authentication Flow:** Verified login for `admin`, `atc_controller`, and `ops_user`.
- **Navigation Verification:** Confirmed that sidebar items route correctly and role permissions dynamically restrict editing buttons.
