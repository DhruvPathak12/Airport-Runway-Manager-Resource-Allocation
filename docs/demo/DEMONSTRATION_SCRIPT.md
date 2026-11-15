# SmartATC — Final B.Tech Demonstration & Viva Script

## Presentation Overview
- **Project Title:** Smart Air Traffic Control Management System (SmartATC)
- **Target Context:** Chhatrapati Shivaji Maharaj International Airport (BOM / VABB), Mumbai
- **Target Audience:** B.Tech Project Evaluation Panel / External Examiner
- **Total Duration:** 18–20 Minutes (8 Structured Phases)
- **Technology Stack:** C++17, Qt 6 Widgets, MySQL 8 (InnoDB), CMake

---

## Timeline & Phase Breakdown

```
[00:00 - 02:00] Phase 1: Architecture & System Overview (2 min)
[02:00 - 04:00] Phase 2: Authentication & Role-Based Access Control (2 min)
[04:00 - 06:00] Phase 3: Flight & Aircraft Resource Management (2 min)
[06:00 - 10:00] Phase 4: OS Non-Preemptive Priority Scheduling & Emergency Priority Escalation (4 min)
[10:00 - 12:00] Phase 5: Dynamic Priority Aging & Starvation Demonstration (2 min)
[12:00 - 15:00] Phase 6: Runway Contention, Critical Sections & Multithreading (3 min)
[15:00 - 18:00] Phase 7: MySQL Transactions & ACID Properties (3 min)
[18:00 - 20:00] Phase 8: Real-Time Dashboard, Metrics & Viva Wrap-up (2 min)
```

---

## Phase 1: Architecture & Project Introduction (00:00 – 02:00)

### Action Steps
1. Launch `SmartATC.exe` or show the landing screen.
2. Present the high-level architecture slide or open `PROJECT_SPEC.md` / `ARCHITECTURE_OVERVIEW.md`.

### What to Say to the Examiner
> "Good morning, respected examiners. Today we present the **Smart Air Traffic Control Management System (SmartATC)**. 
> 
> Real-world air traffic control is one of the most critical real-time distributed environments in the world. At a busy hub like Mumbai Airport (BOM), multiple aircraft compete for scarce physical resources—namely, runways and airspace slots.
> 
> In this project, we bridge theoretical computer science with aviation engineering by modeling airport operations as classic **Operating System** and **Database Management System** paradigms:
> - Aircraft flight operations are modeled as **Processes**, encapsulated within **Process Control Blocks (PCBs)** with explicit lifecycle states (`New`, `Ready`, `Running`, `Blocked`, `Completed`).
> - Runways are modeled as **Shared Hardware Resources** requiring **Mutual Exclusion**.
> - Air traffic prioritization is executed via a **Non-Preemptive OS Priority Scheduler** with dynamic aging to prevent starvation.
> - Runway allocations and state changes are executed as **Pessimistic ACID Transactions** in **MySQL 8 (InnoDB)**, ensuring zero race conditions or partial updates.
>
> We will now walk through the live application to demonstrate each of these OS and DBMS concepts in action."

### Concepts Proven
- Systems architecture, separation of concerns (MVC/Service-Repository layer), desktop simulation principles.

### Contingency
- If projector is not connected, use laptop screen directly.

---

## Phase 2: Authentication & Role-Based Access Control (02:00 – 04:00)

### Action Steps
1. Show the **Login Dialog** (`LoginDialog`).
2. Attempt invalid login: Username `admin_demo`, Password `WrongPassword` $\rightarrow$ Observe error message.
3. Log in as **Airport Operator** (`operator_demo`).
4. Show the Navigation Bar: Note that **Declare Emergency** and **Runway Allocation** are disabled or hidden, but **Runway Maintenance** is enabled.
5. Log out.
6. Log in as **Air Traffic Controller** (`atc_demo`).
7. Observe that flight management, emergency declaration, and runway allocation are fully unlocked.

### What to Say to the Examiner
> "We begin with security and Role-Based Access Control (RBAC). 
> 
> SmartATC enforces strict principle-of-least-privilege at the presentation and service layers:
> - Passwords are securely hashed using salted SHA-256 before verification against the MySQL `users` table.
> - We provide three core roles: **Admin**, **Air Traffic Controller (ATC)**, and **Airport Operator**.
> - When logged in as `operator_demo`, the user can toggle runway maintenance and view aircraft status, but cannot allocate runways or declare emergencies.
> - When logged in as `atc_demo`, the controller receives operational authorization to schedule flights, handle emergencies, and allocate runway resources.
>
> We are now logged in as `atc_demo` for operational command."

### Concepts Proven
- RBAC, cryptographic security, authorization boundaries.

### Contingency
- If user lockout occurs, use `admin_demo` to reset user state.

---

## Phase 3: Flight & Aircraft Management (04:00 – 06:00)

### Action Steps
1. Navigate to **Flight Management View** (`FlightManagementView`).
2. Point out the live table populated from MySQL containing the BOM demonstration fleet:
   - `AI-101` (A320neo, Base Priority 2, Scheduled)
   - `6E-202` (A321neo, Base Priority 3, Scheduled)
   - `AI-303` (B777-300ER, Base Priority 3, Scheduled)
   - `SG-404` (B737-800, Base Priority 4, Scheduled)
   - `UK-505` (A320neo, Base Priority 5, Scheduled)
3. Open the **Aircraft / Runway View** to demonstrate physical aircraft bindings:
   - Aircraft `VT-AT1` through `VT-AT4` with wake turbulence categories (Heavy, Medium).

### What to Say to the Examiner
> "Here in the Flight Management module, each record in MySQL represents an incoming or outgoing flight. 
> 
> In Operating System terminology, every flight corresponds to a **Process**. Notice the priority column: in our system, **Priority 1 is the highest priority** and **Priority 5 is lowest**. 
> 
> We have pre-configured a realistic arrival queue: Air India flight 101 has base priority 2 (international landing), IndiGo 6E-202 and Air India 303 have priority 3 (standard domestic), SpiceJet 404 has priority 4, and Vistara 505 has priority 5.
> 
> Now let us see how the operating system scheduler manages these processes in memory."

### Concepts Proven
- Process creation, static attributes, process metadata mapping.

---

## Phase 4: OS Priority Scheduling & Emergency Priority Escalation (06:00 – 10:00)

### Action Steps
1. Navigate to **Scheduling View** (`SchedulingView`) $\rightarrow$ Click **Ready Queue Tab**.
2. Click **Build Ready Queue** or observe the active queue table:
   - Show ordering: `AI-101` (Pri 2) $\rightarrow$ `6E-202` (Pri 3, arrived 10:05) $\rightarrow$ `AI-303` (Pri 3, arrived 10:08) $\rightarrow$ `SG-404` (Pri 4) $\rightarrow$ `UK-505` (Pri 5).
3. Explain the tie-breaker rule between `6E-202` and `AI-303`: both have Priority 3, but `6E-202` arrived earlier, so it takes precedence.
4. Click **Step Next Process**:
   - `AI-101` transitions from `Ready` to `Running`.
   - PCB state updates visually to `Running`.
5. **The Emergency Scenario:**
   - In the Emergency Declaration panel, select Flight **AI-303** (which was next in line with Priority 3).
   - Enter reason: `"Engine Oil Pressure Loss - MAYDAY"`.
   - Click **Declare Emergency**.
   - Observe the Ready Queue update instantaneously:
     - `AI-303`'s effective priority overrides dynamically from **3 to 1**.
     - `AI-303` jumps immediately to the head of the `Ready` queue ahead of all normal flights.
     - **Crucial Non-Preemptive Check:** Point out that **`AI-101` is NOT preempted or interrupted in mid-runway**. Because SmartATC enforces non-preemptive priority scheduling, the currently running process finishes its operation safely, and the emergency flight is selected at the **very next scheduling opportunity**.

### What to Say to the Examiner
> "This is a cornerstone of our Operating System implementation: **Non-Preemptive Priority Scheduling**.
> 
> In `ReadyQueue.cpp`, runnable processes in the `Ready` state are sorted strictly by `effectivePriority ASC`, with tie-breakers on `arrivalTime ASC` and `flightId ASC`. Lower numerical value indicates higher scheduling precedence.
> 
> When we dispatched the queue, AI-101 entered the `Running` state.
> 
> Now observe what happened when an in-flight emergency occurred on AI-303:
> 1. The controller declared an emergency. The system dynamically elevated AI-303's effective priority to 1, while strictly preserving its base priority 3.
> 2. AI-303 immediately jumped to index 0 of the Ready Queue.
> 3. Notice that AI-101 was NOT aborted or preempted. In aviation safety, as in real-time operating systems, non-preemptive scheduling guarantees that active hardware operations terminate safely before context-switching to the emergency process."

### Concepts Proven
- PCB state transitions (`New` $\to$ `Ready` $\to$ `Running` $\to$ `Completed`), Ready Queue sorting, dynamic priority escalation without preemption.

### Contingency
- If queue does not populate, click **Refresh** or restart the app.

---

## Phase 5: Dynamic Priority Aging & Starvation Prevention (10:00 – 12:00)

### Action Steps
1. Navigate to the **Starvation Demo Tab** in `SchedulingView`.
2. Point out low-priority flight **UK-505** (Base Priority 5).
3. **Without Aging Demonstration:**
   - Click **Simulate Inflow Flights** (high priority flights continuously arrive).
   - Observe that UK-505 remains stuck at the back of the queue indefinitely.
   - Point to the metrics label: `Wait Time: 450s | Status: Starving`.
4. **With Aging Demonstration:**
   - Enable the **Enable Priority Aging** checkbox (configured default interval: 30 seconds).
   - Explain the aging formula:
     $$\text{agingLevels} = \left\lfloor\frac{\text{readySince.secsTo}(now)}{\text{configuredIntervalSeconds}}\right\rfloor$$
     $$\text{effectivePriority} = \max(1, \ \text{basePriority} - \text{agingLevels})$$
   - Observe UK-505's effective priority progressively improve over elapsed intervals: $5 \to 4 \to 3 \to 2 \to 1$.
   - UK-505 advances to the front of the queue and is dispatched.

### What to Say to the Examiner
> "In static priority scheduling, low-priority processes suffer from **Starvation** or indefinite blocking if higher-priority processes continuously arrive.
> 
> On this Starvation Demo panel, we prove both the vulnerability and the OS solution:
> - Without aging, Vistara 505 (Priority 5) was starved by incoming traffic.
> - By activating our `AgingEngine`, the scheduler periodically evaluates elapsed waiting time in the `Ready` state.
> - Based on the configured aging interval—defaulting to 30 seconds—the system accrues aging levels and reduces the numerical priority value.
> - Notice that UK-505 gradually improves from 5 to 1, guaranteeing bounded waiting time and eliminating starvation."

### Concepts Proven
- Process starvation, priority aging, bounded waiting time, idempotent timer evaluation based on `readySince`.

---

## Phase 6: Runway Contention, Critical Sections & Multithreading (12:00 – 15:00)

### Action Steps
1. Navigate to **Runway Management View** (`RunwayManagementView`).
2. Point out **Runway 09L** (Available) and **Runway 27** (Maintenance).
3. Open the **Concurrent Allocation Demo** dialog (`ConcurrentAllocationDemo`).
4. Select shared resource: **Runway 09L**.
5. Select two competing threads:
   - **Thread 1:** Flight `AI-101` requesting 09L.
   - **Thread 2:** Flight `6E-202` simultaneously requesting 09L.
6. Click **Launch Concurrent Threads**:
   - Both worker threads spawn concurrently via `std::thread`.
   - The UI execution log displays timestamped thread synchronization outputs:
     ```
     [Thread 1] Attempting to acquire ScopedResourceLock for Runway 09L...
     [Thread 1] Mutex acquired! Entering Critical Section.
     [Thread 2] Attempting to acquire ScopedResourceLock for Runway 09L...
     [Thread 2] Runway 09L locked by Thread 1. Thread 2 blocked (Contention).
     [Thread 1] Allocation committed. Releasing ScopedResourceLock.
     [Thread 2] Mutex acquired. Runway status is now OCCUPIED. Allocation rejected safely!
     ```
7. Show that Runway 09L is assigned to exactly **one** flight, with zero double-booking.

### What to Say to the Examiner
> "Here we demonstrate **Critical Sections, Mutual Exclusion, and Multithreading**.
> 
> A runway is a shared physical resource. If two concurrent threads attempt to allocate Runway 09L simultaneously without synchronization, a race condition would corrupt system state.
> 
> In `ResourceManager.cpp`, each runway is guarded by a dedicated `std::mutex`. When a thread requests a runway, it must acquire an RAII-style `ScopedResourceLock`.
> 
> As you observed in this live concurrent test:
> - Thread 1 acquired the lock and entered the Critical Section.
> - Thread 2 attempted acquisition simultaneously, was blocked, and waited safely.
> - Once Thread 1 committed and exited the critical section, Thread 2 inspected the updated state, identified the runway as Occupied, and cleanly aborted.
> - We have mathematically guaranteed **Mutual Exclusion** with zero deadlocks and zero race conditions."

### Concepts Proven
- Critical section, mutual exclusion, race condition prevention, `std::mutex`, RAII scoped locking (`std::lock_guard`), thread contention.

---

## Phase 7: MySQL Transactions & ACID Verification (15:00 – 18:00)

### Action Steps
1. Show the **Transaction Activity Log** embedded in the Runway Management View.
2. **Demonstrate Successful COMMIT:**
   - Allocate Runway **09R** to Flight **SG-404**.
   - Show committed transaction log:
     - `runway_assignments` record inserted (`status = 'Active'`).
     - `runways.status` updated to `'Occupied'`.
     - `flights.operational_status` updated to `'Cleared'`.
     - `operation_logs` written with `transaction_status = 'Committed'`.
3. **Demonstrate Deterministic ROLLBACK:**
   - Explain the distinction between pre-mutation validation rejection vs. genuine transaction rollback.
   - Use the failure-injection hook (`AllocationFailureStage::DuringUpdateFlight`) in test/demo mode to show midway failure.
   - Execute the allocation: statements 1 and 2 execute in MySQL, statement 3 fails artificially, and the catch block issues `db.rollback()`.
   - Run verification query:
     ```sql
     SELECT runway_number, status FROM runways WHERE runway_id = 2;
     SELECT * FROM runway_assignments WHERE runway_id = 2 AND status = 'Active';
     ```
   - Show that all partial writes were rolled back cleanly by InnoDB undo logs.

### What to Say to the Examiner
> "Now we transition from Operating Systems to the **Database Management System (DBMS)** layer.
> 
> Allocating a runway is not a single SQL statement—it is an atomic state transition spanning four database tables: `runways`, `runway_assignments`, `flights`, and `operation_logs`.
> 
> In `ApplicationService.cpp`, all four operations are enclosed within a single atomic MySQL transaction:
> 1. We issue `START TRANSACTION` and apply pessimistic row locking using `SELECT ... FOR UPDATE` under MySQL's `REPEATABLE READ` isolation level.
> 2. We execute all inserts and updates.
> 3. If any failure occurs midway, the engine issues `ROLLBACK`, restoring all rows via InnoDB undo logs.
> 
> As demonstrated, we have verified all four **ACID properties**:
> - **Atomicity:** All four tables update together, or all roll back.
> - **Consistency:** Foreign keys and valid enum statuses are strictly enforced.
> - **Isolation:** `SELECT ... FOR UPDATE` row locks prevent dirty reads or conflicting concurrent writes.
> - **Durability:** Committed transactions persist on disk across application shutdowns and restarts."

### Concepts Proven
- ACID properties, transaction boundaries (`START TRANSACTION`, `COMMIT`, `ROLLBACK`), pessimistic row locking (`FOR UPDATE`), database consistency.

---

## Phase 8: Dashboard, Operations Monitoring & Conclusion (18:00 – 20:00)

### Action Steps
1. Navigate to **System Dashboard** (`DashboardView`).
2. Highlight the real-time operational summary cards:
   - **Total Flights:** 5 | **Queued / Scheduled:** Active counts
   - **Emergency Alert Banner:** Displays `AI-303 (Engine Oil Pressure Loss)` in bold red.
   - **Runway Utilization Gauges:** 09L (Occupied), 09R (Occupied), 27 (Maintenance) $\rightarrow$ dynamic utilization percentage.
   - **Scheduling Performance Panel:** Average Wait Time, Completed Processes, State counts.
   - **Recent Transaction & Audit Stream:** Shows full trace of committed operations.
3. Conclude the demonstration.

### What to Say to the Examiner
> "To conclude our demonstration, this is the **SmartATC System Dashboard**.
> 
> Every metric displayed on this screen is computed dynamically from real application and database state. There are no hardcoded values.
> 
> To summarize our project achievements:
> 1. We modeled airport flight operations using **OS Process Management** and **Non-Preemptive Priority Scheduling**.
> 2. We solved process starvation through **Dynamic Priority Aging**.
> 3. We protected physical airport resources using **C++ Multithreading**, **Critical Sections**, and **Mutual Exclusion**.
> 4. We guaranteed airport data integrity through **Atomic MySQL Transactions** satisfying all **ACID properties**.
> 5. We integrated all modules into a responsive, production-quality **Qt 6 desktop application**.
> 
> Thank you, respected examiners. We are now ready for your questions."

---

## Viva Q&A Cheat Sheet (Top 5 Anticipated Questions)

| # | Anticipated Examiner Question | Immediate Concise Answer |
|---|---|---|
| **1** | *Why did you choose non-preemptive scheduling over preemptive scheduling?* | "In aviation, an aircraft in the final stages of landing or takeoff cannot be safely aborted mid-runway without severe hazard. Non-preemptive scheduling guarantees that the active process completes its critical physical maneuver before context-switching to an emergency process at the next scheduling opportunity." |
| **2** | *What are the actual process lifecycle states in SmartATC?* | "Our C++ `ProcessState` enum defines five states: `New`, `Ready`, `Running`, `Blocked`, and `Completed`. In the MySQL `scheduling_queue` table, these map to the `process_state` ENUM (`Ready`, `Running`, `Completed`, `Blocked`), with `New` mapping to `Ready` upon queue entry." |
| **3** | *How does priority aging calculate elapsed time?* | "`AgingEngine` computes continuous waiting time from `readySince.secsTo(now)`. The number of aging levels is `floor(elapsedSeconds / configuredIntervalSeconds)` (defaulting to 30s). The effective priority formula is `max(1, basePriority - agingLevels)`. This design is strictly idempotent and prevents drift." |
| **4** | *Why did you use `SELECT ... FOR UPDATE` instead of standard `SELECT`?* | "Under MySQL InnoDB's default `REPEATABLE READ` isolation level, standard `SELECT` reads from a point-in-time snapshot. Two concurrent transactions could both read `status = 'Available'`. Using `SELECT ... FOR UPDATE` acquires an exclusive X-lock on the specific rows, forcing competing transactions to block until the active transaction commits or rolls back." |
| **5** | *How is durability verified in SmartATC?* | "We verified durability by committing runway allocations to MySQL InnoDB, terminating the application, and restarting it to confirm all committed assignments and statuses persisted. Because MySQL commits flush write-ahead redo logs to non-volatile disk, committed state survives application restarts." |
