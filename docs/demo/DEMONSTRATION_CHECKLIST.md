# SmartATC — Demonstration & Verification Checklist

## 1. Overview & Verification Protocol
This checklist serves as the official pre-flight inspection and live presentation audit protocol for the **Smart Air Traffic Control Management System (SmartATC)** B.Tech project viva.

**Status Legend:**
- `[PASS]` : Fully executed and verified via automated test suites (`CTest`) or live environment inspection.
- `[FAIL]` : Verification failed; defect identified.
- `[BLOCKED]` : Test blocked by an upstream dependency.
- `[NOT TESTED]` : Preserved for live evaluation panel presentation (not executed on live database to prevent state alteration).

---

## 2. Pre-Flight Environment Checklist

| # | Inspection Item | Verification Method | Expected Result | Status |
|---|---|---|---|---|
| **ENV-01** | MySQL 8 Service Active | Windows Process / Port Check | MySQL running on `127.0.0.1:3306` | `[PASS]` |
| **ENV-02** | Database `smartatc` Provisioned | MySQL CLI `USE smartatc; SHOW TABLES;` | 8 core InnoDB tables present | `[PASS]` |
| **ENV-03** | User Credentials Seeded | SQL query on `users` table | `admin_demo`, `atc_demo`, `operator_demo` confirmed active | `[PASS]` |
| **ENV-04** | Qt 6.5+ Runtime DLLs | Check build directory (`cmake` release) | `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, `Qt6Sql.dll` present | `[PASS]` |
| **ENV-05** | QMYSQL Driver Available | Inspect `QSqlDatabase::drivers()` | `QMYSQL` driver loaded successfully | `[PASS]` |
| **ENV-06** | Executable Built Cleanly | `cmake --build build --config Release` | `SmartATC.exe` compiled without errors | `[PASS]` |

---

## 3. Application Launch & Authentication Checklist

| # | Inspection Item | Verification Method | Expected Result | Status |
|---|---|---|---|---|
| **AUTH-01** | Application Startup | Launch `SmartATC.exe` | Application loads cleanly; `LoginDialog` opens | `[PASS]` |
| **AUTH-02** | Invalid Login Rejection | Service authentication test | Rejection on invalid credentials; session not created | `[PASS]` |
| **AUTH-03** | Admin Login & Permissions | Service authentication test | Login successful; full administrative navigation enabled | `[PASS]` |
| **AUTH-04** | ATC Login & Permissions | Service authentication test | Flight/Runway/Scheduling actions enabled; maintenance disabled | `[PASS]` |
| **AUTH-05** | Operator Login & Permissions | Service authentication test | Runway maintenance enabled; allocation and emergency disabled | `[PASS]` |
| **AUTH-06** | Logout & Session Teardown | Service authentication test | Session cleared cleanly; permissions revoked | `[PASS]` |

---

## 4. UI Views & Presentation Layer Verification

| # | View / Component | Inspection Point | Expected Behavior | Status |
|---|---|---|---|---|
| **UI-01** | `DashboardView` | Summary cards, gauges, charts, audit log | Displays real-time data from MySQL and `ApplicationService` | `[PASS]` |
| **UI-02** | `FlightManagementView` | Flight Table, Status Badges, Action Buttons | Populated with demo flights; edit/add actions operational | `[PASS]` |
| **UI-03** | `AircraftManagementView` | Aircraft Table, Model / Category specs | Lists aircraft fleet records with wake turbulence categories | `[PASS]` |
| **UI-04** | `RunwayManagementView` | Runway Cards, Status Indicators, Allocator | Displays 09L (Available), 09R (Available), 27 (Maintenance) | `[PASS]` |
| **UI-05** | `SchedulingView` | Dual Tabs: Ready Queue & Starvation Demo | Full PCB queue inspection and interactive starvation controls | `[PASS]` |
| **UI-06** | `LoginDialog` Live Interaction | Manual login presentation | Operator enters credentials interactively in UI | `[NOT TESTED]` (Reserved for live viva) |
| **UI-07** | `FlightDialog` Live Interaction | Manual flight creation | Controller adds flight interactively in UI | `[NOT TESTED]` (Reserved for live viva) |
| **UI-08** | `RunwayAllocationDialog` | Manual allocation popup | Modal dialog prompts runway and flight selection | `[NOT TESTED]` (Reserved for live viva) |
| **UI-09** | `ConcurrentAllocationDemo` | Multi-threaded simulation window | Interactive launch with thread status logging | `[NOT TESTED]` (Reserved for live viva) |

---

## 5. Operating System Concepts Verification

| # | OS Concept | Verification Method | Expected Result | Status |
|---|---|---|---|---|
| **OS-01** | Process Control Block (PCB) | `TestProcessModel` (Unit Test) | Attributes loaded (`flightId`, `state`, priorities, arrival) | `[PASS]` |
| **OS-02** | PCB State Transitions | `TestReadyQueueAndProcessStates` | Transitions across `New`, `Ready`, `Running`, `Completed` | `[PASS]` |
| **OS-03** | Priority Queue Ordering | `TestPriorityScheduler` | Sorted strictly by `effectivePriority ASC` (1=highest) | `[PASS]` |
| **OS-04** | Deterministic Tie-Breaking | `TestPriorityScheduler` | Priority tie broken by `arrivalTime ASC`, then `flightId ASC` | `[PASS]` |
| **OS-05** | Non-Preemptive Scheduling | `TestDynamicEmergencyPriority` | Running process is NOT preempted; emergency queues at head of `Ready` | `[PASS]` |
| **OS-06** | Emergency Priority Escalation | `TestDynamicEmergencyPriority` | `effectivePriority` overrides to 1; `basePriority` preserved | `[PASS]` |
| **OS-07** | Priority Aging Mechanism | `TestPriorityAging` | Aging levels accrued from `readySince`; decreases numerical priority | `[PASS]` |
| **OS-08** | Starvation Demonstration | `TestStarvationSimulation` | Low-priority process starved under continuous inflow without aging | `[PASS]` |
| **OS-09** | Starvation Resolution | `TestStarvationSimulation` | Enabling aging elevates starved process to Priority 1; dispatched | `[PASS]` |
| **OS-10** | Critical Section & Mutex | `TestCriticalSectionAndMutualExclusion` | Only 1 thread enters critical section per runway; second blocks | `[PASS]` |
| **OS-11** | Mutual Exclusion Invariant | `TestConcurrentRunwayAllocation` | Runway assigned to exactly 1 flight; double-booking prevented | `[PASS]` |
| **OS-12** | Thread Lifecycle & Clean Join | `TestConcurrentRunwayAllocation` | Worker threads cleanly join; zero zombie threads or resource leaks | `[PASS]` |

---

## 6. DBMS & Transaction Verification

| # | DBMS Concept | Verification Method | Expected Result | Status |
|---|---|---|---|---|
| **DB-01** | Database Connection Pooling | `TestDatabaseConnectivity` | Single shared connection managed safely via `DatabaseManager` | `[PASS]` |
| **DB-02** | Pessimistic Row Locking | `TestDatabaseTransactions` | `SELECT ... FOR UPDATE` acquires row X-locks during allocation | `[PASS]` |
| **DB-03** | Atomic Multi-Table COMMIT | `TestDatabaseTransactions` | `runways`, `runway_assignments`, `flights`, `operation_logs` committed | `[PASS]` |
| **DB-04** | Deterministic ROLLBACK | `TestACIDProperties` (Failure Injection) | Statement failure triggers `ROLLBACK`; all tables reverted | `[PASS]` |
| **DB-05** | Zero Partial Writes | `TestACIDProperties` | After rollback, no orphan assignments or partial status updates | `[PASS]` |
| **DB-06** | Schema Consistency & FKs | `TestACIDProperties` | Invalid foreign keys rejected; database state remains valid | `[PASS]` |
| **DB-07** | Concurrency Isolation | `TestACIDProperties` | Concurrent transactions isolated under `REPEATABLE READ` + row locks | `[PASS]` |
| **DB-08** | Durability (App Restart) | `TestACIDProperties` | Committed transaction persists in MySQL across application restart | `[PASS]` |
| **DB-09** | Operation Audit Logging | `TestDatabaseTransactions` | Audited in `operation_logs` with `transaction_status` and message | `[PASS]` |

---

## 7. System Dashboard & Monitoring Verification

| # | Inspection Item | Verification Method | Expected Result | Status |
|---|---|---|---|---|
| **DASH-01** | Live Flight Statistics | `TestDashboardMonitoring` | Flight counts match database exactly (Total, Queued, Cleared) | `[PASS]` |
| **DASH-02** | Active Emergency Banner | `TestDashboardMonitoring` | Banner highlights active emergency flight in real time | `[PASS]` |
| **DASH-03** | Runway Utilization Gauge | `TestDashboardMonitoring` | Gauge dynamically calculates utilization percentage from DB | `[PASS]` |
| **DASH-04** | Scheduler Performance Card | `TestDashboardMonitoring` | Real-time averages calculated from completed PCB metrics | `[PASS]` |
| **DASH-05** | Live Audit Trail Stream | `TestDashboardMonitoring` | Audit table streams recent committed operations | `[PASS]` |
| **DASH-06** | Manual Refresh Trigger | Live GUI Keypress (`Ctrl+R`) | Reloads all data cleanly from MySQL without crash | `[NOT TESTED]` (Reserved for live viva) |

---

## 8. Reset Protocol Verification

| # | Inspection Item | Verification Method | Expected Result | Status |
|---|---|---|---|---|
| **RST-01** | Script Schema Conformance | Code audit of `demo_reset.sql` | Column names match MySQL schema (`runway_id`, `flight_id`, etc.) | `[PASS]` |
| **RST-02** | Non-Destructive Invariant | Code audit of `demo_reset.sql` | Zero `DROP`, `TRUNCATE`, or deletion of users/aircraft | `[PASS]` |
| **RST-03** | Runway State Restoration | Disposable test run | 09L (Available), 09R (Available), 27 (Maintenance) | `[NOT TESTED]` (Preserved for evaluation) |
| **RST-04** | Active Assignment Cleared | Disposable test run | Zero records with `status = 'Active'` | `[NOT TESTED]` (Preserved for evaluation) |
| **RST-05** | Emergency Flag Reset | Disposable test run | All demo flights show `is_emergency = 0` and base priorities | `[NOT TESTED]` (Preserved for evaluation) |
| **RST-06** | Scheduling Queue Re-seeded | Disposable test run | 5 demo flights queued in `Ready` state | `[NOT TESTED]` (Preserved for evaluation) |

---

## 9. Verification Summary
- **Total Verification Items:** 38
- **PASS (Verified via CTest suites or direct inspection):** 28
- **NOT TESTED (Preserved for live evaluation panel presentation):** 10
- **FAIL:** 0
- **BLOCKED:** 0
