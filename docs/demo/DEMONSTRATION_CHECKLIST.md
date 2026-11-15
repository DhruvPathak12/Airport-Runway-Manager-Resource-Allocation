# SmartATC — Comprehensive Demonstration & Verification Checklist

## 1. Overview
This checklist serves as the official pre-flight inspection and live presentation audit protocol for the **Smart Air Traffic Control Management System (SmartATC)** B.Tech project viva.

All items must be verified prior to the examination panel session.

**Status Legend:**
- `[PASS]` : Fully verified, functioning as designed without defects.
- `[FAIL]` : Verification failed; defect identified.
- `[BLOCKED]` : Test blocked by upstream dependency.
- `[NOT TESTED]` : Optional or unexecuted verification item.

---

## 2. Pre-Flight Environment Checklist

| # | Inspection Item | Verification Method | Expected Result | Status |
|---|---|---|---|---|
| **ENV-01** | MySQL 8 Service Active | Windows Services / `sc query mysql` | MySQL running on `127.0.0.1:3306` | `[PASS]` |
| **ENV-02** | Database `smartatc` Provisioned | MySQL CLI `USE smartatc; SHOW TABLES;` | 6 core InnoDB tables present | `[PASS]` |
| **ENV-03** | User Credentials Seeded | SQL query on `users` table | `admin_demo`, `atc_demo`, `operator_demo` present with SHA-256 hashes | `[PASS]` |
| **ENV-04** | Qt 6.5+ Runtime DLLs | Check build directory (`qmake` / `cmake`) | `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, `Qt6Sql.dll` present | `[PASS]` |
| **ENV-05** | QMYSQL Driver Available | Inspect `QSqlDatabase::drivers()` | `QMYSQL` driver loaded successfully | `[PASS]` |
| **ENV-06** | Executable Built Cleanly | `cmake --build build --config Release` | `SmartATC.exe` compiled without errors | `[PASS]` |

---

## 3. Application Launch & Authentication Checklist

| # | Inspection Item | Verification Method | Expected Result | Status |
|---|---|---|---|---|
| **AUTH-01** | Application Startup | Double click `SmartATC.exe` | Application loads cleanly; `LoginDialog` opens | `[PASS]` |
| **AUTH-02** | Invalid Login Rejection | Enter `admin_demo` / `wrongpass` | Error label: `"Invalid username or password."` | `[PASS]` |
| **AUTH-03** | Admin Login | Enter `admin_demo` / `Admin@1234` | Login successful; full administrative navigation enabled | `[PASS]` |
| **AUTH-04** | ATC Login & Permissions | Enter `atc_demo` / `ATC@1234` | Flight/Runway/Scheduling actions enabled; maintenance disabled | `[PASS]` |
| **AUTH-05** | Operator Login & Permissions | Enter `operator_demo` / `Operator@1234` | Runway maintenance enabled; allocation and emergency disabled | `[PASS]` |
| **AUTH-06** | Logout & Session Teardown | Click **Logout** button | Session cleared; returns cleanly to `LoginDialog` | `[PASS]` |

---

## 4. UI Views & Dialogs Verification

| # | View / Dialog | Inspection Point | Expected Behavior | Status |
|---|---|---|---|---|
| **UI-01** | `DashboardView` | Summary cards, gauges, charts, audit log | Displays real-time data from MySQL and `ApplicationService` | `[PASS]` |
| **UI-02** | `FlightManagementView` | Flight Table, Status Badges, Action Buttons | Lists all BOM demonstration flights; edit/add actions active | `[PASS]` |
| **UI-03** | `AircraftManagementView` | Aircraft Table, Model / Category specs | Lists aircraft `VT-AT1` through `VT-AT4` with wake turb. classes | `[PASS]` |
| **UI-04** | `RunwayManagementView` | Runway Cards, Status Indicators, Allocator | Displays 09L (Available), 09R (Available), 27 (Maintenance) | `[PASS]` |
| **UI-05** | `SchedulingView` | Dual Tabs: Ready Queue & Starvation Demo | Full PCB queue inspection and interactive starvation controls | `[PASS]` |
| **UI-06** | `LoginDialog` | Modal dialog, secure password field | EchoMode set to `Password`; input validation active | `[PASS]` |
| **UI-07** | `FlightDialog` | Create / Edit flight modal dialog | Form fields validate flight number, airline, priority 1-5 | `[PASS]` |
| **UI-08** | `RunwayAllocationDialog` | Resource assignment modal dialog | Dropdowns filter available flights and eligible runways | `[PASS]` |
| **UI-09** | `ConcurrentAllocationDemo` | Multi-threaded simulation dialog | Real-time console showing thread synchronization logs | `[PASS]` |

---

## 5. Operating System Concepts Verification

| # | OS Concept | Verification Method | Expected Result | Status |
|---|---|---|---|---|
| **OS-01** | Process Control Block (PCB) | Inspect Process attributes in `SchedulingView` | `flightId`, `state`, `basePriority`, `effectivePriority`, timestamps loaded | `[PASS]` |
| **OS-02** | PCB State Transitions | Click **Step Next Process** | Process transitions from `READY` to `RUNNING` to `TERMINATED` | `[PASS]` |
| **OS-03** | Priority Queue Ordering | Inspect Ready Queue table ordering | Sorted strictly by `effectivePriority ASC` (1=highest, 5=lowest) | `[PASS]` |
| **OS-04** | Deterministic Tie-Breaking | Check position of `6E-202` and `AI-303` | Both Priority 3; `6E-202` scheduled first due to earlier arrival time | `[PASS]` |
| **OS-05** | Non-Preemptive Scheduling | Step `AI-101` to `RUNNING`, then declare emergency | `AI-101` remains `RUNNING`; emergency process queues at head of `READY` | `[PASS]` |
| **OS-06** | Emergency Priority Override | Declare Emergency on `AI-303` | `effectivePriority` becomes 1; `basePriority` remains 3; jumps to queue head | `[PASS]` |
| **OS-07** | Priority Aging Mechanism | Step aging clock by 60-second intervals | `effectivePriority` decrements by 1 every 60s; never drops below 1 | `[PASS]` |
| **OS-08** | Starvation Demonstration | Starvation Tab: Inflow without Aging | Low-priority flight `UK-505` wait time increases; remains unscheduled | `[PASS]` |
| **OS-09** | Starvation Resolution | Starvation Tab: Enable Dynamic Aging | `UK-505` effective priority elevates to 1; successfully scheduled | `[PASS]` |
| **OS-10** | Critical Section & Mutex | Launch `ConcurrentAllocationDemo` | Only 1 thread enters critical section per runway; second thread blocked | `[PASS]` |
| **OS-11** | Mutual Exclusion Invariant | Examine post-contention allocation table | Runway 09L assigned to exactly 1 flight; double-booking impossible | `[PASS]` |
| **OS-12** | Thread Lifecycle & Join | Thread execution completion | Worker threads cleanly join; zero zombie threads or memory leaks | `[PASS]` |

---

## 6. DBMS & Transaction Verification

| # | DBMS Concept | Verification Method | Expected Result | Status |
|---|---|---|---|---|
| **DB-01** | Database Connection Pooling | Launch application | `DatabaseManager` initializes single shared connection safely | `[PASS]` |
| **DB-02** | Pessimistic Row Locking | Code inspection & runtime trace | `SELECT ... FOR UPDATE` acquires row X-locks during allocation | `[PASS]` |
| **DB-03** | Atomic Multi-Table COMMIT | Allocate Runway 09L to AI-101 | `runways`, `runway_assignments`, `flights`, `operation_logs` update atomically | `[PASS]` |
| **DB-04** | Deterministic ROLLBACK | Trigger conflict or injected failure | All statements roll back; database returns to pre-transaction state | `[PASS]` |
| **DB-05** | Zero Partial Writes | Check DB after failed allocation | No orphaned assignment records or partial status updates exist | `[PASS]` |
| **DB-06** | Schema Consistency & FKs | Attempt allocation with invalid ID | MySQL foreign key constraint rejects statement; transaction rolls back | `[PASS]` |
| **DB-07** | Isolation Verification | Concurrent allocation via threads | Transactions execute in isolation; zero dirty reads or race conditions | `[PASS]` |
| **DB-08** | Durability Verification | Terminate app after commit, restart | Committed allocation persists identically across application restarts | `[PASS]` |
| **DB-09** | Operation Audit Logging | Inspect `operation_logs` table | Records stored with user ID, action, details, and transaction status | `[PASS]` |

---

## 7. System Dashboard & Monitoring Verification

| # | Inspection Item | Verification Method | Expected Result | Status |
|---|---|---|---|---|
| **DASH-01** | Live Flight Statistics | Compare Dashboard metrics to `flights` table | Flight counts match database exactly (Total, Queued, Cleared) | `[PASS]` |
| **DASH-02** | Active Emergency Banner | Declare emergency on AI-303 | Banner highlights red with emergency details and flight number | `[PASS]` |
| **DASH-03** | Runway Utilization Gauge | Allocate 1 of 2 available runways | Gauge indicates 50% / 66.7% utilization dynamically | `[PASS]` |
| **DASH-04** | Scheduler Performance Card | Inspect Avg Wait Time & Turnaround | Real-time averages calculated from completed PCBs | `[PASS]` |
| **DASH-05** | Live Audit Trail Stream | Execute runway allocation | Audit table updates immediately with latest committed action | `[PASS]` |
| **DASH-06** | Manual Refresh Trigger | Press `Ctrl+R` or click **Refresh** | All metrics refresh from MySQL without flickering or data corruption | `[PASS]` |

---

## 8. Reset Protocol Verification

| # | Inspection Item | Verification Method | Expected Result | Status |
|---|---|---|---|---|
| **RST-01** | Non-Destructive SQL Reset | Execute `demo_reset.sql` in MySQL | Runs cleanly in < 0.2s without error | `[PASS]` |
| **RST-02** | Runway State Restoration | Query `runways` table after reset | 09L (Available), 09R (Available), 27 (Maintenance) | `[PASS]` |
| **RST-03** | Active Assignment Cleared | Query `runway_assignments` after reset | Zero records with `status = 'Active'` | `[PASS]` |
| **RST-04** | Emergency Flag Reset | Query `flights` table after reset | All demo flights show `emergency_flag = 0` and base priorities | `[PASS]` |
| **RST-05** | Ready Queue Re-seeded | Query `scheduling_queue` after reset | 5 demo flights queued in `Ready` state | `[PASS]` |
| **RST-06** | User Accounts Preserved | Query `users` table after reset | `admin_demo`, `atc_demo`, `operator_demo` untouched | `[PASS]` |

---

## 9. Final Sign-Off

- **Checklist Completed By:** SmartATC Engineering Team
- **Date of Verification:** 2026-11-15
- **Overall Status:** `READY FOR FINAL B.TECH EVALUATION PANEL`
