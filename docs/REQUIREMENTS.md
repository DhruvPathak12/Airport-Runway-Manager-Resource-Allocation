# Software Requirements Specification (SRS) — SmartATC

## 1. Introduction

### 1.1 Purpose
This document specifies the software requirements for the **Smart Air Traffic Control Management System (SmartATC)**. It outlines both the functional and non-functional requirements governing the application's architecture, user interface, scheduling engine, database transactions, and security enforcement.

### 1.2 Target Audience
This specification is designed for academic evaluators, software engineers, database administrators, and project stakeholders reviewing the architectural and operational compliance of SmartATC.

---

## 2. Overall System Description

### 2.1 User Classes and Characteristics
The system defines three distinct operational roles with strict Role-Based Access Control (RBAC):
1. **System Administrator (`Admin`):** Responsible for system user accounts, airport runway infrastructure creation, aircraft registration, and comprehensive system oversight.
2. **Air Traffic Controller (`ATC`):** Responsible for air traffic operations, flight scheduling, declaring/clearing emergency distress calls, queue dispatching, and runway resource allocation.
3. **Airport Ground Operator (`Airport Operator`):** Responsible for aircraft fleet maintenance logs, ground taxi monitoring, and runway physical maintenance flags.

---

## 3. Functional Requirements

### 3.1 User Authentication and Session Management
- **FR-AUTH-01:** The system shall require all users to authenticate with a unique username and password before accessing application functions.
- **FR-AUTH-02:** Passwords shall be cryptographically hashed using SHA-256 before verification or persistence. Passwords must never be transmitted or stored in plaintext.
- **FR-AUTH-03:** The system shall reject authentication attempts for non-existent users, incorrect passwords, or accounts flagged as `Inactive`.
- **FR-AUTH-04:** Successful login shall generate an active `Session` object holding user ID, username, full name, and assigned role.
- **FR-AUTH-05:** Logout shall immediately terminate the user session, purge role permissions from memory, and return the interface to the login view.
- **FR-AUTH-06:** The system shall provide a secure CLI account provisioning tool (`SmartATC --provision-user`) that accepts passwords via non-echoing console input only.

### 3.2 Role-Based Access Control (RBAC) Matrix
The system enforces RBAC at both the GUI presentation layer and the `ApplicationService` business logic layer:

| Permission Key | Description | Admin | ATC | Airport Operator |
|---|---|:---:|:---:|:---:|
| `page:Dashboard` | View operational dashboard & live KPIs | **Allowed** | **Allowed** | **Allowed** |
| `page:Flights` | Access flights table and flight history | **Allowed** | **Allowed** | **Allowed** |
| `page:Aircraft` | Access registered aircraft fleet table | **Allowed** | **Allowed** | **Allowed** |
| `page:Runways` | Access airport runways status table | **Allowed** | **Allowed** | **Allowed** |
| `page:Scheduling` | Access scheduling ready queue & aging | **Allowed** | **Allowed** | Denied |
| `page:Users` | Manage system user credentials and roles | **Allowed** | Denied | Denied |
| `action:ManageFlights` | Create, edit, or cancel flight schedules | Denied | **Allowed** | Denied |
| `action:DeclareEmergency` | Toggle emergency priority override | Denied | **Allowed** | Denied |
| `action:ManageAircraft` | Update aircraft operational/maintenance status | **Allowed** | Denied | **Allowed** |
| `action:AdministerAircraft` | Register new airframe or delete aircraft | **Allowed** | Denied | Denied |
| `action:ManageScheduling` | Enqueue process, dispatch, trigger aging | **Allowed** | **Allowed** | Denied |
| `action:RunwayAllocation` | Allocate or release physical runway resource | **Allowed** | **Allowed** | Denied |
| `action:RunwayMaintenance` | Mark runway in/out of maintenance state | **Allowed** | Denied | **Allowed** |
| `action:AdministerRunway` | Create or update runway physical attributes | **Allowed** | Denied | Denied |

### 3.3 Flight Management
- **FR-FLT-01:** The system shall support full CRUD operations on flight records (`flight_number`, `airline`, `aircraft_id`, `flight_type`, `scheduled_time`, `priority`, `source`, `destination`).
- **FR-FLT-02:** Flight types must be strictly constrained to `Arrival` or `Departure`.
- **FR-FLT-03:** Base priority must be an integer between 1 (highest) and 5 (lowest).
- **FR-FLT-04:** Operational status must progress according to the established lifecycle: `Scheduled` $\to$ `Taxiing` $\to$ `Cleared` $\to$ `Landed` (for Arrivals) or `Departed` (for Departures), or `Cancelled`.
- **FR-FLT-05:** Active flights (`Scheduled`, `Taxiing`, `Cleared`) cannot be deleted without prior cancellation.
- **FR-FLT-06:** An ATC controller may declare an emergency (`is_emergency = true`), elevating scheduling preference immediately.

### 3.4 Aircraft Management
- **FR-AIR-01:** The system shall register aircraft with unique tail registration numbers (e.g., `VT-XYZ`), model names, passenger/cargo capacity, and operational status (`Active`, `Maintenance`, `Retired`).
- **FR-AIR-02:** An aircraft assigned to one or more active flight records cannot be deleted (foreign key referential integrity).

### 3.5 Runway Infrastructure Management
- **FR-RWY-01:** The system shall manage physical runway assets with unique runway designations (e.g., `09L/27R`), operational type (`Landing`, `Takeoff`, `Both`), length, heading, and surface type.
- **FR-RWY-02:** Runway operational status shall be restricted to `Available`, `Occupied`, or `Maintenance`.
- **FR-RWY-03:** Only Airport Operators and Admins can toggle the `Maintenance` flag. Runways in maintenance must reject all flight allocation requests.

### 3.6 Simulated OS Process Model & PCB
- **FR-OS-01:** Each scheduled flight shall be represented in the scheduling subsystem by a simulated Process Control Block (`Models::Process`).
- **FR-OS-02:** The system shall enforce a 1:0..1 uniqueness constraint between a flight and an active scheduling queue entry.
- **FR-OS-03:** Each process shall track: PID (`flightId`), queue ID, arrival time, continuous ready duration (`readySince`), CPU burst time (`serviceTime`), base priority, dynamic priority (`effectivePriority`), and lifecycle state.
- **FR-OS-04:** The process lifecycle states shall be: `Ready`, `Running`, `Blocked`, and `Completed`.
- **FR-OS-05:** Invalid state transitions (e.g. `Completed` $\to$ `Ready` or `New` $\to$ `Running`) must be rejected.

### 3.7 Non-Preemptive Priority Scheduler
- **FR-SCH-01:** The scheduler shall implement Non-Preemptive Priority Scheduling.
- **FR-SCH-02:** The candidate process with the lowest numeric effective priority (1 is highest) shall be selected for dispatch.
- **FR-SCH-03:** Ties in effective priority shall be broken deterministically by earlier arrival time (FCFS). Remaining ties shall be broken by smaller flight ID.
- **FR-SCH-04:** Once dispatched (`Running`), a process cannot be preempted by a higher-priority arrival.
- **FR-SCH-05:** Upon process completion, the system shall compute:
  - Completion Time: $\text{CT} = \text{Start Time} + \text{Burst Time}$
  - Turnaround Time: $\text{TAT} = \text{CT} - \text{Arrival Time}$
  - Waiting Time: $\text{WT} = \text{TAT} - \text{Burst Time}$

### 3.8 Dynamic Emergency Priority & Priority Aging
- **FR-EMG-01:** When a flight declares an emergency, its corresponding process effective priority shall be set to 1.
- **FR-EMG-02:** When an emergency is cleared, effective priority shall be restored to $\max(1, \text{basePriority} - \text{agingLevel})$. Base priority must never be altered.
- **FR-AGE-01:** The aging engine shall evaluate all processes continuously in `Ready` state.
- **FR-AGE-02:** For every completed aging interval (configurable, default 30 seconds), effective priority shall improve by 1 level.
- **FR-AGE-03:** Priority aging shall never reduce effective priority below 1.
- **FR-AGE-04:** Aging evaluations must dynamically reorder the ready queue.

### 3.9 Runway Mutual Exclusion & Critical Sections
- **FR-MUT-01:** A physical runway is a non-shareable critical section. Exactly one flight may hold allocation rights at any moment.
- **FR-MUT-02:** Runway allocation in memory shall be protected by `std::mutex` and RAII locks (`ScopedResourceLock`).
- **FR-MUT-03:** Concurrent allocation requests for an occupied runway must fail immediately with `Resource Contention`.

### 3.10 ACID Database Transactions
- **FR-TX-01:** Runway allocation must execute as a single atomic transaction:
  1. Acquire row lock via `SELECT ... FOR UPDATE` on runway and flight.
  2. Validate eligibility and compatibility.
  3. Insert record into `runway_assignments`.
  4. Update runway status to `Occupied`.
  5. Update flight operational status to `Cleared`.
  6. Insert immutable audit log into `operation_logs`.
  7. `COMMIT` transaction.
- **FR-TX-02:** Any failure during steps 1–6 shall trigger an immediate `ROLLBACK`, leaving zero partial data.
- **FR-TX-03:** Runway release must execute as an atomic transaction restoring runway status to `Available` and setting assignment status to `Completed`.

### 3.11 Operational Dashboard & Monitoring
- **FR-DSH-01:** The dashboard shall present real-time KPIs: Total Flights, Scheduled Flights, Active Flights, Emergency Flights, Total Runways, Available Runways, Occupied Runways, Maintenance Runways, Runway Utilization %, Queue Depth, Running Count, and Completed Count.
- **FR-DSH-02:** Runway utilization percentage must safely handle zero total runways, returning `0.0%` without divide-by-zero errors.
- **FR-DSH-03:** The dashboard shall display active emergency flights in a prominent alert banner.
- **FR-DSH-04:** The dashboard shall update via a non-overlapping 5-second polling timer and event-driven signals.

---

## 4. Non-Functional Requirements

### 4.1 Reliability & Fault Resilience
- **NFR-REL-01:** The application must not crash or freeze if the MySQL server becomes disconnected.
- **NFR-REL-02:** In offline mode, the system shall surface clear status banners and continue executing in-memory scheduling simulations.

### 4.2 Data Consistency & Durability
- **NFR-DAT-01:** All relational tables must use the MySQL InnoDB storage engine to guarantee crash-safe ACID transactions.
- **NFR-DAT-02:** Foreign key relationships must enforce cascading deletes on operational child tables and restrict deletion on reference tables.

### 4.3 Security
- **NFR-SEC-01:** All database interactions must use parameterized SQL statements to eliminate SQL injection vulnerabilities.
- **NFR-SEC-02:** Database credentials must be stored externally in `db.conf` (excluded from version control via `.gitignore`).
- **NFR-SEC-03:** Account provisioning must read passwords interactively without echoing to the terminal.

### 4.4 Usability & UI Standards
- **NFR-USE-01:** The user interface must be clean, responsive, and provide high visual contrast with intuitive status colors (Green = Available/Normal, Blue = Active/Occupied, Amber = Warning/Scheduled, Red = Emergency/Maintenance).
- **NFR-USE-02:** Table views must support dynamic column sorting and real-time search filtering.

### 4.5 Performance
- **NFR-PERF-01:** In-memory priority scheduling selection and ready queue reordering must execute in under 5 milliseconds for up to 1,000 enqueued processes.
- **NFR-PERF-02:** Local MySQL transactional allocation cycles must complete in under 50 milliseconds under normal database load.

---

## 5. System Environment & Hardware Requirements

### 5.1 Hardware Requirements
- **Processor:** x86_64 dual-core processor, 2.0 GHz or faster.
- **Memory (RAM):** Minimum 4 GB RAM (8 GB recommended).
- **Storage:** Minimum 500 MB free disk space for application binaries, libraries, and MySQL database files.
- **Display Resolution:** Minimum 1280 $\times$ 720 pixels (optimized for 1080p and higher).

### 5.2 Software & Runtime Environment
- **Operating System:** Microsoft Windows 10 / Windows 11 (64-bit).
- **Compiler:** MinGW-w64 GCC 13.1.0 (64-bit).
- **GUI Framework:** Qt 6.11.2 (Core, Gui, Widgets, Sql, Test).
- **Database Server:** MySQL Server 8.0.x (verified on 8.0.46) with InnoDB support.
- **Build System:** CMake 3.19 or higher, Ninja build accelerator.
- **Database Driver:** Qt QMYSQL plugin (`qsqlmysql.dll`) with 64-bit `libmysql.dll`.
