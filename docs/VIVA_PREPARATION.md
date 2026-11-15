# Comprehensive Viva & Technical Evaluation Guide

This document prepares project members for oral examinations (viva voce), project presentations, technical reviews, and academic evaluations for the **Smart Air Traffic Control Management System (SmartATC)**.

---

## Section 1: Project Overview & Core Philosophy

### Q1: What is the core objective of the Smart Air Traffic Control Management System?
**Answer:**  
SmartATC is an educational desktop simulation designed to demonstrate fundamental concepts of **Operating Systems (OS)** and **Database Management Systems (DBMS)** through the real-world domain analogy of airport runway operations.  
- **OS Concepts Demonstrated:** Process Control Blocks (PCB), Process State Transitions, Non-Preemptive Priority Scheduling, Starvation & Priority Aging, Critical Sections, Mutual Exclusion, and Multi-threaded Synchronization.
- **DBMS Concepts Demonstrated:** Relational Modeling, 3NF Normalization, ACID Transactions, Atomic Multi-Statement Operations, Pessimistic Row Locking (`SELECT ... FOR UPDATE`), and Audit Trails.

### Q2: Why did you choose C++17, Qt 6, and MySQL 8 for this project?
**Answer:**  
1. **C++17:** Provides native, zero-cost abstractions for systems programming, including modern concurrency (`std::thread`, `std::mutex`, `std::unique_lock`), deterministic memory management via RAII, and strong type safety required for simulation fidelity.
2. **Qt 6 Widgets:** Offers a mature, native cross-platform GUI architecture with Model-View separation, signal-and-slot event decoupling, and robust database abstraction (`QSqlDatabase`, `QSqlQuery`).
3. **MySQL 8 (InnoDB):** Enterprise-grade relational engine supporting true ACID properties, row-level pessimistic locking (`FOR UPDATE`), foreign key constraints, and consistent crash recovery.

### Q3: What is the analogy between airport operations and computing systems?
**Answer:**  
- **Flight $\rightarrow$ Simulated Process:** An active flight operation represents a schedulable job with an identifier (`flightId` as PID).
- **Flight Waiting Queue $\rightarrow$ Ready Queue:** Schedulable flights waiting for an available runway correspond to processes in an OS Ready Queue.
- **Flight Priority $\rightarrow$ Process Priority:** Integer priority scale where 1 is highest priority and 5 is lowest priority.
- **Emergency Flight $\rightarrow$ High-Priority Process:** Dynamic escalation to priority 1 override.
- **Runway $\rightarrow$ Shared OS Resource:** A runway is an exclusive shared resource (not a CPU core) protected by mutual exclusion.
- **Runway Assignment $\rightarrow$ Resource Allocation:** Assigning an available runway to an eligible flight.
- **Long Waiting $\rightarrow$ Starvation Risk:** Indefinite postponement of low-priority tasks under continuous high-priority traffic.
- **Priority Aging $\rightarrow$ Starvation Prevention:** Dynamically improving effective priority based on waiting duration.

---

## Section 2: Operating System & Scheduling Concepts

### Q4: How is a Process and Process Control Block (PCB) implemented in SmartATC?
**Answer:**  
A Process is modeled by `Models::Process` in `src/models/Process.h`. It acts as the PCB, maintaining:
- `flightId`: Unique integer process identifier (simulated PID).
- `queueId`: Foreign reference linking to `scheduling_queue.queue_id`.
- `state`: Enumerated state (`New`, `Ready`, `Running`, `Blocked`, `Completed`).
- `basePriority`: Static priority on a 1 (Highest) to 5 (Lowest) scale.
- `effectivePriority`: Dynamic priority adjusted by aging or emergency (1 to 5).
- `arrivalTime`, `readySince`, `serviceTime` (service duration in seconds), `waitingTime`, `turnaroundTime`, and `completionTime`.
- `agingLevel`: Counter tracking accumulated aging intervals.
- `isEmergency`: Boolean flag denoting active distress.

### Q5: Describe the process lifecycle state transitions.
**Answer:**  
1. **Creation $\rightarrow$ READY:** When a flight enters the scheduling system, its process enters the ready queue with state `READY`.
2. **READY $\rightarrow$ RUNNING:** The non-preemptive scheduler dispatches the process when a compatible runway is available and allocated.
3. **RUNNING $\rightarrow$ BLOCKED:** If a runway is taken offline for maintenance or operational delay, the process transitions to `BLOCKED`.
4. **BLOCKED $\rightarrow$ READY:** Once the runway or resource clears, the process is re-enqueued into `READY`.
5. **RUNNING $\rightarrow$ COMPLETED:** Once the aircraft finishes its operational service duration, the runway is released, state is updated to `COMPLETED`, and metrics are logged.

### Q6: Why did you choose a Non-Preemptive Priority Scheduler instead of Preemptive?
**Answer:**  
In physical aviation, once an aircraft enters final touchdown rollout or begins a high-speed takeoff roll, aborting mid-action (preempting) introduces catastrophic hazards. Therefore, non-preemptive scheduling accurately models runway physics: once allocated, a flight holds the runway resource non-preemptively until completion of its service duration.

### Q7: How does your scheduler break ties when two processes share the same priority?
**Answer:**  
SmartATC implements a strict 3-tier deterministic sorting hierarchy in `Scheduler::selectNextProcess()`:
1. **Effective Priority (Ascending):** Lowest numerical value wins (1 is highest priority, 5 is lowest).
2. **Arrival Time (Ascending - FCFS):** If priorities are equal, the earlier `arrivalTime` wins.
3. **Flight ID (Ascending):** If priorities and arrival times are identical, the smaller `flightId` breaks the tie deterministically.

### Q8: What are Turnaround Time, Waiting Time, and Response Time? State their formulas.
**Answer:**  
- **Completion Time ($CT$):** The timestamp when the process finishes its service duration.
- **Turnaround Time ($TAT$):** Total elapsed time from process arrival to completion:
  $$TAT = CT - AT$$
- **Waiting Time ($WT$):** Total time spent waiting in the ready queue prior to execution:
  $$WT = TAT - BT$$
  *(where $BT$ is the service duration / burst time).*
- **Response Time ($RT$):** Time from arrival until first transition to `RUNNING`. (In non-preemptive scheduling without interruption, $RT = WT$).

### Q9: How does Dynamic Emergency Priority work?
**Answer:**  
When an aircraft declares an in-flight emergency (`is_emergency = true`), `ApplicationService::declareFlightEmergency()`:
1. Sets `is_emergency = true` in MySQL and in-memory models.
2. Escalates `effectivePriority = 1` (Emergency highest priority).
3. Re-evaluates ready-queue precedence.  
This immediately places the distressed flight at the head of the queue, ensuring it is selected for the very next available runway without preempting the currently running aircraft. When the emergency is resolved, `effectivePriority` is restored to $\max(1, \text{basePriority} - \text{agingLevel})$, strictly preserving the original `basePriority`.

### Q10: What is Starvation, and how does your Priority Aging algorithm prevent it?
**Answer:**  
- **Starvation:** In pure priority scheduling, a continuous stream of higher-priority processes (e.g., Priority 1, 2, or 3) can cause lower-priority processes (e.g., Priority 4 or 5) to wait indefinitely.
- **Priority Aging:** Aging dynamically improves the effective priority of waiting processes based on continuous waiting time in `Ready` state:
  $$\text{agingLevel} = \min\left(255, \left\lfloor\frac{t_{\text{now}} - \text{readySince}}{T_{\text{aging}}}\right\rfloor\right)$$
  $$\text{effectivePriority} = \max(1, \text{basePriority} - \text{agingLevel})$$
  As waiting time accumulates, the numerical priority decrements towards **1** (highest priority), guaranteeing bounded waiting time and eventual dispatch.

---

## Section 3: Concurrency, Threads & Mutual Exclusion

### Q11: What is a Critical Section, and what shared resource does it protect?
**Answer:**  
A Critical Section is a segment of code that accesses shared, mutable resources that must not be concurrently accessed by more than one execution thread.  
In SmartATC, the **Runway** is the shared resource. If two flight worker threads attempt to allocate the same runway simultaneously, a race condition occurs. The critical section encapsulates checking runway availability, validating operational compatibility, and transitioning status to `Occupied`.

### Q12: How is Mutual Exclusion implemented in SmartATC?
**Answer:**  
Mutual exclusion is implemented at two distinct architectural levels:
1. **In-Memory (C++ Level):** Handled via `std::mutex m_resourceMutex` inside `ResourceManager`. Thread acquisition utilizes the RAII construct `std::unique_lock<std::mutex>` wrapped in `ResourceManager::ScopedResourceLock`.
2. **Database (DBMS Level):** Handled via MySQL InnoDB pessimistic row-level locking (`SELECT status FROM runways WHERE runway_id = ? FOR UPDATE`).

### Q13: What is RAII, and why is it critical for lock management?
**Answer:**  
**Resource Acquisition Is Initialization (RAII)** binds resource management to object lifetime. When a scoped lock is constructed on the stack, it acquires the mutex. When execution leaves scope (via return, break, or exception), its destructor automatically releases the mutex. This eliminates **deadlocks caused by unreleased locks**.

### Q14: Explain the difference between Threads and Processes in SmartATC.
**Answer:**  
- **Simulated Process:** A conceptual domain entity (`Models::Process`) representing a flight job with its own PCB, timing metrics, and state.
- **C++ Thread:** A physical OS execution thread (`std::thread`). Used in `ConcurrentAllocationDemo` to launch simultaneous competing worker routines that verify `std::mutex` synchronization under contention.

---

## Section 4: Database Management & ACID Transactions

### Q15: What is a Database Transaction?
**Answer:**  
A transaction is a logical unit of database work consisting of multiple SQL operations executed atomically. Either all statements succeed and changes are permanently committed, or any failure triggers a complete rollback to the initial state.

### Q16: Explain how SmartATC fulfills all four ACID properties.
**Answer:**  
1. **Atomicity:** `ApplicationService::allocateRunway()` encapsulates locking queries, `INSERT INTO runway_assignments`, `UPDATE runways`, `UPDATE flights`, and `INSERT INTO operation_logs` between `db.transaction()` and `db.commit()`. If any step fails, `db.rollback()` reverts all modifications completely.
2. **Consistency:** Schema constraints (foreign keys on `aircraft_id`, `flight_id`, `runway_id`, `role_id`, unique keys, priority checks) guarantee that the database transitions exclusively from one valid state to another.
3. **Isolation:** MySQL uses the `REPEATABLE READ` isolation level and pessimistic row locking (`SELECT ... FOR UPDATE`) to prevent dirty reads, non-repeatable reads, and lost updates across concurrent transactions.
4. **Durability:** MySQL InnoDB Write-Ahead Logging (WAL) via redo logs ensures that once `commit()` returns success, modifications survive application crashes or database restarts.

### Q17: What is the purpose of `SELECT ... FOR UPDATE`?
**Answer:**  
`SELECT ... FOR UPDATE` acquires an exclusive pessimistic write lock on queried row(s) in InnoDB. Any concurrent transaction attempting to read or modify the same runway or flight row is blocked until the locking transaction issues a `COMMIT` or `ROLLBACK`. This prevents two transactions from simultaneously observing a runway as `Available` and issuing conflicting assignments (lost update anomaly).

### Q18: What database schema normalization level did you achieve?
**Answer:**  
All 8 relational tables in SmartATC achieve **Third Normal Form (3NF)**:
- **1NF:** Atomic attributes and scalar fields with explicit primary keys.
- **2NF:** No partial functional dependencies on composite keys.
- **3NF:** No transitive dependencies (e.g., role names reside in `roles`, aircraft details reside in `aircraft`, assignment history resides in `runway_assignments`).

---

## Section 5: Architecture, Security & Limitations

### Q19: Explain the layered architecture of SmartATC.
**Answer:**  
SmartATC follows a 4-tier layered architecture:
1. **Presentation Layer (Qt GUI):** Views (`LoginView`, `MainWindow`, `DashboardView`, `FlightsView`, `AircraftView`, `RunwaysView`, `SchedulingView`) presenting controls and emitting user events.
2. **Application Service Layer:** `ApplicationService` acting as a central facade orchestrating business logic, RBAC validation, and coordinating between simulation and persistence.
3. **Core Domain & Simulation Layer:** `Scheduler`, `ReadyQueue`, `AgingEngine`, and `ResourceManager` maintaining in-memory state, queue ordering, and `std::mutex` locking.
4. **Data Access & Persistence Layer:** `DatabaseManager` managing connection pooling and executing prepared SQL statements via Qt SQL against MySQL 8.0.

### Q20: How does the application handle Role-Based Access Control (RBAC)?
**Answer:**  
Users are assigned one of three roles: `Admin`, `ATC`, or `Airport Operator`. RBAC is enforced in the service layer via `Session::hasPermission()`:
- `ATC`: Authorized for flight operations (`action:ManageFlights`), emergency declarations (`action:DeclareEmergency`), runway allocation (`action:RunwayAllocation`), and scheduling (`action:ManageScheduling`).
- `Airport Operator`: Authorized for runway maintenance (`action:RunwayMaintenance`) and aircraft management (`action:ManageAircraft`).
- `Admin`: Authorized for user administration (`action:ManageUsers`), scheduling management, and system-wide monitoring.
- Runway release is permitted for ATC, Airport Operator, and Admin.

### Q21: How are passwords secured in the system?
**Answer:**  
Passwords are never stored in plaintext. They are hashed using **SHA-256** cryptographic hashing (`QCryptographicHash`) in `PasswordUtils`.  
*Educational Limitation:* The current academic implementation uses unsalted SHA-256. In production environments, salted hashing algorithms with deliberate work factors (such as **Argon2id** or **bcrypt**) must be used to resist rainbow table and GPU dictionary attacks.

### Q22: How does the system handle offline mode when MySQL is unavailable?
**Answer:**  
If `db.conf` is missing or the MySQL connection fails, `DatabaseManager` flags offline mode. `ApplicationService` gracefully degrades to **In-Memory Simulation Mode**:
- Flights and runways are populated in volatile memory.
- All OS scheduling, Ready Queue operations, Priority Aging, and thread contention simulations function 100% in-memory.
- Transaction persistence is bypassed, allowing educational demonstrations and unit tests to run anywhere without requiring a live database server.

### Q23: What automated test suites verify the application?
**Answer:**  
SmartATC includes **19 automated CTest suites** comprising over 360 individual test assertions covering:
- Models (`TestFlightModel`, `TestAircraftModel`, `TestProcessModel`, `TestRunwayModel`)
- Scheduling (`TestReadyQueue`, `TestScheduler`, `TestDynamicEmergencyPriority`, `TestPriorityAging`, `TestStarvationSimulation`)
- Concurrency (`TestCriticalSection`, `TestThreadSynchronization`)
- DBMS (`TestDatabaseTransactions`, `TestACIDProperties`)
- Integration (`TestCompleteIntegration`, `TestAuthentication`, `TestOperationLogs`)

### Q24: What are the primary technical limitations of the current implementation?
**Answer:**  
1. Desktop user-space simulation rather than a distributed real-time system.
2. Unsalted SHA-256 password hashing (Argon2id recommended for production).
3. Non-preemptive scheduling does not simulate runway go-around or emergency wave-off maneuvers.
4. Simplified operational categories (`Landing`, `Takeoff`, `Both`) rather than dynamic meteorological and wake vortex radar feeds.

### Q25: How would you scale SmartATC for real-world multi-airport airspace control?
**Answer:**  
1. Transition desktop client to microservices architecture communicating via gRPC or WebSockets.
2. Implement distributed partitioned scheduling across multiple airport nodes with Kafka or Redis queues.
3. Replace single-instance pessimistic row locking with event sourcing and distributed transactions (Saga pattern).
4. Ingest real-time ADS-B transponder telemetry data streams.
