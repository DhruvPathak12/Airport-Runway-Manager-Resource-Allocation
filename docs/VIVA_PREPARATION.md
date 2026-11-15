# Comprehensive Viva & Technical Evaluation Guide

This document prepares project members for oral examinations (viva voce), project presentations, technical reviews, and academic evaluations for the **Smart Air Traffic Control Management System (SmartATC)**.

---

## Section 1: Project Overview & Core Philosophy

### Q1: What is the core objective of the Smart Air Traffic Control Management System?
**Answer:**  
SmartATC is an educational desktop simulation designed to demonstrate fundamental concepts of **Operating Systems (OS)** and **Database Management Systems (DBMS)** through the real-world domain analogy of airport runway operations.  
- **OS Concepts Demonstrated:** Process Control Blocks (PCB), Process State Transitions, Non-Preemptive Priority CPU Scheduling, Starvation & Priority Aging, Critical Sections, Mutual Exclusion, and Multi-threaded Synchronization.
- **DBMS Concepts Demonstrated:** Relational Modeling, 3NF Normalization, ACID Transactions, Atomic Multi-Statement Operations, Pessimistic Row Locking (`SELECT ... FOR UPDATE`), and Audit Trails.

### Q2: Why did you choose C++17, Qt 6, and MySQL 8 for this project?
**Answer:**  
1. **C++17:** Provides native, zero-cost abstractions for systems programming, including modern concurrency (`std::thread`, `std::mutex`, `std::unique_lock`), deterministic memory management via RAII, and strong type safety required for simulation fidelity.
2. **Qt 6 Widgets:** Offers a mature, native cross-platform GUI architecture with Model-View separation, signal-and-slot event decoupling, and robust database abstraction (`QSqlDatabase`, `QSqlQuery`).
3. **MySQL 8 (InnoDB):** Enterprise-grade relational engine supporting true ACID properties, row-level pessimistic locking (`FOR UPDATE`), foreign key cascading constraints, and consistent crash recovery.

### Q3: What is the analogy between airport operations and computing systems?
**Answer:**  
- **Flight Request $\rightarrow$ Process:** An incoming or departing aircraft represents an executable computing job requiring system servicing.
- **Flight Metadata $\rightarrow$ Process Control Block (PCB):** Priority, arrival time, state, and flight identifiers represent the internal scheduling metadata of a task.
- **Air Traffic Queue $\rightarrow$ Ready Queue:** Flights awaiting runway clearance correspond to CPU processes waiting in the OS Ready Queue.
- **Runway $\rightarrow$ CPU / Shared Resource:** A runway is a physical, exclusive bottleneck resource that can only service one job at a time.
- **Runway Occupancy $\rightarrow$ Critical Section Execution:** Allocating and occupying a runway represents entering and executing within a protected critical section.

---

## Section 2: Operating System & Scheduling Concepts

### Q4: How is a Process and Process Control Block (PCB) implemented in SmartATC?
**Answer:**  
A Process is modeled by the `Process` class in `src/models/Process.h`. It acts as the PCB, maintaining:
- `m_processId`: Unique integer process identifier.
- `m_flightId`: Foreign reference linking to domain entity.
- `m_state`: Enumerated state (`READY`, `RUNNING`, `BLOCKED`, `COMPLETED`).
- `m_basePriority` & `m_effectivePriority`: Integer scale (1 = Low to 5 = Emergency).
- `m_arrivalTime`, `m_burstTime`, `m_waitingTime`, `m_turnaroundTime`, and `m_completionTime`.
- `m_agingLevel`: Counter tracking accumulated priority promotions.

### Q5: Describe the process lifecycle state transitions.
**Answer:**  
1. **Creation $\rightarrow$ READY:** When a flight is scheduled, its corresponding process enters the Ready Queue with state `READY`.
2. **READY $\rightarrow$ RUNNING:** The scheduler dispatches the process when a compatible runway is available. The process enters `RUNNING`.
3. **RUNNING $\rightarrow$ BLOCKED:** If a runway experiences sudden maintenance or weather hold, the process is transitioned to `BLOCKED`.
4. **BLOCKED $\rightarrow$ READY:** When runway clearance or maintenance completes, the process is placed back into `READY`.
5. **RUNNING $\rightarrow$ COMPLETED:** Once the aircraft completes takeoff/landing, resource is released, state is updated to `COMPLETED`, and metrics are logged.

### Q6: Why did you choose a Non-Preemptive Priority Scheduler instead of Preemptive?
**Answer:**  
In physical aviation, once an aircraft enters final touchdown or begins high-speed takeoff roll on a runway, aborting mid-action (preempting) introduces severe catastrophic safety hazards. Therefore, non-preemptive scheduling accurately models runway physics: once allocated, a flight retains the runway until completion of its burst duration.

### Q7: How does your scheduler break ties when two processes share the same priority?
**Answer:**  
SmartATC implements a strict 3-tier deterministic sorting hierarchy in `ReadyQueue`:
1. **Effective Priority (Descending):** Highest priority process is scheduled first (5 down to 1).
2. **Arrival Time (Ascending - FCFS):** If priorities are equal, earlier arrival time breaks the tie.
3. **Flight ID (Ascending):** If priorities and arrival times are identical, unique flight identifier provides deterministic ordering.

### Q8: What are Turnaround Time, Waiting Time, and Response Time? State their formulas.
**Answer:**  
- **Completion Time ($CT$):** The timestamp when the process finishes execution.
- **Turnaround Time ($TAT$):** Total elapsed time from process arrival to completion:
  $$TAT = CT - AT$$
- **Waiting Time ($WT$):** Total time spent waiting in the Ready Queue prior to execution:
  $$WT = TAT - BT$$
- **Response Time ($RT$):** Time from arrival until first dispatch to CPU. (In non-preemptive scheduling without interruption, $RT = WT$).

### Q9: How does Dynamic Emergency Priority work?
**Answer:**  
When an aircraft declares an in-flight emergency (e.g., fuel distress, engine anomaly), `ApplicationService::declareFlightEmergency()` immediately:
1. Sets `is_emergency = true` in MySQL.
2. Escalates `m_effectivePriority = 5` (Emergency maximum).
3. Re-sorts the `ReadyQueue`.  
This immediately places the distressed flight at the head of the queue, ensuring it is dispatched at the very next runway availability without preempting the active flight.

### Q10: What is Starvation, and how does your Priority Aging algorithm prevent it?
**Answer:**  
- **Starvation:** In pure priority scheduling, a continuous stream of high-priority jobs can cause lower-priority jobs (e.g., cargo flights) to wait indefinitely in the Ready Queue.
- **Priority Aging:** Aging dynamically increases the effective priority of waiting processes based on elapsed waiting time.
  $$\text{Effective Priority} = \min\left(\text{MaxCap}, \text{BasePriority} + \left\lfloor\frac{\text{WaitingTime}}{\text{AgingInterval}}\right\rfloor \times \text{AgingIncrement}\right)$$
  As waiting time accumulates, the low-priority job's effective priority rises, guaranteeing bounded waiting time and eventual execution.

---

## Section 3: Concurrency, Threads & Mutual Exclusion

### Q11: What is a Critical Section, and what shared resource does it protect?
**Answer:**  
A Critical Section is a segment of code that accesses shared, mutable resources that must not be concurrently accessed by more than one execution thread.  
In SmartATC, the **Runway** is the shared resource. If two flight worker threads attempt to acquire the same runway simultaneously, a race condition occurs. The critical section encapsulates the eligibility check, reservation update, and assignment insertion.

### Q12: How is Mutual Exclusion implemented in SmartATC?
**Answer:**  
Mutual exclusion is implemented at two distinct architectural levels:
1. **In-Memory (C++ Level):** Handled via `std::mutex m_resourceMutex` inside `ResourceManager`. Thread acquisition utilizes the RAII construct `std::unique_lock<std::mutex> lock(m_resourceMutex)`. This guarantees exclusive access and automatic lock release even if an exception is thrown.
2. **Database (DBMS Level):** Handled via MySQL InnoDB pessimistic row-level locking (`SELECT status FROM runways WHERE id = ? FOR UPDATE`).

### Q13: What is RAII, and why is it critical for lock management?
**Answer:**  
**Resource Acquisition Is Initialization (RAII)** binds resource lifetime to object lifetime. When `std::unique_lock` or `std::lock_guard` is constructed on the stack, it acquires the mutex. When it exits scope (via return, break, or exception unwinding), its destructor automatically releases the mutex. This eliminates **deadlocks caused by unreleased locks**.

### Q14: Explain the difference between Threads and Processes.
**Answer:**  
- **Process:** An independent execution entity with its own dedicated address space, PCB, file handles, and memory boundaries. Inter-process communication requires IPC (pipes, sockets, shared memory). In our project, simulated flights are modeled as conceptual OS processes.
- **Thread:** A lightweight unit of execution within a process that shares the parent process's address space, code segment, and heap, but possesses its own program counter and stack. In our project, C++ `std::thread`s are used in `ConcurrentAllocationDemo` to simulate concurrent actors competing for resources.

---

## Section 4: Database Management & ACID Transactions

### Q15: What is a Database Transaction?
**Answer:**  
A transaction is a logical unit of database work consisting of one or more SQL operations executed as an atomic unit. Either all statements succeed and changes are permanently committed, or any failure triggers a complete rollback to the initial state.

### Q16: Explain how SmartATC fulfills all four ACID properties.
**Answer:**  
1. **Atomicity:** `ApplicationService::allocateRunwayWithTransaction()` wraps 4 SQL statements (`UPDATE flights`, `INSERT INTO runway_assignments`, `UPDATE runways`, `UPDATE scheduling_queue`). If statement 3 fails, `QSqlDatabase::rollback()` restores all previous updates.
2. **Consistency:** Schema constraints (foreign keys on `flight_id`, `runway_id`, unique aircraft registrations, enum status checks) ensure the database transitions exclusively from one valid state to another.
3. **Isolation:** MySQL uses the `REPEATABLE READ` isolation level and pessimistic row locking (`SELECT ... FOR UPDATE`) to prevent dirty reads, non-repeatable reads, and lost updates across concurrent transactions.
4. **Durability:** The InnoDB Write-Ahead Logging (WAL) via redo logs ensures that once `commit()` returns success, modifications survive server crashes or restarts.

### Q17: What is the purpose of `SELECT ... FOR UPDATE`?
**Answer:**  
`SELECT ... FOR UPDATE` acquires an exclusive pessimistic write lock on the queried row(s) in InnoDB. Any concurrent transaction attempting to read or modify the same runway row is blocked until the first transaction issues a `COMMIT` or `ROLLBACK`. This prevents two transactions from simultaneously observing a runway as `FREE` and issuing conflicting assignments (lost update anomaly).

### Q18: What database schema normalization level did you achieve?
**Answer:**  
All relational tables in SmartATC achieve **Third Normal Form (3NF)**:
- **1NF:** All column values are atomic (no repeating groups or comma-separated lists).
- **2NF:** All non-key attributes are fully functionally dependent on the entire primary key (no partial dependencies).
- **3NF:** No non-key attribute is transitively dependent on another non-key attribute (e.g., aircraft model and manufacturer reside in `aircraft`, while flight details reference `aircraft_id`).

---

## Section 5: Architecture, Security & Limitations

### Q19: Explain the layered architecture of SmartATC.
**Answer:**  
SmartATC follows a strict 4-tier layered architecture:
1. **Presentation Layer (Qt GUI):** Views (`LoginView`, `MainWindow`, `DashboardView`, etc.) presenting UI controls and emitting user events.
2. **Application Service Layer:** `ApplicationService` acting as the central facade orchestrating business logic, RBAC validation, and coordinating between simulation and persistence.
3. **Core Domain & Simulation Layer:** `Scheduler`, `ReadyQueue`, `AgingEngine`, and `ResourceManager` maintaining in-memory state, queue ordering, and mutex locking.
4. **Data Access Layer:** `DatabaseManager` and DAO classes (`FlightDao`, `RunwayDao`, etc.) executing parameterized SQL through Qt SQL against MySQL.

### Q20: How does the application handle Role-Based Access Control (RBAC)?
**Answer:**  
Users are assigned one of three roles: `Admin`, `ATC`, or `Airport Operator`. RBAC is enforced in the service layer:
- `ApplicationService` verifies the active session role before executing sensitive operations (e.g., only `Admin` and `ATC` can dispatch processes; only `Admin` and `Airport Operator` can toggle runway maintenance).
- The GUI disables or hides buttons based on permissions.

### Q21: How are passwords secured in the system?
**Answer:**  
Passwords are never stored in plaintext. They are hashed using **SHA-256** cryptographic hashing (`QCryptographicHash`) in `AuthService`. In production environments, modern key-derivation functions with salting (such as **Argon2id** or **bcrypt**) are recommended to mitigate rainbow table and GPU brute-force attacks.

### Q22: How does the system handle offline mode when MySQL is unavailable?
**Answer:**  
If `db.conf` is missing or the MySQL connection fails, `DatabaseManager` flags offline mode. `ApplicationService` gracefully degrades to **In-Memory Simulation Mode**:
- Flights and runways are populated in volatile memory.
- All OS scheduling, Ready Queue operations, Priority Aging, and thread contention simulations function 100% in-memory.
- Transaction persistence is bypassed, allowing educational demonstrations and unit tests to run anywhere.

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
1. Single-node architecture without distributed consensus (Raft/Paxos).
2. Simplified wake vortex separation intervals rather than dynamic meteorological radar feeds.
3. Non-preemptive scheduling does not model runway abort/go-around maneuvers.
4. SHA-256 hashing lacks per-user salt and computational work factor (Argon2id recommended).

### Q25: How would you scale SmartATC for real-world multi-airport airspace control?
**Answer:**  
1. Transition the desktop client to a microservices architecture using gRPC or WebSocket streams.
2. Implement distributed partitioned scheduling across multiple airport nodes with Redis / Kafka queues.
3. Replace pessimistic row locking with event sourcing and distributed transactions (Saga pattern).
4. Integrate real-time ADS-B transponder telemetry ingestion engines.
