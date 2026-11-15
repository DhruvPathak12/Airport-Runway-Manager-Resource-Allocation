# Project Overview — Smart Air Traffic Control Management System (SmartATC)

## 1. Project Title
**Smart Air Traffic Control Management System (SmartATC): Airport Runway Manager & Resource Allocation**  
*An Educational Desktop Simulation Demonstrating Operating System and Database Management System Principles in C++.*

---

## 2. Educational Simulation Disclaimer
> [!IMPORTANT]
> **Academic & Educational Simulation Disclaimer:**  
> SmartATC is an educational desktop application developed as a B.Tech Computer Science & Engineering capstone project. It is **not** safety-critical avionics software and does not interface with physical radar, aircraft transponders, or certified Air Traffic Control (ATC) infrastructure.
>
> The application runs entirely in user space on the host operating system (Windows) and simulates Operating System resource allocation and Database Management System transactions through the analogy of an airport control center. It **does not implement a kernel scheduler**, nor does it control actual aircraft.

---

## 3. Executive Purpose & Problem Statement

### 3.1 The Real-World Domain Problem
At commercial airports, runways represent physical bottleneck resources that multiple arriving and departing flights contend for simultaneously. Safe ground operations require:
1. Ordered request processing based on flight priority, scheduled arrival, and waiting duration to avoid indefinite postponement.
2. Immediate prioritization of emergencies (distress calls, fuel shortages, mechanical failures).
3. Physical exclusivity: no two aircraft may occupy the same runway simultaneously.
4. Transactional integrity: runway allocation, flight status transitions, and audit logging must commit atomically to prevent conflicting or orphaned assignments.

### 3.2 The Conceptual Operating System Analogy
Airport runway management provides a concrete, intuitive domain for demonstrating Operating System and Database Management System principles:

| Airport Domain Concept | Operating System Concept | SmartATC Simulation Implementation |
|:---|:---|:---|
| **Flight** | **Simulated Process** | `Models::Process` representing a schedulable job with PID (`flightId`). |
| **Flight Waiting Queue** | **Ready Queue** | `SchedulerComponents::ReadyQueue` holding runnable processes. |
| **Flight Priority** | **Process Priority** | Priority scale from 1 (Highest) to 5 (Lowest). |
| **Emergency Flight** | **High-Priority Process** | Dynamic escalation to Priority 1 override. |
| **Runway** | **Shared OS Resource** | Critical section protected by `std::mutex` and database row locks. |
| **Runway Assignment** | **Resource Allocation** | Allocation of an exclusive shared resource to a process. |
| **Long Waiting** | **Starvation Risk** | Indefinite waiting of low-priority processes under high traffic. |
| **Priority Aging** | **Starvation Prevention** | Dynamic priority improvement over time via `AgingEngine`. |

> [!NOTE]
> **Resource Allocation vs. CPU Analogy:**  
> A runway is a **shared OS resource** subject to mutual exclusion, not a CPU core. Aircraft do not execute CPU instruction cycles; rather, simulated flight processes hold exclusive access to an allocated runway resource for a defined operational service duration (`serviceTime`) before releasing it.

---

## 4. Proposed Solution
SmartATC provides a native C++17 desktop application built on Qt 6 Widgets and a normalized MySQL 8.0 relational database backend:
1. **Interactive Air Traffic Management:** CRUD operations for aircraft fleet records, flight schedules, and runway infrastructure.
2. **Deterministic OS Process Scheduler:** Application-level non-preemptive priority scheduling simulating process dispatching from a thread-safe ready queue with dynamic emergency overrides and priority aging.
3. **Mutual Exclusion & Thread Synchronization:** In-memory resource protection (`std::mutex`, `ScopedResourceLock`) preventing race conditions during concurrent allocation requests.
4. **Pessimistic ACID MySQL Transactions:** Multi-statement transactions using InnoDB row locks (`SELECT ... FOR UPDATE`), atomic `COMMIT`, and safe `ROLLBACK` for state consistency.
5. **Real-Time Operational Monitoring:** A data-driven dashboard presenting key performance indicators, live queue depth, runway status indicators, emergency flight alerts, and operation audit logs.

---

## 5. Project Objectives
1. **Model Process Scheduling:** Implement simulated Process Control Blocks (PCBs) with arrival times, service durations, base priorities, effective priorities, waiting times, and turnaround times.
2. **Demonstrate Priority Aging:** Formulate and verify an aging algorithm that eliminates starvation for low-priority processes by dynamically adjusting priorities based on continuous waiting time.
3. **Enforce Mutual Exclusion:** Protect physical runways with RAII-based mutexes, demonstrating race condition prevention under multi-threaded contention.
4. **Guarantee Database ACID Properties:** Ensure runway allocations, releases, and log insertions either commit entirely or roll back completely under normal and failure conditions.
5. **Implement Role-Based Access Control (RBAC):** Restrict system operations across three distinct operational roles: `Admin`, `ATC` (Air Traffic Controller), and `Airport Operator`.

---

## 6. Technology Stack

| Layer | Component | Version / Standard | Purpose |
|:---|:---|:---|:---|
| **Programming Language** | C++ | ISO C++17 | Core logic, multithreading, synchronization, memory safety |
| **GUI Framework** | Qt Widgets | Qt 6.11.2 | Cross-platform desktop UI, signals/slots, event loop |
| **Database Engine** | MySQL Server | MySQL 8.0+ (InnoDB) | Normalized relational storage, ACID transactions, row locking |
| **Database Driver** | Qt SQL | QMYSQL (`libmysql.dll`) | Database connectivity and prepared SQL statements |
| **Build System** | CMake / Ninja | CMake 3.19+ | Modern declarative cross-platform build orchestration |
| **Compiler Toolchain** | MinGW-w64 GCC | GCC 13.1.0 (64-bit) | Native Windows compilation and binary optimization |
| **Test Harness** | Qt Test / CTest | QtTest 6.11.2 | Automated unit, regression, integration, and ACID verification |
| **Version Control** | Git / GitHub | Git 2.x | Source tracking, commit history, and milestone tagging |

---

## 7. Major Features Summary

1. **Authentication & RBAC:** SHA-256 hashed password verification with granular permission keys (`page:Dashboard`, `action:ManageScheduling`, `action:DeclareEmergency`, `action:RunwayMaintenance`).
2. **Flight & Fleet Administration:** Normalized aircraft registry and flight lifecycle tracking (`Scheduled`, `Taxiing`, `Cleared`, `Landed`, `Departed`, `Cancelled`).
3. **Runway Infrastructure Management:** Status classification (`Available`, `Occupied`, `Maintenance`) and operational categorization (`Landing`, `Takeoff`, `Both`).
4. **Simulated OS Process Lifecycle:** Complete transition state machine (`New` $\to$ `Ready` $\to$ `Running` $\to$ `Completed` / `Blocked`).
5. **Non-Preemptive Priority Scheduling:** Deterministic dispatching based on effective priority (1 = Highest to 5 = Lowest), arrival time tie-breaking (FCFS), and flight ID stability.
6. **Dynamic Emergency Priority Overrides:** Instantaneous elevation of emergency flights to Priority 1 while strictly preserving base priorities upon emergency resolution.
7. **Priority Aging Engine:** Configurable wall-clock evaluation timer that systematically decrements numerical priority values per interval to prevent starvation.
8. **Critical Section & Contention Simulation:** Multi-threaded stress testing (`ConcurrentAllocationDemo`) demonstrating race-free access to shared runway resources.
9. **Atomic MySQL Transactions:** Database transaction boundaries ensuring synchronized updates across `runways`, `flights`, `runway_assignments`, and `operation_logs`.
10. **Centralized Operational Dashboard:** Real-time KPI cards, safe runway utilization calculation, emergency flight monitor, and audit log viewer.

---

## 8. Summary of Operating System Concepts

- **Process Representation & PCB:** Encapsulated in `Models::Process`, storing PID (`flightId`), queue ID, arrival time, simulated service time, process state, base priority, and effective priority.
- **Process State Transition Machine:** Enforced transitions between `Ready`, `Running`, `Blocked`, and `Completed`.
- **Priority Scheduling:** Non-preemptive priority scheduling with arrival-time tie-breaking.
- **Starvation & Aging:** Mathematical proof and simulation demonstrating starvation prevention via periodic priority promotion.
- **Mutual Exclusion & Critical Sections:** Protection of shared resources using `std::mutex` and RAII-based `ScopedResourceLock`.
- **Concurrency & Synchronization:** Multi-threaded worker dispatching utilizing `std::thread` and mutex synchronization.

---

## 9. Summary of DBMS Concepts

- **Relational Normalization:** 8-table relational database structured in 3NF.
- **Referential Integrity:** Enforced via foreign key constraints (`ON DELETE RESTRICT` / `CASCADE` / `SET NULL`), unique constraints, and check constraints.
- **ACID Transaction Management:** Multi-statement atomic transactions (`transaction()`, `commit()`, `rollback()`) across runway allocations and releases.
- **Concurrency Control & Isolation:** Pessimistic row locking (`SELECT ... FOR UPDATE`) in MySQL InnoDB preventing double allocation and lost updates.
- **Audit Logging:** Synchronized logging in `operation_logs` capturing operation status, timestamps, and user actions.

---

## 10. Final Demonstration & Evaluation Package

For academic defense and viva presentations, a dedicated suite of demonstration manuals, worked examples, and non-destructive reset runbooks is available in `docs/demo/`:

1. [**Demonstration Master Guide**](demo/DEMONSTRATION_GUIDE.md) — Master runbook, timing plan, navigation index, and viva contingency matrix.
2. [**Demonstration Script & Viva Presentation**](demo/DEMONSTRATION_SCRIPT.md) — Step-by-step 8-phase oral presentation script (18–20 minutes) with spoken text and cues.
3. [**Demonstration Data Catalog**](demo/DEMONSTRATION_DATA.md) — Concrete BOM scenario data: credentials, fleet, runways, and baseline PCB queue.
4. [**OS Concepts Demonstration Guide**](demo/OS_CONCEPT_DEMO.md) — Worked examples and verification of PCB, Priority Scheduling, Aging, and Mutex contention.
5. [**DBMS Transaction Demonstration Guide**](demo/DBMS_TRANSACTION_DEMO.md) — Real MySQL transactions, pessimistic row locking (`FOR UPDATE`), atomic commit, and rollback proof.
6. [**Demonstration Reset Guide**](demo/DEMO_RESET_GUIDE.md) — Non-destructive, idempotent reset instructions and SQL scripts.
7. [**Demonstration Verification Checklist**](demo/DEMONSTRATION_CHECKLIST.md) — Comprehensive 38-point pre-flight and live presentation verification audit.

