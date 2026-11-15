# Module Design Specification — SmartATC

## 1. Overview of System Modules

SmartATC is architectured into six cohesive, modular subsystems:

1. **User & Role Management Module**
2. **Flight & Aircraft Management Module**
3. **Runway Infrastructure & Contention Module**
4. **Process & Priority Scheduling Module**
5. **Database Transaction & ACID Management Module**
6. **System Dashboard & Operational Monitoring Module**

---

## 2. Module 1: User & Role Management

### 2.1 Purpose & Scope
Provides user identity verification, session lifecycle management, and role-based authorization across all presentation and service interfaces.

### 2.2 Key Functionality
- Authenticates users against SHA-256 password digests.
- Populates the in-memory `Session` model.
- Restricts UI navigation tabs and action triggers based on user role (`Admin`, `ATC`, `Airport Operator`).
- Command-line account provisioning (`SmartATC --provision-user`) with non-echoing password input.

### 2.3 Relevant Files & Classes
- **Models:** `src/models/User.h`, `src/models/Session.h`, `src/models/Session.cpp`
- **Utilities:** `src/utils/PasswordUtils.h`, `src/utils/PasswordUtils.cpp`
- **Services:** `src/services/ApplicationService.h` (`login()`, `logout()`, `currentSession()`)
- **GUI:** `ui/LoginView.h`, `ui/LoginView.cpp`, `ui/MainWindow.cpp`
- **Tests:** `tests/TestAuthAndRBAC.cpp`

### 2.4 Data & OS/DBMS Concepts
- **Inputs:** Username string, plaintext password input.
- **Outputs:** Authenticated `Session` instance with assigned permissions list.
- **Database Interaction:** Queries `users` joined with `roles`. Updates `users.password_hash`.
- **Relevant Concepts:** Principle of Least Privilege, Access Control Lists (ACLs), Cryptographic Hashing.

---

## 3. Module 2: Flight & Aircraft Management

### 3.1 Purpose & Scope
Manages the airport's flight timetable, fleet inventory, aircraft capacities, flight lifecycle phases, and emergency distress declarations.

### 3.2 Key Functionality
- CRUD operations for flights (`Arrival`, `Departure`) and aircraft (`Active`, `Maintenance`, `Retired`).
- Real-time search and status filtering in Qt table widgets.
- Emergency declaration toggling (`is_emergency`) with dynamic scheduler priority elevation.
- Foreign key integrity verification before aircraft deletion.

### 3.3 Relevant Files & Classes
- **Models:** `src/models/Flight.h`, `src/models/Aircraft.h`
- **Services:** `ApplicationService.h` (`getAllFlights()`, `createFlight()`, `updateFlightStatus()`, `setFlightEmergency()`, `createAircraft()`, `deleteAircraft()`)
- **GUI:** `ui/FlightsView.h`, `ui/FlightsView.cpp`, `ui/FlightDialog.cpp`, `ui/AircraftView.h`, `ui/AircraftView.cpp`, `ui/AircraftDialog.cpp`
- **Tests:** `tests/TestFlightManagement.cpp`, `tests/TestAircraftManagement.cpp`

### 3.4 Data & OS/DBMS Concepts
- **Inputs:** Flight schedules, tail registrations, carrier names, emergency status toggles.
- **Outputs:** Populated flight and aircraft record models, UI table models.
- **Database Interaction:** Queries and updates `flights` and `aircraft` tables with parameterized SQL.
- **Relevant Concepts:** Relational Foreign Keys (`ON DELETE RESTRICT`), Entity Integrity, Domain Constraints.

---

## 4. Module 3: Runway Infrastructure & Contention

### 4.1 Purpose & Scope
Models airport runways as exclusive physical resources, enforces operation compatibility (Landing vs Takeoff), and demonstrates resource contention and mutual exclusion under multi-threaded load.

### 4.2 Key Functionality
- Runway configuration management (`Landing`, `Takeoff`, `Both`).
- Maintenance mode toggle (`Airport Operator` / `Admin`).
- Compatibility evaluation: matches flight type with runway capabilities.
- Multi-threaded race condition demonstration (`ConcurrentAllocationDemo`) using `std::thread` and `std::mutex`.

### 4.3 Relevant Files & Classes
- **Models:** `src/models/Runway.h`, `src/models/RunwayAssignment.h`
- **Resource Management:** `src/resource/ResourceManager.h`, `src/resource/ResourceManager.cpp`, `src/resource/ConcurrentAllocationDemo.h`, `src/resource/ConcurrentAllocationDemo.cpp`
- **GUI:** `ui/RunwaysView.h`, `ui/RunwaysView.cpp`, `ui/RunwayDialog.cpp`
- **Tests:** `tests/TestRunwayManagement.cpp`, `tests/TestRunwayResourceAllocation.cpp`, `tests/TestCriticalSectionAndMutualExclusion.cpp`, `tests/TestConcurrentRunwayAllocation.cpp`

### 4.4 Data & OS/DBMS Concepts
- **Inputs:** Runway ID, flight ID, worker thread count.
- **Outputs:** `AllocationResult` (`Success`, `RunwayOccupied`, `RunwayMaintenance`, `Incompatible`), `DemoReport`.
- **Database Interaction:** Reads and updates `runways.status` (`Available`, `Occupied`, `Maintenance`).
- **Relevant Concepts:** Critical Sections, Mutual Exclusion, Resource Contention, RAII Lock Guards, Thread Synchronization Barriers.

---

## 5. Module 4: Process & Priority Scheduling

### 5.1 Purpose & Scope
Simulates operating system process management, Process Control Blocks, ready-queue ordering, non-preemptive priority dispatching, and dynamic priority aging to prevent starvation.

### 5.2 Key Functionality
- Converts eligible flights into simulated Process Control Blocks (`Models::Process`).
- Enforces strict process state transitions (`Ready`, `Running`, `Completed`, `Blocked`).
- Executes Non-Preemptive Priority Scheduling with arrival-time tie breaking.
- Evaluates real-time wall-clock aging via `AgingEngine` (QTimer-driven).
- Calculates and visualizes Turnaround Time (TAT) and Waiting Time (WT).
- Runs controlled dual-track starvation simulations (`WithAging` vs `WithoutAging`).

### 5.3 Relevant Files & Classes
- **Models:** `src/models/Process.h`, `src/models/Process.cpp`
- **Scheduler Core:** `src/scheduler/Scheduler.h`, `src/scheduler/Scheduler.cpp`, `src/scheduler/ReadyQueue.h`, `src/scheduler/ReadyQueue.cpp`, `src/scheduler/AgingEngine.h`, `src/scheduler/AgingEngine.cpp`, `src/scheduler/StarvationSimulation.h`, `src/scheduler/StarvationSimulation.cpp`
- **GUI:** `ui/SchedulingView.h`, `ui/SchedulingView.cpp`
- **Tests:** `tests/TestProcessModel.cpp`, `tests/TestReadyQueueAndProcessStates.cpp`, `tests/TestPriorityScheduler.cpp`, `tests/TestSchedulingMetrics.cpp`, `tests/TestDynamicEmergencyPriority.cpp`, `tests/TestPriorityAging.cpp`, `tests/TestStarvationSimulation.cpp`

### 5.4 Data & OS/DBMS Concepts
- **Inputs:** Flight PCB, service duration, priority aging interval, evaluation timestamps.
- **Outputs:** Selected candidate process, dispatched process, computed CT, TAT, WT, aggregate metrics.
- **Database Interaction:** Synchronizes in-memory ready queue with persistent `scheduling_queue` table.
- **Relevant Concepts:** Process Control Block (PCB), Ready Queue, CPU Scheduling, Starvation, Priority Aging, Metrics Calculation.

---

## 6. Module 5: Database Transaction & ACID Management

### 6.1 Purpose & Scope
Provides transactional boundaries for runway allocation and release, guaranteeing Atomicity, Consistency, Isolation, and Durability across multi-statement SQL operations.

### 6.2 Key Functionality
- Manages connection pooling and configuration parsing via `DatabaseManager`.
- Executes atomic runway allocation: acquires row locks (`SELECT ... FOR UPDATE`), inserts assignment, updates runway, updates flight, and logs transaction.
- Executes atomic runway release: updates assignment status, resets runway status to `Available`, logs transaction.
- Performs automatic `ROLLBACK` on any statement failure.
- Provides transaction state reconciliation for uncertain-commit recovery.

### 6.3 Relevant Files & Classes
- **Database Core:** `src/database/DatabaseManager.h`, `src/database/DatabaseManager.cpp`
- **Service Integration:** `src/services/ApplicationService.h` (`allocateRunway()`, `releaseRunway()`, `reconcileTransactionState()`, `lastTransaction()`)
- **SQL Scripts:** `sql/schema.sql`, `sql/seed.sql`, `sql/transactions.sql`, `sql/migration_day17_aging.sql`
- **Tests:** `tests/TestDatabaseConnectivity.cpp`, `tests/TestDatabaseTransactions.cpp`, `tests/TestACIDProperties.cpp`, `tests/TestCompleteIntegration.cpp`

### 6.4 Data & OS/DBMS Concepts
- **Inputs:** Runway ID, flight ID, user session ID.
- **Outputs:** `TransactionSummary` (`Committed`, `Rolled Back`, timestamp, error message).
- **Database Interaction:** Multi-table transactions across `runways`, `flights`, `runway_assignments`, and `operation_logs`.
- **Relevant Concepts:** ACID Properties, Pessimistic Row Locking, Two-Phase Locking, Write-Ahead Logging (WAL).

---

## 7. Module 6: System Dashboard & Operational Monitoring

### 7.1 Purpose & Scope
Aggregates live operational telemetry from the database and OS scheduling subsystems, presenting a centralized, high-contrast, data-driven overview of airport activity.

### 7.2 Key Functionality
- Renders 12 Key Performance Indicator (KPI) cards covering flights, runways, utilization %, and OS processes.
- Mathematical zero-runway protection in utilization percentage calculation.
- Active emergency flight monitoring panel with instantaneous navigation deep-links.
- Visual capacity progress bar for runway utilization with adaptive color thresholds.
- Telemetry cards for scheduling policy, aging engine status, and next candidate preview.
- Real-time transaction outcome diagnostics and immutable audit logs table.

### 7.3 Relevant Files & Classes
- **Data Structures:** `ApplicationService::DashboardSummary`, `ApplicationService::OperationLogEntry`
- **Service API:** `ApplicationService.h` (`getDashboardSummary()`, `getRunwayUtilizationPercentage()`, `getRecentOperationLogs()`)
- **GUI:** `ui/DashboardView.h`, `ui/DashboardView.cpp`
- **Tests:** `tests/TestDashboardMonitoring.cpp`

### 7.4 Data & OS/DBMS Concepts
- **Inputs:** Database state aggregates, in-memory scheduler metrics, polling clock.
- **Outputs:** Live summary view, progress bar styling, tabular audit listings.
- **Database Interaction:** Aggregate queries (`COUNT`, `SUM`, `CASE`) and joined audit log selection.
- **Relevant Concepts:** Real-Time Telemetry, Defensive Boundary Programming, Zero-Division Safety, Event-Driven vs Polling Update Loops.
