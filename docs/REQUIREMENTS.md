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
1. **System Administrator (`Admin`):** Responsible for user account administration, fleet management, and system-wide monitoring.
2. **Air Traffic Controller (`ATC`):** Responsible for flight management, emergency declarations, process queue dispatching, and runway resource allocation.
3. **Airport Operator (`Airport Operator`):** Responsible for aircraft fleet maintenance logs, ground resource monitoring, and runway physical maintenance flags.

---

## 3. Functional Requirements

### 3.1 User Authentication and Session Management
- **FR-AUTH-01:** The system shall require all users to authenticate with a unique username and password before accessing application functions.
- **FR-AUTH-02:** Passwords shall be cryptographically hashed using SHA-256 before verification or persistence. Passwords must never be transmitted or stored in plaintext.
- **FR-AUTH-03:** The system shall reject authentication attempts for non-existent users, incorrect passwords, or accounts flagged as `Inactive`.
- **FR-AUTH-04:** Successful login shall generate an active `Session` object holding user ID, username, full name, and assigned role.
- **FR-AUTH-05:** Logout shall immediately terminate the user session, purge role permissions from memory, and return the interface to the login view.
- **FR-AUTH-06:** The system shall provide a secure CLI account provisioning tool (`manage_users.exe`) that accepts passwords via non-echoing console input only.

### 3.2 Role-Based Access Control (RBAC) Matrix
The system enforces RBAC at the `ApplicationService` business logic layer via `Session::hasPermission()`:

| Permission Key | Description | Admin | ATC | Airport Operator |
|:---|:---|:---:|:---:|:---:|
| `page:Dashboard` | View operational dashboard & live KPIs | **Allowed** | **Allowed** | **Allowed** |
| `page:Flights` | Access flights table and flight history | **Allowed** | **Allowed** | **Allowed** |
| `page:Aircraft` | Access registered aircraft fleet table | **Allowed** | **Allowed** | **Allowed** |
| `page:Runways` | Access airport runways status table | **Allowed** | **Allowed** | **Allowed** |
| `page:Scheduling` | Access scheduling ready queue & aging | **Allowed** | **Allowed** | Denied |
| `page:Users` | Manage system user credentials and roles | **Allowed** | Denied | Denied |
| `action:ManageFlights` | Create, edit, or cancel flight schedules | Denied | **Allowed** | Denied |
| `action:DeclareEmergency` | Toggle emergency priority override | Denied | **Allowed** | Denied |
| `action:ManageAircraft` | Update aircraft operational status | **Allowed** | Denied | **Allowed** |
| `action:AdministerAircraft` | Register new airframe | **Allowed** | Denied | Denied |
| `action:ManageScheduling` | Enqueue process, dispatch, trigger aging | **Allowed** | **Allowed** | Denied |
| `action:RunwayAllocation` | Allocate physical runway resource to flight | Denied | **Allowed** | Denied |
| `action:RunwayMaintenance` | Mark runway in/out of maintenance state | Denied | Denied | **Allowed** |

*(Note: Runway release is permitted for ATC, Airport Operator, and Admin in `ApplicationService::releaseRunwayResource`).*

### 3.3 Flight Management
- **FR-FLT-01:** The system shall support full CRUD operations on flight records (`flight_number`, `airline`, `aircraft_id`, `flight_type`, `scheduled_time`, `priority`, `source`, `destination`).
- **FR-FLT-02:** Flight types must be strictly constrained to `Arrival` or `Departure`.
- **FR-FLT-03:** Base priority must be an integer between 1 (highest priority) and 5 (lowest priority).
- **FR-FLT-04:** Operational status must progress according to the established lifecycle: `Scheduled` $\to$ `Taxiing` $\to$ `Cleared` $\to$ `Landed` (for Arrivals) or `Departed` (for Departures), or `Cancelled`.
- **FR-FLT-05:** Active flights (`Scheduled`, `Taxiing`, `Cleared`) cannot be deleted without prior cancellation.
- **FR-FLT-06:** An ATC controller may declare an emergency (`is_emergency = true`), elevating scheduling preference immediately.

### 3.4 Aircraft Management
- **FR-AIR-01:** The system shall register aircraft with unique tail registration numbers (e.g., `VT-ANB`), aircraft type, passenger capacity, and operational status (`Active`, `Maintenance`, `Retired`).
- **FR-AIR-02:** An aircraft assigned to one or more active flight records cannot be deleted (foreign key referential integrity).

### 3.5 Runway Infrastructure Management
- **FR-RWY-01:** The system shall manage physical runway assets with unique runway designations (e.g., `09L`, `27R`), operational type (`Landing`, `Takeoff`, `Both`), and operational status (`Available`, `Occupied`, `Maintenance`).
- **FR-RWY-02:** Only Airport Operators can toggle the `Maintenance` flag. Runways in maintenance must reject all flight allocation requests.

### 3.6 Simulated OS Process Model & PCB
- **FR-OS-01:** Each scheduled flight shall be represented in the scheduling subsystem by a simulated Process Control Block (`Models::Process`).
- **FR-OS-02:** The system shall enforce a 1:0..1 uniqueness constraint between a flight and an active scheduling queue entry.
- **FR-OS-03:** Each process shall track: PID (`flightId`), queue ID, arrival time, continuous ready duration (`readySince`), simulated service duration (`serviceTime`), base priority, dynamic priority (`effectivePriority`), and lifecycle state.
- **FR-OS-04:** The process lifecycle states shall be: `Ready`, `Running`, `Blocked`, and `Completed`.
- **FR-OS-05:** Invalid state transitions (e.g. `Completed` $\to$ `Ready`) must be rejected.

### 3.7 Non-Preemptive Priority Scheduler
- **FR-SCH-01:** The scheduler shall implement Non-Preemptive Priority Scheduling.
- **FR-SCH-02:** The candidate process with the lowest numeric effective priority (1 is highest priority, 5 is lowest) shall be selected for dispatch.
- **FR-SCH-03:** Ties in effective priority shall be broken deterministically by earlier arrival time (FCFS). Remaining ties shall be broken by smaller flight ID.
- **FR-SCH-04:** Once dispatched (`Running`), a process cannot be preempted by a higher-priority arrival.
- **FR-SCH-05:** Upon process completion, the system shall compute:
  - Completion Time: $\text{CT} = \text{Start Time} + \text{Service Time}$
  - Turnaround Time: $\text{TAT} = \text{CT} - \text{Arrival Time}$
  - Waiting Time: $\text{WT} = \text{TAT} - \text{Service Time}$

### 3.8 Dynamic Emergency Priority & Priority Aging
- **FR-EMG-01:** When a flight declares an emergency, its corresponding process effective priority shall be set to 1 (highest priority).
- **FR-EMG-02:** When an emergency is cleared, effective priority shall be restored to $\max(1, \text{basePriority} - \text{agingLevel})$. Base priority must never be altered.
- **FR-AGE-01:** The aging engine shall evaluate all processes continuously in `Ready` state.
- **FR-AGE-02:** For every completed aging interval (configurable, default 30 seconds), effective priority shall improve by decrementing its numerical value by 1.
- **FR-AGE-03:** Priority aging shall never reduce effective priority below 1.
- **FR-AGE-04:** Aging evaluations must dynamically reorder the ready queue.

### 3.9 Runway Mutual Exclusion & Critical Sections
- **FR-MUT-01:** A physical runway is a non-shareable critical section. Exactly one flight may hold allocation rights at any moment.
- **FR-MUT-02:** Runway allocation in memory shall be protected by `std::mutex` and RAII locks (`ScopedResourceLock`).
- **FR-MUT-03:** Concurrent allocation requests for an occupied runway must fail immediately with conflict rejection.

### 3.10 ACID Database Transactions
- **FR-TX-01:** Runway allocation shall execute within an atomic MySQL transaction encompassing row-level locks (`SELECT ... FOR UPDATE`), `runway_assignments` insertion, `runways` status update, `flights` operational status update, and `operation_logs` insertion.
- **FR-TX-02:** Runway release shall execute within an atomic transaction updating `runway_assignments`, resetting `runways.status` to `Available`, and recording an audit log.
- **FR-TX-03:** If any statement within an allocation or release fails, the system must execute `ROLLBACK` and reconcile in-memory states.
- **FR-TX-04:** Committed transactions must survive service termination and database connection re-establishment (Durability).

### 3.11 Operational Dashboard & Monitoring
- **FR-DSH-01:** The dashboard shall present real-time KPIs: Total Flights, Scheduled Flights, Active Flights, Emergency Flights, Total Runways, Available Runways, Occupied Runways, Maintenance Runways, Runway Utilization %, Queue Depth, Running Count, and Completed Count.
- **FR-DSH-02:** Runway utilization calculation shall be mathematically safe against zero-runway division.
- **FR-DSH-03:** The dashboard shall display active emergency flights in a prominent alert banner.
- **FR-DSH-04:** The dashboard shall display the 10 most recent persistent database audit logs.

---

## 4. Non-Functional Requirements (NFR)

- **NFR-REL-01 (Reliability):** In the absence of a MySQL database connection, the application must gracefully degrade to in-memory simulation mode without crashes.
- **NFR-CON-01 (Consistency):** Concurrently executing threads shall never produce duplicate active runway assignments.
- **NFR-SEC-01 (Security):** All database queries must use prepared statements with parameterized binding.
- **NFR-PERF-01 (Performance):** In-memory priority scheduling candidate selection must execute in under 5 milliseconds.
