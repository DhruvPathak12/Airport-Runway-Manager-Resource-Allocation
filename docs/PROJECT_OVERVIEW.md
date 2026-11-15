# Project Overview — Smart Air Traffic Control Management System (SmartATC)

## 1. Project Title
**Smart Air Traffic Control Management System (SmartATC): Airport Runway Manager & Resource Allocation**
*An Educational Desktop Simulation Demonstrating Operating System and Database Management System Principles.*

---

## 2. Educational Simulation Disclaimer
> [!IMPORTANT]
> **Academic & Educational Disclaimer:**
> SmartATC is an educational simulation developed as a B.Tech Computer Science & Engineering capstone project. It is **not** a safety-critical avionics or Federal Aviation Administration (FAA) certified Air Traffic Control platform. It does not communicate with physical transponders or radar arrays. Air traffic operations are modeled conceptually to provide an intuitive, high-stakes domain for demonstrating concurrent Operating System resource management and ACID database transaction processing.

---

## 3. Executive Purpose & Problem Statement

### 3.1 The Real-World Domain Problem
At modern commercial hub airports, runways represent the primary operational bottleneck. High arrival and departure flight volumes constantly contend for a limited set of physical runway surfaces. Ground operations require:
1. Strict ordering of competing requests according to flight priority, fuel status, and waiting duration to avoid indefinite postponement.
2. Immediate prioritization of emergencies (low fuel, mechanical failure, medical emergencies) over regular commercial traffic.
3. Strict physical exclusivity: at no point may two aircraft occupy or execute on the same runway simultaneously.
4. Guaranteed transactional integrity: runway reservations, flight status transitions, and audit logging must commit atomically to prevent ghost assignments or split-brain allocation states.

### 3.2 The Conceptual Computing Analogy
Runway management perfectly mirrors core Operating System and Database Management System challenges:
- **Runway = Shared Hardware Resource (CPU Core / Exclusive I/O Device):** Only one process can execute on a runway at a time.
- **Flight Operation = Process / Thread:** An active execution unit with CPU burst time (runway occupancy service duration), arrival timestamp, and base/dynamic priority.
- **Approach & Departure Queue = Ready Queue:** Schedulable entities awaiting dispatch governed by scheduling policies.
- **Runway Allocation = Critical Section Entry & Resource Acquisition:** Requires mutual exclusion locks (`std::mutex`) and ACID database transactions.
- **Emergency Declaration = Non-Maskable Interrupt (NMI) / Priority Boost:** Immediately elevates candidate process to highest priority.
- **Waiting Duration = Starvation & Aging:** Low-priority flights experience starvation unless an aging engine gradually elevates their priority.

---

## 4. Proposed Solution
SmartATC provides a native C++17 desktop application built on Qt 6 Widgets and a normalized MySQL 8 relational database backend. The system integrates:
1. **Interactive Air Traffic Management:** CRUD management of registered aircraft, flight schedules, and airport runway infrastructure.
2. **Deterministic OS Process Scheduler:** A non-preemptive priority scheduling engine that manages simulated Process Control Blocks (PCBs) in a FIFO ready queue with dynamic emergency overrides and priority aging.
3. **Mutual Exclusion & Thread Synchronization:** In-memory resource locks (`std::mutex`, `std::unique_lock`, `ScopedResourceLock`) that prevent concurrent threads from acquiring the same runway simultaneously.
4. **Pessimistic ACID MySQL Transactions:** Multi-statement transactions using InnoDB row locks (`SELECT ... FOR UPDATE`), `START TRANSACTION`, `COMMIT`, and `ROLLBACK` for guaranteed state consistency.
5. **Real-Time Operational Monitoring:** A data-driven dashboard presenting key performance indicators, live queue depth, runway capacity gauges, active emergency alerts, and immutable operation audit logs.

---

## 5. Project Objectives
1. **Model Process Scheduling:** Implement simulated Process Control Blocks (PCBs) with arrival times, service durations, base priorities, effective priorities, waiting times, and turnaround times.
2. **Demonstrate Priority Aging:** Formulate an aging algorithm that eliminates starvation for low-priority processes by dynamically adjusting priorities based on continuous waiting time.
3. **Enforce Mutual Exclusion:** Protect physical runways with RAII-based mutexes, demonstrating race condition prevention under multi-threaded contention.
4. **Guarantee Database ACID Properties:** Ensure all runway allocations, releases, and log insertions either commit entirely or rollback completely under simulated failures.
5. **Implement Role-Based Access Control (RBAC):** Restrict system operations across three distinct operational roles: `Admin`, `ATC` (Air Traffic Controller), and `Airport Operator`.

---

## 6. Technology Stack

| Layer | Component | Version / Standard | Purpose |
|---|---|---|---|
| **Programming Language** | C++ | ISO C++17 | Core logic, multithreading, synchronization, memory safety |
| **GUI Framework** | Qt Widgets | Qt 6.11.2 | Professional cross-platform desktop UI, signals/slots, event loop |
| **Database Engine** | MySQL Server | MySQL 8.0.46 (InnoDB) | Normalized relational storage, ACID transactions, row locking |
| **Database Driver** | Qt SQL | QMYSQL (`libmysql.dll`) | High-performance C++ database connectivity and prepared statements |
| **Build System** | CMake / Ninja | CMake 3.19+ | Modern declarative cross-platform build orchestration |
| **Compiler Toolchain** | MinGW-w64 GCC | GCC 13.1.0 (64-bit) | Native Windows compilation and binary optimization |
| **Test Harness** | Qt Test / CTest | QtTest 6.11.2 | Automated unit, regression, integration, and ACID verification |
| **Version Control** | Git / GitHub | Git 2.x | Source tracking, commit history, and milestone tagging |

---

## 7. Major Features Summary

1. **Authentication & RBAC:** Secure SHA-256 hashed password verification with granular permission flags (`page:Dashboard`, `action:ManageScheduling`, `action:DeclareEmergency`, `action:RunwayMaintenance`).
2. **Flight & Fleet Administration:** Normalized aircraft registry and flight lifecycle tracking (`Scheduled`, `Taxiing`, `Cleared`, `Landed`, `Departed`, `Cancelled`).
3. **Runway Infrastructure Management:** Status classification (`Available`, `Occupied`, `Maintenance`) and operational categorization (`Landing`, `Takeoff`, `Both`).
4. **Simulated OS Process Lifecycle:** Complete transition state machine (`New` $\to$ `Ready` $\to$ `Running` $\to$ `Completed` / `Blocked`).
5. **Non-Preemptive Priority Scheduling:** Deterministic dispatching based on effective priority (1=Highest to 5=Lowest), arrival time tie-breaking, and flight ID stability.
6. **Dynamic Emergency Priority Overrides:** Instantaneous elevation of emergency flights to priority 1 while strictly preserving base priorities upon emergency resolution.
7. **Priority Aging Engine:** Configurable wall-clock evaluation timer that systematically decrements numeric priority per interval to prevent starvation.
8. **Critical Section & Contention Simulation:** Multi-threaded stress testing (`ConcurrentAllocationDemo`) demonstrating race-free access to shared runway resources.
9. **Atomic MySQL Transactions:** Robust database transaction boundaries ensuring synchronized updates across `runways`, `flights`, `runway_assignments`, and `operation_logs`.
10. **Centralized Operational Dashboard:** Real-time KPI cards, safe zero-runway utilization calculation, emergency flight monitor, and audit log viewer.

---

## 8. Summary of Operating System Concepts

- **Process Representation & PCB:** Encapsulated in `Models::Process`, storing PID (`flightId`), queue ID, arrival time, CPU burst time, process state, base priority, and dynamic priority.
- **Process State Transition Machine:** Strictly enforced transitions between `Ready`, `Running`, `Blocked`, and `Completed`.
- **CPU Scheduling Algorithms:** Non-preemptive priority scheduling with arrival-time tie-breaking.
- **Starvation & Aging:** Mathematical proof and simulation demonstrating starvation prevention via periodic priority promotion.
- **Mutual Exclusion & Critical Sections:** Protection of shared resources using `std::mutex` and RAII-based `std::unique_lock`.
- **Concurrency & Synchronization:** Multi-threaded worker dispatching utilizing `std::thread` and `std::condition_variable` synchronization barriers.

---

## 9. Summary of DBMS Concepts

- **Relational Normalization:** 8-table relational database structured in Boyce-Codd Normal Form (BCNF) / 3NF.
- **Referential Integrity:** Enforced via foreign key constraints (`ON DELETE RESTRICT` / `CASCADE`), unique constraints, and check constraints.
- **ACID Transaction Management:**
  - *Atomicity:* Multi-statement execution encapsulated within `BEGIN ... COMMIT / ROLLBACK`.
  - *Consistency:* Schema validation and relational integrity preserved across all transitions.
  - *Isolation:* Row-level exclusive locks (`SELECT ... FOR UPDATE`) preventing dirty, non-repeatable, and phantom reads.
  - *Durability:* Persistent storage on InnoDB engine surviving simulated application crashes and restarts.
- **Audit Logging:** Immutable historical ledger tracking all administrative, scheduling, and resource operations with user attribution.

---

## 10. Major Limitations

1. **Simulated OS Kernel:** Scheduling runs as an in-process C++ application logic simulation; it does not replace the host OS (Windows/Linux) kernel process dispatcher.
2. **Non-Preemptive Execution:** Once a flight acquires a runway, its service burst must finish before another process can be scheduled.
3. **Password Hashing:** Passwords use unsalted SHA-256 for academic demonstration; production enterprise environments should transition to salted `Argon2id` or `bcrypt`.
4. **Local Database Scope:** Configured for single-node MySQL server access over localhost/private network rather than globally distributed multi-region topologies.
