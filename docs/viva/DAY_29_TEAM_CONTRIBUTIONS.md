# SmartATC — 4-Member Technical Contribution & Viva Defense Guide
## Equal Division of Operating Systems (OS) and Database Management Systems (DBMS)

---

## 1. Team Structure & Git Authorship Transparency

SmartATC is a full-stack, desktop Air Traffic Control and Runway Resource Management simulation developed as a collaborative 4-member B.Tech capstone project combining core concepts from **Operating Systems (OS)** and **Database Management Systems (DBMS)**.

> [!IMPORTANT]
> **Git Authorship Evidence Notice:**  
> In the Git repository history (`git log`), commits were committed and pushed through a single developer identity (`Dhruv Pathak` / `DhruvPathak12`), who acted as the project's **Repository Maintainer and Lead Integrator** on behalf of the group. 
> 
> Therefore, Git commit authorship alone does not provide an isolated per-member commit breakdown. The technical division of responsibilities documented below reflects the agreed module ownership, architectural boundaries, test suites authored, and domain deliverables established across the team lifecycle. Every team member holds equal ownership over both an **OS subsystem** and a **DBMS subsystem**.

---

## 2. Comprehensive 4-Member Responsibility Matrix

| Team Member | Functional Domain | OS Concepts Owned | DBMS Concepts Owned | Primary Source Files | Database Tables Handled | Automated Tests Authored |
|:---|:---|:---|:---|:---|:---|:---|
| **Member 1** | **Process Scheduling & Queue Persistence** | Process Control Block (PCB), Ready Queue, Non-Preemptive Priority CPU Dispatcher, Turnaround/Waiting/Completion Time Metrics | `scheduling_queue` Table Schema, Process State Persistence, Queue Startup Sync, Timestamp Tracking | `src/models/Process.*`, `src/scheduler/ReadyQueue.*`, `src/scheduler/Scheduler.*`, `ui/SchedulingView.*` (Ready Queue Tab) | `scheduling_queue` | `TestProcessModel`, `TestReadyQueueAndProcessStates`, `TestPriorityScheduler`, `TestSchedulingMetrics` (4 Suites) |
| **Member 2** | **Dynamic Priority, Aging & Fleet Relational Modeling** | Starvation Prevention, Wall-Clock Aging Engine, Dynamic Emergency Priority Preemption, Starvation Verification Simulation | Relational Fleet Schema (`aircraft`, `flights`), 1:N Referential Integrity, Priority/Status `CHECK` Constraints, Dynamic Status Transitions | `src/models/Flight.h`, `src/models/Aircraft.h`, `src/scheduler/AgingEngine.*`, `src/scheduler/StarvationSimulation.*`, `ui/FlightsView.*`, `ui/AircraftView.*` | `aircraft`, `flights` | `TestPriorityAging`, `TestStarvationSimulation`, `TestDynamicEmergencyPriority`, `TestFlightManagement`, `TestAircraftManagement` (5 Suites) |
| **Member 3** | **Critical Sections, Mutual Exclusion & ACID Transactions** | Runway Critical Section Protection, `std::mutex`, RAII `ScopedResourceLock`, Multithreaded Contention (`std::thread`) | ACID Transaction Engine (`START TRANSACTION`, `COMMIT`, `ROLLBACK`), InnoDB Pessimistic Row Locking (`SELECT ... FOR UPDATE`), Failure Injection | `src/resource/ResourceManager.*`, `src/resource/ConcurrentAllocationDemo.*`, `src/database/DatabaseManager.*`, `src/models/Runway.*`, `ui/RunwaysView.*` | `runways`, `runway_assignments` | `TestCriticalSectionAndMutualExclusion`, `TestConcurrentRunwayAllocation`, `TestDatabaseTransactions`, `TestACIDProperties`, `TestRunwayManagement`, `TestRunwayResourceAllocation` (6 Suites) |
| **Member 4** | **Security/RBAC, System Orchestration & Analytical Monitoring** | Least-Privilege Protection Model, End-to-End Dispatch-Runway Binding (`scheduleAndAllocateNext`), Resource Contention Resolution | Role-Based Access Control (`roles`, `users`), SHA-256 Hash Auth, Transactional Audit Trail (`operation_logs`), Dashboard SQL Aggregations | `src/models/User.h`, `src/models/Session.*`, `src/utils/PasswordUtils.*`, `ui/LoginView.*`, `ui/DashboardView.*`, `src/services/ApplicationService.*` | `roles`, `users`, `operation_logs`, System Aggregates | `TestAuthAndRBAC`, `TestDashboardMonitoring`, `TestCompleteIntegration`, `TestDatabaseConnectivity` (4 Suites) |

---

## 3. Detailed Individual Member Contribution Profiles

---

### Member 1: Process Scheduling & Queue Persistence

#### 1. Technical Domain Summary
- **Operating Systems:** Process Control Block (PCB) architecture, Ready Queue priority ordering, Non-Preemptive CPU Scheduling, Turnaround Time (TAT), Waiting Time (WT), and Completion Time (CT) calculation.
- **Database Systems:** Relational persistence of process states in `scheduling_queue`, arrival/completion timestamp tracking, and database-to-RAM queue recovery (`syncReadyQueueFromDatabase`).

#### 2. Key Implemented Features
- **Process Control Block (`Models::Process`):** Modeled simulated operating system processes representing flights, tracking PID, arrival timestamp, service burst time, base priority, effective priority, and finite state machine (`New`, `Ready`, `Running`, `Blocked`, `Completed`).
- **Thread-Safe Ready Queue (`SchedulerComponents::ReadyQueue`):** Deterministic priority queue ordered by: (1) Effective Priority ($1 = \text{Highest}$), (2) Earlier arrival timestamp, (3) Lower Process ID (PID).
- **Non-Preemptive CPU Dispatcher (`SchedulerComponents::Scheduler`):** Dispatches highest-priority eligible processes from `Ready` to `Running`, clearing the running register upon burst expiration.
- **Turnaround & Waiting Time Metrics:** Implemented OS performance formulas:
  $$\text{Turnaround Time (TAT)} = \text{Completion Time (CT)} - \text{Arrival Time (AT)}$$
  $$\text{Waiting Time (WT)} = \text{TAT} - \text{Burst/Service Time (ST)}$$
- **Queue Database Synchronization:** Authored `syncReadyQueueFromDatabase()` ensuring that waiting processes in `scheduling_queue` survive system reboots and reload seamlessly on launch.

#### 3. Primary Source Files
- `src/models/Process.h` & `src/models/Process.cpp`
- `src/scheduler/ReadyQueue.h` & `src/scheduler/ReadyQueue.cpp`
- `src/scheduler/Scheduler.h` & `src/scheduler/Scheduler.cpp`
- `ui/SchedulingView.h` & `ui/SchedulingView.cpp` (Live Ready Queue Tab)

#### 4. Primary Database Table Handled
- **`scheduling_queue`**: `queue_id` (PK), `flight_id` (FK), `arrival_time`, `ready_since`, `base_priority`, `effective_priority`, `aging_level`, `service_time`, `process_state`, `waiting_time`, `turnaround_time`, `completion_time`.

#### 5. Automated CTest Suites Authored
1. `TestProcessModel`: State machine transitions, validation logic, burst boundaries.
2. `TestReadyQueueAndProcessStates`: Enqueue/dequeue ordering, duplicate rejection, state filtering.
3. `TestPriorityScheduler`: Dispatching candidate selection, non-preemptive lock enforcement.
4. `TestSchedulingMetrics`: Verification that $\text{TAT} \ge \text{Service Time}$ and $\text{WT} \ge 0$.

#### 6. Spoken Viva Defense Speeches
- **60-Second Explanation:**
  > "In SmartATC, I owned the OS Process Modeling and Database Queue Persistence subsystem. On the OS side, I engineered the Process Control Block (`Process`) and the `ReadyQueue`, implementing non-preemptive priority scheduling where Priority 1 is highest. I implemented the classical OS metric calculations for Turnaround Time, Waiting Time, and Completion Time. On the DBMS side, I designed the `scheduling_queue` table and built the database synchronization logic (`syncReadyQueueFromDatabase`), which loads persistent process states into memory upon system launch. I validated my implementation across 4 automated CTest suites."
- **2-Minute Explanation:**
  > "My responsibility in SmartATC bridged Operating System CPU scheduling with DBMS queue persistence. 
  > 
  > In an air traffic control environment, flights awaiting runway access behave identically to processes awaiting CPU cores in an operating system. I designed the C++ `Process` class as a complete Process Control Block (PCB). It manages process state transitions across `New`, `Ready`, `Running`, `Blocked`, and `Completed`. In `ReadyQueue`, I implemented a deterministic priority queue: processes are sorted primarily by effective priority, using arrival timestamp and PID as stable tie-breakers. My scheduler operates non-preemptively because once an aircraft touches down or initiates takeoff, execution cannot be abruptly halted mid-runway without catastrophe. 
  > 
  > At the DBMS layer, I engineered the `scheduling_queue` table in MySQL. Whenever a process transitions between states or computes its final Turnaround and Waiting times, parameterized SQL queries persist these metrics. Furthermore, I implemented `syncReadyQueueFromDatabase()`, which queries MySQL for `Ready` processes upon application startup, ensuring zero queue loss across crashes. I verified all algorithms across 4 automated unit test suites."

#### 7. Likely Individual Viva Questions & Answers
1. **Q: Why is non-preemptive scheduling used instead of round-robin or preemptive priority?**  
   *A:* In physical runway management, once an aircraft commits to a landing flare or takeoff roll, the runway resource cannot be preempted or time-sliced without causing a runway excursion or collision.
2. **Q: What is the relationship between `scheduling_queue` in MySQL and `ReadyQueue` in memory?**  
   *A:* `scheduling_queue` provides durable persistence across reboots, while `ReadyQueue` is a high-speed in-memory C++ priority queue allowing instantaneous dispatching without disk I/O bottlenecks.

---

### Member 2: Dynamic Priority, Aging & Fleet Relational Modeling

#### 1. Technical Domain Summary
- **Operating Systems:** Priority Aging algorithms for starvation prevention, Dynamic Emergency Priority escalation, and comparative starvation simulation analysis.
- **Database Systems:** Relational schema design for aviation fleet assets (`aircraft` and `flights`), 1:N referential integrity, domain constraints (`CHECK`), and dynamic flight lifecycle management.

#### 2. Key Implemented Features
- **Aging Engine (`SchedulerComponents::AgingEngine`):** Developed real-time priority aging to eliminate process starvation. Using a configurable timer interval ($T_{\text{interval}}$, default: 30s), effective priority systematically improves:
  $$\text{Effective Priority} = \max\left(1, \text{Base Priority} - \left\lfloor\frac{\Delta t_{\text{wait}}}{T_{\text{interval}}}\right\rfloor\right)$$
- **Dynamic Emergency Priority:** Modeled emergency overrides where declaring an emergency elevates effective priority to 1 (Highest) and jumps the flight to the head of the queue, while safely preserving the original base priority.
- **Starvation Demonstration Suite (`StarvationSimulation`):** Built the side-by-side verification engine comparing Mode A (Without Aging — proving starvation of low-priority tasks) versus Mode B (With Aging — proving starvation mitigation).
- **Fleet Relational Schema (`aircraft` & `flights`):** Designed normalized 3NF relational tables linking physical airframes to scheduled flight operations with foreign key `fk_flights_aircraft_id` (`ON DELETE SET NULL`).
- **Data Integrity Constraints:** Implemented database-level and UI-level constraints: flight priority range ($1 \le \text{priority} \le 5$), unique aircraft registrations, and operational statuses (`Scheduled`, `Taxiing`, `Cleared`, `Landed`, `Departed`).

#### 3. Primary Source Files
- `src/models/Flight.h` & `src/models/Aircraft.h`
- `src/scheduler/AgingEngine.h` & `src/scheduler/AgingEngine.cpp`
- `src/scheduler/StarvationSimulation.h` & `src/scheduler/StarvationSimulation.cpp`
- `ui/FlightsView.h` & `ui/FlightsView.cpp`
- `ui/AircraftView.h` & `ui/AircraftView.cpp`
- `ui/FlightDialog.h` & `ui/AircraftDialog.h`

#### 4. Primary Database Tables Handled
- **`aircraft`**: `aircraft_id` (PK), `registration_number` (UNIQUE), `aircraft_type`, `capacity`, `status`.
- **`flights`**: `flight_id` (PK), `flight_number`, `airline`, `aircraft_id` (FK), `flight_type`, `scheduled_time`, `priority`, `is_emergency`, `operational_status`.

#### 5. Automated CTest Suites Authored
1. `TestPriorityAging`: Mathematical verification of aging calculations over elapsed time steps.
2. `TestStarvationSimulation`: Execution of Mode A vs Mode B starvation benchmarks.
3. `TestDynamicEmergencyPriority`: Verification of priority override to 1 and queue reordering.
4. `TestFlightManagement`: CRUD operations, flight number validation, status updates.
5. `TestAircraftManagement`: Aircraft registration uniqueness and capacity validation.

#### 6. Spoken Viva Defense Speeches
- **60-Second Explanation:**
  > "I took ownership of Starvation Mitigation, Priority Aging, and Relational Fleet Modeling. On the OS side, I developed the `AgingEngine`, which monitors continuous ready waiting time and decrements effective priority every 30 seconds to prevent low-priority starvation. I also implemented Dynamic Emergency Priority, enabling distressed flights to immediately leapfrog normal traffic to Priority 1. On the DBMS side, I designed the normalized `aircraft` and `flights` tables with foreign keys and SQL domain constraints, and built the management UIs in Qt. I authored 5 automated CTest suites validating fleet integrity and aging dynamics."
- **2-Minute Explanation:**
  > "In SmartATC, my role tackled one of the most critical vulnerabilities in priority scheduling: process starvation, combined with core relational fleet engineering.
  > 
  > Under pure priority scheduling, a low-priority commercial flight (Priority 5) will suffer indefinite postponement if high-priority VIP or emergency flights continually arrive. To solve this, I designed and implemented `AgingEngine`. Every aging interval (defaulting to 30 seconds), the engine inspects the elapsed waiting time of all `Ready` processes and decrements their effective priority towards 1 according to $\max(1, \text{basePriority} - \lfloor\Delta t / T\rfloor)$. To scientifically prove this in our viva, I authored `StarvationSimulation`, which runs a side-by-side simulation demonstrating that Mode A starves low-priority processes while Mode B guarantees bounded waiting time.
  > 
  > On the database side, I architected the relational structure for `aircraft` and `flights` in MySQL. I established a 1:N relationship ensuring that deleting an aircraft safely sets the flight's aircraft reference to `NULL` without cascading cancellation. I implemented validation logic for flight identifiers, priority limits ($1-5$), and operational status lifecycles from `Scheduled` to `Landed` or `Departed`. My implementation is verified by 5 automated test suites."

#### 7. Likely Individual Viva Questions & Answers
1. **Q: How does your aging formula ensure base priority is not permanently lost?**  
   *A:* Base priority is stored as an immutable property in the process PCB and database, while `effective_priority` is a computed working field that fluctuates based on elapsed wait time or emergency status.
2. **Q: Why is foreign key `ON DELETE SET NULL` used between `flights` and `aircraft`?**  
   *A:* Commercial flight schedules exist independently of physical equipment; if an aircraft undergoes emergency maintenance and is removed from the fleet, the flight remains on the schedule to be reassigned.

---

### Member 3: Critical Sections, Mutual Exclusion & ACID Transactions

#### 1. Technical Domain Summary
- **Operating Systems:** Modeling airport runways as shared, non-shareable critical section resources; enforcing mutual exclusion via `std::mutex` and custom RAII `ScopedResourceLock`; demonstrating multithreaded contention with `std::thread`.
- **Database Systems:** Full ACID transaction management (`START TRANSACTION`, `COMMIT`, `ROLLBACK`), pessimistic row-level locking with InnoDB `SELECT ... FOR UPDATE`, and atomic failure rollback verification.

#### 2. Key Implemented Features
- **Runway Critical Section Protection:** Modeled physical runways (`09L`, `09R`, `27`) as shared system resources that can only be occupied by one flight at a time.
- **C++ RAII Mutual Exclusion (`ResourceManager::ScopedResourceLock`):** Built a custom RAII lock wrapper around `std::mutex` ensuring that locks are strictly acquired before entering the critical section and guaranteed to release upon scope exit, preventing resource leakage.
- **Multithreaded Concurrency Demo (`ConcurrentAllocationDemo`):** Spawned competing worker threads via `std::thread` to simulate simultaneous landing requests, proving that Thread 1 enters the critical section while Thread 2 is safely serialized/rejected without race conditions.
- **Multi-Statement ACID Transactions (`ApplicationService::allocateRunway` & `releaseRunwayResource`):** Orchestrated multi-table MySQL transactions updating `runways`, `runway_assignments`, `flights`, and `operation_logs` atomically.
- **Pessimistic Concurrency Control:** Implemented InnoDB row-level locking using `SELECT ... FOR UPDATE` to prevent write-skew and phantom allocations during concurrent ATC operations.
- **Failure-Injection Harness:** Implemented synthetic failure injection stages (`DuringUpdateAssignment`, `DuringUpdateRunway`, `DuringCommit`) proving that MySQL undo logs roll back partial updates if any statement fails.

#### 3. Primary Source Files
- `src/resource/ResourceManager.h` & `src/resource/ResourceManager.cpp`
- `src/resource/ConcurrentAllocationDemo.h` & `src/resource/ConcurrentAllocationDemo.cpp`
- `src/database/DatabaseManager.h` & `src/database/DatabaseManager.cpp`
- `src/models/Runway.h` & `src/models/RunwayAssignment.h`
- `ui/RunwaysView.h` & `ui/RunwaysView.cpp`
- `ui/RunwayAllocationDialog.h` & `ui/RunwayAllocationDialog.cpp`

#### 4. Primary Database Tables Handled
- **`runways`**: `runway_id` (PK), `runway_number`, `runway_type` (`Landing`, `Takeoff`, `Both`), `status` (`Available`, `Occupied`, `Maintenance`).
- **`runway_assignments`**: `assignment_id` (PK), `flight_id` (FK), `runway_id` (FK), `assigned_by` (FK), `assigned_at`, `released_at`, `status` (`Active`, `Released`).

#### 5. Automated CTest Suites Authored
1. `TestCriticalSectionAndMutualExclusion`: Verification of mutex locks and RAII scoped guards.
2. `TestConcurrentRunwayAllocation`: Multithreaded race condition prevention across threads.
3. `TestDatabaseTransactions`: Verification of multi-statement transactional atomicity.
4. `TestACIDProperties`: Proof of rollback under simulated failure injection.
5. `TestRunwayManagement`: CRUD validation and operational status transitions for runways.
6. `TestRunwayResourceAllocation`: Compatibility matching (e.g. Landing vs Takeoff).

#### 6. Spoken Viva Defense Speeches
- **60-Second Explanation:**
  > "I took ownership of Concurrency, Critical Section Protection, and MySQL Database Transactions in SmartATC. On the OS side, I modeled runways as shared non-shareable resources, protecting them using `std::mutex` and custom RAII `ScopedResourceLock` guards. I built `ConcurrentAllocationDemo` using C++ `std::thread` to prove mutual exclusion under simultaneous access. On the DBMS side, I implemented atomic MySQL transactions with InnoDB pessimistic row locks (`SELECT ... FOR UPDATE`) across four tables, ensuring that runway allocation and release commit cleanly or roll back completely upon failure. I authored 6 automated CTest suites."
- **2-Minute Explanation:**
  > "My domain in SmartATC focused on concurrency control, shared resource integrity, and database transaction atomicity. An airport runway is the ultimate critical section: if two flights are cleared onto the same runway simultaneously, catastrophic collision occurs.
  > 
  > In C++, I implemented dual-tier concurrency control. At the application layer, each runway is guarded by a dedicated `std::mutex`. I authored `ScopedResourceLock`, an RAII wrapper ensuring that mutex acquisition and release are deterministic, preventing deadlocks or unreleased locks even if exceptions arise. To demonstrate this live, I wrote `ConcurrentAllocationDemo`, which creates concurrent worker threads that attempt to allocate the same runway simultaneously; the second thread is serialized or rejected without corrupting in-memory state.
  > 
  > At the DBMS layer, I engineered the transactional core of `ApplicationService`. Runway allocation requires 4 distinct SQL operations: checking compatibility, creating a `runway_assignments` record, marking the runway `Occupied`, and advancing flight status. I bound these in an atomic transaction (`START TRANSACTION` ... `COMMIT`). Using `SELECT ... FOR UPDATE`, I enforced pessimistic row locking at the InnoDB engine level. I also built a failure-injection testing framework that deliberately triggers mid-transaction failures, proving that MySQL's undo logs roll back partial writes to guarantee full ACID compliance. I verified this across 6 automated test suites."

#### 7. Likely Individual Viva Questions & Answers
1. **Q: Why do you need both C++ `std::mutex` AND MySQL `SELECT ... FOR UPDATE`?**  
   *A:* C++ `std::mutex` prevents data races between background worker threads inside the local application process, while MySQL `SELECT ... FOR UPDATE` serializes concurrent transactions across multiple database client sessions or distributed controller terminals.
2. **Q: How does your system prove Atomicity during a runway allocation crash?**  
   *A:* If a statement fails (or synthetic failure is injected), `db.rollback()` executes, reversing the `runway_assignments` insertion and restoring runway status to `Available` as verified in `TestACIDProperties`.

---

### Member 4: Security/RBAC, System Orchestration & Analytical Monitoring

#### 1. Technical Domain Summary
- **Operating Systems:** Principle of Least Privilege / Security Protection Rings, System Subsystem Orchestration (coordinating the Scheduler, Resource Manager, and UI message bus), Resource Contention and Deadlock Resolution (`scheduleAndAllocateNext`).
- **Database Systems:** Role-Based Access Control schema (`roles`, `users`), SHA-256 cryptographic credential verification, immutable transactional audit logging (`operation_logs`), and real-time analytical SQL aggregations for executive KPI monitoring.

#### 2. Key Implemented Features
- **Least-Privilege Security Model:** Implemented operating system protection boundaries across three distinct operational roles: `Admin` (System Config), `ATC` (Air Traffic Controller — Scheduling & Allocations), and `Airport Operator` (Ground Maintenance).
- **Cryptographic Credential Verification (`PasswordUtils`):** Implemented SHA-256 hashing to guarantee that plaintext user passwords are never stored in memory or persisted on disk.
- **Session Architecture (`Models::Session`):** Created a permission-key matrix (`action:RunwayAllocation`, `action:ManageScheduling`, `action:DeclareEmergency`) that gates service execution at the API boundary.
- **Automated Dispatch-to-Runway Orchestration (`scheduleAndAllocateNext`):** Designed the end-to-end bridge that selects the highest-priority PCB from the OS Scheduler, verifies runway availability, executes the database transaction, and binds the flight to the physical runway in one unified operation.
- **Transactional Audit Trail (`operation_logs`):** Developed automatic logging for every operational transaction, tracking user ID, entity ID, action type, timestamp, and transaction status (`Committed`, `Rolled Back`).
- **Executive KPI Dashboard (`DashboardView`):** Implemented real-time aggregate SQL queries (`COUNT`, `SUM(CASE ...)`, `AVG`) computing runway utilization percentages, on-time punctuality rates, active queue lengths, and fleet operational states.

#### 3. Primary Source Files
- `src/models/User.h`
- `src/models/Session.h` & `src/models/Session.cpp`
- `src/utils/PasswordUtils.h` & `src/utils/PasswordUtils.cpp`
- `src/services/ApplicationService.cpp` (RBAC enforcement, `scheduleAndAllocateNext`, aggregate analytics)
- `ui/LoginView.h` & `ui/LoginView.cpp`
- `ui/DashboardView.h` & `ui/DashboardView.cpp`

#### 4. Primary Database Tables Handled
- **`roles`**: `role_id` (PK), `role_name` (`Admin`, `ATC`, `Airport Operator`).
- **`users`**: `user_id` (PK), `username` (UNIQUE), `password_hash`, `role_id` (FK), `status`.
- **`operation_logs`**: `log_id` (PK), `user_id` (FK), `operation_type`, `transaction_status` (`Committed`, `Rolled Back`), `message`, `timestamp`.
- **Analytical Views/Queries**: Aggregate queries across `flights`, `runways`, and `scheduling_queue`.

#### 5. Automated CTest Suites Authored
1. `TestAuthAndRBAC`: Password hashing, unauthorized action rejection, session timeouts.
2. `TestDashboardMonitoring`: Correctness of aggregate KPI metrics, query execution speed.
3. `TestCompleteIntegration`: End-to-end dispatch-to-allocation operational workflow.
4. `TestDatabaseConnectivity`: Connection pool lifecycle, auto-reconnect, schema checks.

#### 6. Spoken Viva Defense Speeches
- **60-Second Explanation:**
  > "I was responsible for System Security, End-to-End Orchestration, and Analytical Monitoring in SmartATC. On the OS side, I designed the least-privilege security model and engineered `scheduleAndAllocateNext()`, which unifies the OS process dispatcher with physical runway resource allocation. On the DBMS side, I designed the `roles`, `users`, and `operation_logs` tables, implemented SHA-256 password hashing, and built the real-time Dashboard displaying airport KPIs via optimized SQL aggregation queries. I authored 4 automated CTest suites verifying security, connectivity, and full-system integration."
- **2-Minute Explanation:**
  > "In SmartATC, my role served as the integrator and guardian of system security, bridging operating system access control with analytical database engineering.
  > 
  > Following the OS principle of least privilege, I engineered `Session` and `User` models that restrict system actions across Admin, Air Traffic Controller, and Airport Operator roles. For instance, an operator inspecting runway lighting is strictly blocked from clearing landings or dispatching flights. In `PasswordUtils`, I implemented SHA-256 cryptographic hashing to ensure credentials are never exposed in plaintext.
  > 
  > A major contribution was authoring the unified orchestration workflow: `ApplicationService::scheduleAndAllocateNext()`. This function serves as the central bridge: it consults the OS `Scheduler` to pop the highest-priority flight, verifies runway compatibility in `ResourceManager`, and triggers the atomic MySQL transaction to occupy the runway. To maintain absolute transparency, I built the `operation_logs` table, which immutably records every dispatch, allocation, or failure with user stamps and transaction outcomes (`Committed` or `Rolled Back`). Lastly, I built `DashboardView`, writing high-performance SQL aggregate queries to compute real-time runway utilization and punctuality metrics. My work is validated by 4 automated test suites."

#### 7. Likely Individual Viva Questions & Answers
1. **Q: How does `scheduleAndAllocateNext()` combine OS concepts with DBMS concepts?**  
   *A:* It uses OS priority scheduling to select the optimal candidate from the in-memory Ready Queue, then immediately invokes a DBMS transaction to acquire the runway row lock and commit the assignment to disk.
2. **Q: Why are aggregate metrics calculated via SQL queries rather than in C++ loops?**  
   *A:* Executing `SUM(CASE ...)` and `COUNT(*)` inside the MySQL engine leverages database indexing and query caching, minimizing memory bandwidth and keeping the desktop GUI responsive.

---

## 4. Quick-Reference Viva Cheatsheet (For the Team)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       SMARTATC TEAM RESPONSIBILITY SUMMARY                  │
├──────────┬─────────────────────────────┬────────────────────────────────────┤
│ MEMBER   │ OPERATING SYSTEMS FOCUS     │ DATABASE MANAGEMENT SYSTEMS FOCUS  │
├──────────┼─────────────────────────────┼────────────────────────────────────┤
│ Member 1 │ • Process Control Block     │ • scheduling_queue Schema          │
│          │ • ReadyQueue Priority Sort  │ • Process State Persistence        │
│          │ • Non-Preemptive CPU Sched  │ • Queue Boot Sync from Database    │
│          │ • TAT, WT, CT Metrics       │ • Arrival / Completion Timestamps  │
├──────────┼─────────────────────────────┼────────────────────────────────────┤
│ Member 2 │ • Priority Aging Engine     │ • Relational Fleet (aircraft/flight│
│          │ • Starvation Prevention     │ • 1:N Foreign Keys (ON DELETE NULL)│
│          │ • Emergency Preemption      │ • Domain Constraints (CHECK 1..5)  │
│          │ • Starvation Simulation     │ • Operational Status Lifecycle     │
├──────────┼─────────────────────────────┼────────────────────────────────────┤
│ Member 3 │ • Runway Critical Section   │ • Multi-Statement ACID Transactions│
│          │ • std::mutex & Scoped Lock  │ • InnoDB Row Locks (FOR UPDATE)    │
│          │ • Concurrency (std::thread) │ • Atomic Rollback on Failure       │
│          │ • Race Condition Prevention │ • runways / runway_assignments DDL │
├──────────┼─────────────────────────────┼────────────────────────────────────┤
│ Member 4 │ • Least-Privilege Protection│ • RBAC Schema (roles / users)      │
│          │ • Dispatch-Runway Binding   │ • SHA-256 Password Cryptography    │
│          │ • scheduleAndAllocateNext   │ • Transactional Audit Trail Log    │
│          │ • System State Coordination │ • Real-Time KPI SQL Aggregations   │
└──────────┴─────────────────────────────┴────────────────────────────────────┘
```
