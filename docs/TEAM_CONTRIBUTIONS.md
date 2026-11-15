# Team Contributions and Member Responsibilities

This document provides a detailed breakdown of individual technical contributions, module ownership, design implementations, and deliverables for the Smart Air Traffic Control Management System (**SmartATC**).

---

## 1. Overview of Team Structure

The SmartATC project was architected, implemented, tested, and documented by a four-member engineering team. Each member took primary ownership of specific layers and modules while participating in cross-functional integration, code reviews, and system-wide verification.

| Team Member | Module Ownership | Core Technical Domain | Primary Deliverables |
|:---|:---|:---|:---|
| **Member 1** | User & Role Management | Security, Auth, RBAC | `User`, `Session`, `PasswordUtils`, `LoginView`, Security Audit |
| **Member 2** | Flight & Aircraft Management | Domain Modeling, Fleet Tracking | `Flight`, `Aircraft`, `FlightsView`, `AircraftView`, Modals |
| **Member 3** | OS Process & Scheduling Simulation | Operating System Concepts | `Process`, `ReadyQueue`, `Scheduler`, `AgingEngine`, Metrics, Starvation Demo |
| **Member 4** | Resource Allocation & DBMS Transactions | Concurrency & Database Systems | `ResourceManager`, `DatabaseManager`, `std::mutex`, ACID Transactions, Threads |

---

## 2. Detailed Member Contributions

### Member 1: User, Role & Security Engineering
- **Domain:** Authentication, Authorization, Role-Based Access Control (RBAC), and Audit Trail.
- **Key Modules Authored:**
  - `src/models/User.h`
  - `src/models/Session.h`, `src/models/Session.cpp`
  - `src/utils/PasswordUtils.h`, `src/utils/PasswordUtils.cpp`
  - `ui/LoginView.h`, `ui/LoginView.cpp`
  - SQL DDL for `roles`, `users`, and `operation_logs` tables in `sql/schema.sql`.
- **Technical Responsibilities:**
  - Designed the role hierarchy (`Admin`, `ATC`, `Airport Operator`) and permissions matrix in `Session.cpp`.
  - Implemented secure SHA-256 password hashing and parameter-bound SQL queries to eliminate SQL injection vulnerabilities.
  - Implemented operational audit logging (`operation_logs` table) recording user action timestamps, entity targets, and transaction statuses (`Committed`, `Rolled Back`).
  - Integrated RBAC permission enforcement across the service layer and GUI actions.
- **Unit and Regression Tests Authored:**
  - `TestAuthentication`: Validated login success, wrong password rejection, unknown user rejection, and password hashing.
  - `TestOperationLogs`: Verified audit record insertion, retrieval, and parameter validation.

---

### Member 2: Flight & Aircraft Domain Engineering
- **Domain:** Aviation domain data models, fleet management, and schedule validation.
- **Key Modules Authored:**
  - `src/models/Flight.h`
  - `src/models/Aircraft.h`
  - `ui/FlightsView.h`, `ui/FlightsView.cpp`, `ui/FlightDialog.h`, `ui/FlightDialog.cpp`
  - `ui/AircraftView.h`, `ui/AircraftView.cpp`, `ui/AircraftDialog.h`, `ui/AircraftDialog.cpp`
  - SQL DDL for `aircraft` and `flights` tables in `sql/schema.sql`.
- **Technical Responsibilities:**
  - Engineered relational entities for aircraft models, passenger capacity, and registration validation.
  - Implemented flight lifecycle modeling (`Arrival` / `Departure`, `Scheduled`, `Taxiing`, `Cleared`, `Landed`, `Departed`, `Cancelled`).
  - Developed Qt GUI tables with model-view architecture for real-time fleet and schedule inspection.
  - Connected flight scheduling data to the underlying OS Process generation pipeline.
- **Unit and Regression Tests Authored:**
  - `TestFlightModel`: Validated flight code formats, arrival/departure flags, and status transitions.
  - `TestAircraftModel`: Validated aircraft registration integrity, capacity, and airworthiness status.
  - `TestFlightManagement`, `TestAircraftManagement`: Tested persistence and foreign key relational constraints with MySQL.

---

### Member 3: OS Concepts & Process Scheduling Simulation
- **Domain:** Operating System simulation, Process Control Block (PCB), Ready Queue, Priority Scheduling, Priority Aging, Starvation Prevention.
- **Key Modules Authored:**
  - `src/models/Process.h`, `src/models/Process.cpp`
  - `src/scheduler/ReadyQueue.h`, `src/scheduler/ReadyQueue.cpp`
  - `src/scheduler/Scheduler.h`, `src/scheduler/Scheduler.cpp`
  - `src/scheduler/AgingEngine.h`, `src/scheduler/AgingEngine.cpp`
  - `src/scheduler/StarvationSimulation.h`, `src/scheduler/StarvationSimulation.cpp`
  - `ui/SchedulingView.h`, `ui/SchedulingView.cpp`
  - SQL DDL for `scheduling_queue` table in `sql/schema.sql`.
- **Technical Responsibilities:**
  - Designed the Process Control Block (`Process` model) mapping flight requests to simulated processes with states (`Ready`, `Running`, `Blocked`, `Completed`).
  - Implemented the thread-safe `ReadyQueue` priority queue sorted by effective priority (1=Highest), arrival time, and flight ID.
  - Engineered the Non-Preemptive Priority Scheduler computing turnaround time, waiting time, response time, and metrics.
  - Developed Dynamic Emergency Priority handling guaranteeing Priority 1 escalation for distressed aircraft.
  - Designed the configurable **Priority Aging Engine** preventing low-priority starvation.
  - Implemented comparative starvation simulation verifying wait time reductions when aging is active.
- **Unit and Regression Tests Authored:**
  - `TestProcessModel`, `TestReadyQueue`, `TestScheduler`: Verified priority dispatching, tie-breaking, and state transitions.
  - `TestDynamicEmergencyPriority`: Tested priority escalation for emergency declarations.
  - `TestPriorityAging`, `TestStarvationSimulation`: Verified aging decrement logic, upper bounds, and starvation mitigation metrics.

---

### Member 4: Concurrency, Resource Allocation & DBMS Transactions
- **Domain:** Critical section protection, mutual exclusion, C++ multi-threading, MySQL ACID transactions, and pessimistic locking.
- **Key Modules Authored:**
  - `src/resource/ResourceManager.h`, `src/resource/ResourceManager.cpp`
  - `src/resource/ConcurrentAllocationDemo.h`, `src/resource/ConcurrentAllocationDemo.cpp`
  - `src/database/DatabaseManager.h`, `src/database/DatabaseManager.cpp`
  - `src/models/Runway.h`, `src/models/RunwayAssignment.h`
  - `ui/RunwaysView.h`, `ui/RunwaysView.cpp`, `ui/RunwayDialog.h`, `ui/RunwayDialog.cpp`, `ui/RunwayAllocationDialog.h`, `ui/RunwayAllocationDialog.cpp`
  - `ui/DashboardView.h`, `ui/DashboardView.cpp`
  - SQL DDL for `runways` and `runway_assignments` in `sql/schema.sql`.
- **Technical Responsibilities:**
  - Designed runway infrastructure representation, operation compatibility checking, and status lifecycle (`Available`, `Occupied`, `Maintenance`).
  - Implemented mutual exclusion using `std::mutex` and custom RAII `ScopedResourceLock` ensuring atomic runway allocations in memory.
  - Developed multi-threaded concurrency demonstration executing simultaneous flight worker threads competing for single runways without race conditions.
  - Implemented atomic multi-statement MySQL transactions (`db.transaction()`, `db.commit()`, `db.rollback()`) across tables.
  - Implemented pessimistic concurrency control (`SELECT ... FOR UPDATE`) in InnoDB to serialize concurrent database allocations.
  - Built ACID property verification test suites and failure injection mechanisms.
- **Unit and Regression Tests Authored:**
  - `TestCriticalSection`: Verified exclusive resource ownership and conflict rejection.
  - `TestThreadSynchronization`: Tested multi-threaded contention, join semantics, and race-free resource state.
  - `TestDatabaseTransactions`, `TestACIDProperties`: Verified atomicity on failure, consistency invariants, transaction isolation, and durability.

---

## 3. Collaborative & Shared Milestones

Throughout the 26-day engineering lifecycle, all team members collaborated on cross-cutting tasks:
1. **Architecture & Schema Design (Days 1–5):** Jointly established the 8-table relational schema and layered C++ architecture.
2. **Phase 1 Consolidation (Day 13):** Verified headless build compatibility and cross-platform compilation.
3. **End-to-End System Integration (Day 23):** Integrated flight creation $\to$ process generation $\to$ priority scheduling $\to$ thread-safe allocation $\to$ MySQL transaction commit $\to$ audit logging.
4. **Dashboard & Operational Monitoring (Day 24):** Collaboratively bound the Qt Dashboard UI to live service statistics.
5. **Regression Testing & Hardening (Day 25):** Fixed cross-cutting defects (MySQL 8 DDL syntax, aging byte overflow, transaction fixtures) and verified 19 test suites.
6. **Project Documentation & Viva Preparation (Day 26):** Authored technical reports, system manuals, and architecture diagrams.
