# Day 24 — System Dashboard and Operational Monitoring Report

## Phase
Phase 2 — Remaining 60% of OS and DBMS Implementation

## Date & Milestone
- **Milestone:** Phase 2 / Day 24 — System Dashboard and Operational Monitoring
- **Status:** Complete & Fully Verified
- **Starting Git Commit:** `583c465` (`feat: integrate OS scheduling and DBMS resource workflows`)
- **Repository:** Smart Air Traffic Control Management System (`SmartATC`)

---

## 1. Executive Summary & Objective

Day 24 implements a comprehensive, centralized, visually polished, and strictly data-driven System Dashboard and Operational Monitoring module in the Smart Air Traffic Control Management System (`SmartATC`).

Before Day 24, SmartATC provided specialized individual views for Flights, Aircraft, Runways, Scheduling, and Starvation Simulation. However, the system lacked a high-level operational overview consolidating live air traffic activity, OS process scheduling metrics, real-time runway resource utilization, emergency alerts, atomic transaction telemetry, and immutable audit logs into a single coherent screen.

The Day 24 Dashboard satisfies this requirement by providing:
1. **Air Traffic Operations KPIs:** Total Flights, Scheduled Flights, Active Flights (taxiing/cleared), and Emergency Flights count.
2. **Runway Resource Utilization:** Total Runways, Available Runways, Occupied Runways, Maintenance Runways, and a mathematically protected Runway Utilization Percentage (`(Occupied / Total) * 100`).
3. **OS Process Scheduling Telemetry:** Ready Queue Depth, Running Processes count, Completed Processes count, Scheduling Policy designation, Priority Aging status and interval, Average Waiting Time (WT), Average Turnaround Time (TAT), and Next Process Candidate identification.
4. **Emergency Flight Monitoring:** Dedicated high-visibility alert section highlighting active emergency aircraft with airline, priority, runway status, and immediate ATC action links. When zero emergencies exist, displays a reassuring normal operations indicator.
5. **Database Transaction & Audit Log Telemetry:** Diagnostic summary card of the last executed database transaction (`RUNWAY_ASSIGN`, `RUNWAY_RELEASE`) with COMMIT/ROLLBACK outcome, timestamp, and an interactive audit log table showing the 10 most recent persistent database operations with user attribution.
6. **Strict Zero-Fabrication Rule:** All metrics and statistics are computed directly from live MySQL database tables and in-memory OS scheduling/resource manager objects. No hardcoded demonstration values, mock data, or random number generators are used.
7. **Offline and Fault Resilience:** Gracefully handles database offline or connection drops without throwing exceptions or crashing the UI, providing explicit user-facing status indicators and preserving in-memory scheduler metrics.

---

## 2. Integrated System Architecture & Data Flow

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              DASHBOARD VIEW (GUI)                               │
│  Header Banner │ KPI Cards │ Emergency Alert │ Runway Bar │ OS Metrics │ Logs   │
└──────────────────────────────────────┬──────────────────────────────────────────┘
                                       │ 5-second polling timer + change signals
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                            APPLICATION SERVICE (FACADE)                         │
│   • getDashboardSummary()             • getRunwayUtilizationPercentage()        │
│   • getRecentOperationLogs()          • lastTransaction()                       │
└───────────────────┬─────────────────────────────────────────────┬───────────────┘
                    │                                             │
      (In-Memory OS Telemetry)                      (Persistent MySQL Telemetry)
                    ▼                                             ▼
┌───────────────────────────────────────┐   ┌─────────────────────────────────────┐
│               Scheduler               │   │           DatabaseManager           │
│   • readyQueue().size()               │   │   • Connection state checking       │
│   • selectNextProcess() [Candidate]   │   │   • QSqlDatabase instance           │
│   • calculateAggregateMetrics()       │   └──────────────────┬──────────────────┘
│   • hasRunningProcess()               │                      │ Parameterized SQL
└───────────────────┬───────────────────┘                      ▼
                    │                       ┌─────────────────────────────────────┐
┌───────────────────┴───────────────────┐   │           MySQL 8 Database          │
│              AgingEngine              │   │   • flights (counts, emergencies)   │
│   • isRunning()                       │   │   • runways (status aggregates)     │
│   • intervalSeconds()                 │   │   • runway_assignments (occupancy)  │
│   • readySince duration               │   │   • operation_logs (audit trail)    │
└───────────────────────────────────────┘   └─────────────────────────────────────┘
```

### Data Aggregation Cycle:
1. **Trigger:** The dashboard refreshes upon initial user login, when selecting the `"Dashboard"` navigation tab in `MainWindow`, upon receiving subsystem signals (`flightsChanged`, `runwaysChanged`, `processesChanged`, `agingEvaluated`), and automatically every 5 seconds via an internal `QTimer`.
2. **Aggregation Method:** `ApplicationService::getDashboardSummary()` coordinates the extraction of real-time state:
   - Queries `Scheduler`, `ReadyQueue`, and `AgingEngine` for queue depth, running process, completed metrics, and next candidate.
   - Queries `lastTransaction()` for the latest database transaction state and failure diagnosis.
   - Executes targeted SQL aggregate queries against `flights`, `runways`, `runway_assignments`, and `operation_logs` (if connected).
   - If MySQL is offline, catches the condition cleanly, sets `dbConnected = false`, zero-initializes database counters, and returns available in-memory metrics without raising fatal errors.
3. **UI Rendering:** `DashboardView::renderSummary()` unpacks the `DashboardSummary` struct and updates all QLabel badges, progress bars, tables, and alert banners in a single smooth UI pass.

---

## 3. Dashboard Structure & Layout

The dashboard is structured into a clean, modern, high-contrast, responsive interface built with native Qt Widgets (`QVBoxLayout`, `QHBoxLayout`, `QGridLayout`, `QScrollArea`, `QTableWidget`, `QProgressBar`):

### 3.1 Header Section
- **Title and Description:** Professional header displaying `"System Dashboard & Operational Overview"` and subtitle.
- **Live Clock:** Digital clock updating every second in `yyyy-MM-dd hh:mm:ss` format.
- **System Status Badge:** Visual indicator showing `● DB: Online (smartatc@127.0.0.1:3306)` (green) or `○ DB: Disconnected / Offline` (amber).
- **User / RBAC Badge:** Pill badge displaying `👤 <User> (<Role>)` reflecting the authenticated session.
- **Manual Refresh Button:** Allows ATC controllers or administrators to force an immediate refresh on demand.

### 3.2 Key Performance Indicator (KPI) Cards
Organized into two responsive rows of styled cards with clear metrics and contextual color codes:
- **Flight Activity KPIs:**
  - `Total Flights`: Overall flight count in database.
  - `Scheduled Flights`: Flights in initial scheduled status awaiting dispatch.
  - `Active Flights`: Flights currently taxiing or cleared for runway operations.
  - `Emergency Flights`: Active flights under emergency distress (amber/red).
- **Runway Resource KPIs:**
  - `Total Runways`: Total runway physical assets configured in airport.
  - `Available Runways`: Runways currently free and operational (green).
  - `Occupied Runways`: Runways actively servicing an aircraft (blue).
  - `Maintenance Runways`: Runways under inspection/repair (red).
- **Process Scheduling & Utilization KPIs:**
  - `Runway Utilization`: Real-time percentage of runways occupied (`XX.X%`).
  - `Ready Queue Depth`: Count of runnable simulated processes waiting for execution.
  - `Running Processes`: Count of currently executing processes (0 or 1).
  - `Completed Processes`: Total processes finished since startup.

### 3.3 Active Emergency Flight Monitoring Section
- Displayed prominently below KPI cards.
- **Emergency Condition Present:** Renders an urgent warning banner with alert icon, warning text, and a table detailing Flight Number, Airline, Operation Type, Priority, Route (Origin → Destination), Assigned Runway, and an **"Open Flights"** shortcut button.
- **Normal Condition (0 Emergencies):** Renders a calm, reassuring green banner: `"✓ Normal Operations: No active emergency flights detected."`

### 3.4 Runway Resource Utilization Section
- **Visual Progress Bar:** Modern `QProgressBar` displaying utilization from 0% to 100%, dynamically colored based on capacity (green: <60%, yellow: 60–85%, red: >85%).
- **Runway Details Table:** Compact table listing all runways with Runway Number, Operation Type (Arrival, Departure, Both), Current Status (Available, Occupied, Maintenance), and Length/Heading.
- **Navigation Shortcut:** `"Manage Runways →"` button routing directly to `RunwaysView`.

### 3.5 OS Process Scheduling & Telemetry Section
- **Scheduling Policy Card:** Highlights `"Non-Preemptive Priority Scheduling (Tie-breakers: Arrival Time, Flight ID)"`.
- **Aging Engine Card:** Shows whether Priority Aging is Active, along with the configured interval (e.g. `Every 30s`).
- **Performance Metrics:** Real-time Average Waiting Time (`XX.Xs`) and Average Turnaround Time (`XX.Xs`).
- **Next Candidate in Queue Card:** Identifies the top-priority process that will execute next (Flight Number, Process ID, Effective Priority, Burst Time). If queue is empty, displays `Queue Idle (No waiting candidates)`.
- **Navigation Shortcut:** `"Open Scheduler →"` button routing directly to `SchedulingView`.

### 3.6 Flight Activity & Audit Logs Section
- **Recent Flights Table:** Compact preview of the 8 latest flights with status and flight types.
- **Transaction Monitor Card:** Real-time card showing the last executed database transaction:
  - Operation: `RUNWAY_ASSIGN` / `RUNWAY_RELEASE`
  - Runway & Flight IDs
  - Status: `Committed` (green) / `Rolled Back` (red)
  - Execution Timestamp
- **Operation Audit Log Table:** Immutable history table showing the 10 most recent system events: Log ID, User, Action, Flight, Runway, Transaction Status, Message, and Timestamp.

---

## 4. Metric Definitions & Mathematical Formulas

Every metric displayed on the dashboard is rigorously defined and calculated as follows:

| Metric Name | Source Subsystem | Formula / Definition | Edge Case Defense |
|---|---|---|---|
| **Runway Utilization %** | DBMS `runways` / `ResourceManager` | $\text{Utilization} = \frac{\text{Occupied Runways}}{\text{Total Runways}} \times 100.0$ | If $\text{Total Runways} = 0$, returns exactly $0.0\%$ (prevents divide-by-zero). |
| **Ready Queue Depth** | OS `ReadyQueue` | $N_{\text{Ready}} = \text{readyQueue.size()}$ | Safe on empty queue ($N_{\text{Ready}} = 0$). |
| **Running Processes** | OS `Scheduler` | $1$ if `hasRunningProcess()`, else $0$. | Non-preemptive single CPU core model constraint. |
| **Completed Processes** | OS `Scheduler` | $N_{\text{Completed}} = \text{completedProcesses.size()}$ | Resets cleanly on scheduler reset. |
| **Average Waiting Time** | OS `Scheduler` | $\bar{W} = \frac{\sum_{i=1}^N \text{WaitingTime}_i}{N}$ | If $N = 0$, returns $0.0\text{s}$ (well-defined). |
| **Average Turnaround Time** | OS `Scheduler` | $\overline{\text{TAT}} = \frac{\sum_{i=1}^N \text{TurnaroundTime}_i}{N}$ | If $N = 0$, returns $0.0\text{s}$ (well-defined). |
| **Effective Priority** | OS `Process` / `AgingEngine` | $\text{effPriority} = \begin{cases} 1 & \text{if isEmergency} \\ \max(1, \text{basePriority} - \lfloor \frac{\Delta t}{T_{\text{aging}}} \rfloor) & \text{otherwise} \end{cases}$ | Bounded within $[1, 5]$. Higher priority has lower numeric value. |

---

## 5. Safe Division & Edge Case Protection

The dashboard includes explicit defensive guards against boundary conditions and edge cases:

1. **Zero-Runway Condition:**
   - In a newly provisioned database, an airport with no registered runways, or when operating in disconnected mode, the total runway count is 0.
   - Traditional calculation `(occupied / total) * 100` would trigger a floating-point exception or produce `NaN`/`+Inf`.
   - Both `ApplicationService::getRunwayUtilizationPercentage()` and `ApplicationService::getDashboardSummary()` enforce:
     ```cpp
     if (summary.totalRunways > 0) {
         summary.runwayUtilization = (static_cast<double>(summary.occupiedRunways) /
                                      static_cast<double>(summary.totalRunways)) * 100.0;
     } else {
         summary.runwayUtilization = 0.0;
     }
     ```
   - Verified by unit test `testZeroRunwaySafeDivision()`.

2. **Empty Queue & Zero Completed Processes:**
   - When the scheduler has not dispatched any processes or when the ready queue is empty:
     - `readyQueueDepth = 0`
     - `runningProcesses = 0`
     - `hasHighestPriorityWaiting = false`
     - `averageWaitingTime = 0.0`
     - `averageTurnaroundTime = 0.0`
   - The UI displays `"Queue Idle (No waiting candidates)"` rather than attempting to render a null PCB.
   - Verified by unit test `testEmptyQueueAndProcessMetrics()`.

3. **Zero Flights in Database:**
   - Database queries return clean zeros for all flight aggregations. The recent flights table displays a friendly empty state.

---

## 6. Offline and Fault Resilience

SmartATC can be executed without an active MySQL daemon (e.g., during developer unit testing, sandbox runs, or network disruptions):

1. **No Fatal Exceptions or Crashes:**
   - When `checkDatabaseConnection()` returns `false`, `ApplicationService::getDashboardSummary()` skips all SQL execution and sets `dbConnected = false`.
   - `getRecentOperationLogs()` returns an empty list and sets an informative error message.
   - `getRunwayUtilizationPercentage()` returns `0.0%`.
2. **Preservation of In-Memory Telemetry:**
   - Even when MySQL is offline, the in-memory OS scheduling telemetry remains 100% active and functional.
   - Ready queue depth, running process states, priority aging timers, and candidate selection are computed directly from C++ heap structures and displayed on the dashboard.
3. **Visual Feedback:**
   - The Dashboard header prominently displays `○ DB: Disconnected / Offline (showing in-memory telemetry)`.
   - An explanatory warning is surfaced to the operator indicating that persistent historical logs are temporarily unavailable.

---

## 7. Role-Based Access Control (RBAC) Integration

The dashboard complies with SmartATC's centralized RBAC security model defined in `Session.cpp`:

1. **Dashboard Page Visibility:**
   - `session.hasPermission("page:Dashboard")` evaluates to `true` for all valid authenticated roles: `Admin`, `ATC`, and `Airport Operator`.
   - The Dashboard is placed as the first navigation item (`NAV_ITEMS[0]`) and serves as the primary home landing page after login.
2. **Action Privileges:**
   - Navigation links within the dashboard respect action privileges. For example, clicking `"Manage Runways"` navigates to `RunwaysView`, where modification controls (Add, Edit, Maintenance) are conditionally enabled based on whether the user has `action:RunwayMaintenance` (Operator) or `action:AdministerRunway` (Admin).
   - Emergency flight actions (declaring or clearing emergencies) remain restricted to `ATC` controllers.
3. **Session Attribution:**
   - The top header pill reflects the current user's authenticated identity and role (`👤 atc_controller (ATC)` or `👤 admin (Admin)`).

---

## 8. Navigation Integration

The Dashboard seamlessly integrates with other application modules through Qt signal-slot bindings:
- Clicking **"Manage Runways →"** emits `navigateToRunways`, triggering `MainWindow` to switch `m_navList` to index 3 (`RunwaysView`).
- Clicking **"Open Scheduler →"** emits `navigateToScheduling`, switching `m_navList` to index 4 (`SchedulingView`).
- Clicking **"Open Flights →"** emits `navigateToFlights`, switching `m_navList` to index 1 (`FlightsView`).
- When navigating back to the Dashboard (`onNavItemChanged(0)`), `m_dashboardView->refreshDashboard()` is automatically invoked to ensure fresh statistics.

---

## 9. Verification & Automated Test Suite

A dedicated unit and integration test suite, `tests/TestDashboardMonitoring.cpp`, was created and registered in `CMakeLists.txt` as `TestDashboardMonitoring` (CTest #19).

### Test Cases Implemented in `TestDashboardMonitoring`:
1. `testZeroRunwaySafeDivision()`: Verifies that 0 total runways produces exactly 0.0% utilization without `NaN` or exceptions.
2. `testRunwayUtilizationFormula()`: Tests mathematical correctness of utilization percentages across 0%, 25%, 50%, 75%, 100%, and partial ratios.
3. `testEmptyQueueAndProcessMetrics()`: Verifies safe handling of an empty ready queue and initial zeroed metrics.
4. `testInMemoryQueueSynchronization()`: Enqueues multiple processes into `ReadyQueue` and verifies that `readyQueueDepth`, candidate process identification, and priority matching reflect in-memory state.
5. `testSchedulerMetricsAggregation()`: Dispatches and completes a simulated process, verifying turnaround and waiting time metric propagation into `DashboardSummary`.
6. `testNextCandidateIdentification()`: Verifies that when normal and emergency processes are enqueued, the emergency process is identified as the next candidate.
7. `testOfflineDashboardSummaryResilience()`: Verifies that `getDashboardSummary()` completes cleanly when the database is disconnected.
8. `testOfflineRecentOperationLogsResilience()`: Verifies that `getRecentOperationLogs()` returns empty list and error text when offline.
9. `testOfflineRunwayUtilizationResilience()`: Verifies that `getRunwayUtilizationPercentage()` returns 0.0% and error text when offline.
10. `testLiveDatabaseDashboardSummary()`: (Conditional) Compares dashboard KPI counts against direct SQL counts when live MySQL is available.
11. `testEmergencyFlightDetection()`: (Conditional) Verifies active emergency flight count and list accuracy against database.
12. `testRecentOperationLogsPaginationAndOrdering()`: (Conditional) Verifies limit enforcement and descending order of audit logs.
13. `testLastTransactionExtraction()`: (Conditional) Verifies retrieval of transaction operation and status fields.
14. `testDashboardConsistencyWithSubsystems()`: Asserts that dashboard summary properties strictly equal the corresponding getters on `Scheduler`, `ReadyQueue`, and `AgingEngine`.

### Full Test Suite Results (19 / 19 Passed 100%):
```
Test project D:/Projects/Airport Runway Manager &Resource Allocation/SmartATC/build
      Start  1: TestDatabaseConnectivity
 1/19 Test  #1: TestDatabaseConnectivity ................   Passed    0.28 sec
      Start  2: TestAuthAndRBAC
 2/19 Test  #2: TestAuthAndRBAC .........................   Passed    0.61 sec
      Start  3: TestFlightManagement
 3/19 Test  #3: TestFlightManagement ....................   Passed    0.63 sec
      Start  4: TestAircraftManagement
 4/19 Test  #4: TestAircraftManagement ..................   Passed    0.61 sec
      Start  5: TestProcessModel
 5/19 Test  #5: TestProcessModel ........................   Passed    0.63 sec
      Start  6: TestReadyQueueAndProcessStates
 6/19 Test  #6: TestReadyQueueAndProcessStates ..........   Passed    0.63 sec
      Start  7: TestPriorityScheduler
 7/19 Test  #7: TestPriorityScheduler ...................   Passed    0.61 sec
      Start  8: TestSchedulingMetrics
 8/19 Test  #8: TestSchedulingMetrics ...................   Passed    0.64 sec
      Start  9: TestRunwayManagement
 9/19 Test  #9: TestRunwayManagement ....................   Passed    0.63 sec
      Start 10: TestRunwayResourceAllocation
10/19 Test #10: TestRunwayResourceAllocation ............   Passed    0.63 sec
      Start 11: TestDynamicEmergencyPriority
11/19 Test #11: TestDynamicEmergencyPriority ............   Passed    0.62 sec
      Start 12: TestPriorityAging
12/19 Test #12: TestPriorityAging .......................   Passed    0.63 sec
      Start 13: TestStarvationSimulation
13/19 Test #13: TestStarvationSimulation ................   Passed    0.60 sec
      Start 14: TestCriticalSectionAndMutualExclusion
14/19 Test #14: TestCriticalSectionAndMutualExclusion ...   Passed    0.64 sec
      Start 15: TestConcurrentRunwayAllocation
15/19 Test #15: TestConcurrentRunwayAllocation ..........   Passed    0.65 sec
      Start 16: TestDatabaseTransactions
16/19 Test #16: TestDatabaseTransactions ................   Passed    0.62 sec
      Start 17: TestACIDProperties
17/19 Test #17: TestACIDProperties ......................   Passed    0.64 sec
      Start 18: TestCompleteIntegration
18/19 Test #18: TestCompleteIntegration .................   Passed    0.62 sec
      Start 19: TestDashboardMonitoring
19/19 Test #19: TestDashboardMonitoring .................   Passed    0.04 sec

100% tests passed out of 19
Total Test time (real) = 10.96 sec
```

---

## 10. Files Modified and Created

| File Path | Status | Functional Description |
|---|---|---|
| `src/services/ApplicationService.h` | Modified | Defined `OperationLogEntry` and `DashboardSummary` data structures; declared `getDashboardSummary()`, `getRecentOperationLogs()`, and `getRunwayUtilizationPercentage()`. |
| `src/services/ApplicationService.cpp` | Modified | Implemented live data aggregation across MySQL and OS scheduler subsystems, zero-division protection, and offline resilience handling. |
| `ui/DashboardView.h` | Created | Header for `DashboardView` widget class, defining layout containers, KPI card references, tables, badges, timer, and navigation signals. |
| `ui/DashboardView.cpp` | Created | Implementation of `DashboardView`, creating the visual header, 12 KPI cards, emergency alert section, runway utilization progress bar, scheduler telemetry card, recent flights table, transaction monitor, and operation logs table. |
| `ui/MainWindow.h` | Modified | Added forward declaration and member pointer for `DashboardView *m_dashboardView`. |
| `ui/MainWindow.cpp` | Modified | Integrated `DashboardView` into index 0 of `m_pageStack`, wired navigation deep-links to other views, and added refresh triggers on navigation and login. |
| `tests/TestDashboardMonitoring.cpp` | Created | Comprehensive 14-point test suite verifying calculations, edge cases, divide-by-zero defense, offline handling, and live DB integration. |
| `CMakeLists.txt` | Modified | Added `DashboardView.h` and `DashboardView.cpp` to `SmartATC` sources; registered `TestDashboardMonitoring` executable and CTest test runner. |
| `docs/DAY24_DASHBOARD_MONITORING_REPORT.md` | Created | This complete technical milestone report for Day 24. |
| `PROJECT_STATUS.md` | Modified | Updated project status marking Day 24 complete and specifying Day 25 boundaries. |

---

## 11. Git Commit & Repository Information

- **Branch:** `main`
- **Starting Commit:** `583c465` (`feat: integrate OS scheduling and DBMS resource workflows`)
- **New Commit:** `feat: implement system dashboard and operational monitoring`
- **Scope Enforced:** Day 24 Dashboard and Monitoring completed in full. No Day 25 polish or final presentation tasks were started.

