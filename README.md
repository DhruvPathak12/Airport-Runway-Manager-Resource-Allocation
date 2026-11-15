# Smart Air Traffic Control Management System (SmartATC)

**Airport Runway Manager & Resource Allocation**  
*A comprehensive educational desktop simulation demonstrating Operating System and Database Management System concepts.*  
B.Tech Final Year Capstone Project

---

## 1. Project Overview

The **Smart Air Traffic Control Management System (SmartATC)** is an educational desktop simulation developed in C++17 and Qt 6, integrated with a MySQL 8.0 InnoDB relational database.

The application models the core operations of an airport control center—managing flights, scheduling arrivals/departures, allocating runways, and handling in-flight emergencies—as a direct real-world analogy to fundamental computing principles:
- **Operating Systems:** Process Control Blocks (PCB), Process Lifecycle States, Non-Preemptive Priority Scheduling, Turnaround/Waiting Time Metrics, Starvation Prevention via Priority Aging, Critical Sections, Mutual Exclusion, and Multi-threaded Synchronization.
- **Database Management Systems:** Third Normal Form (3NF) Relational Modeling, Multi-Statement ACID Transactions, Pessimistic Row Locking (`SELECT ... FOR UPDATE`), Referential Integrity, and Audit Trails.

> [!IMPORTANT]
> **Educational Simulation Disclaimer:** This software is an academic simulation running strictly in user space on the host OS. It is designed for demonstrating computer science concepts. It does not interface with actual radar systems, control live aircraft, or replace certified aviation infrastructure.

---

## 2. Conceptual Operating System Mappings

| Airport Domain Concept | Operating System Concept | SmartATC Simulation Implementation |
|:---|:---|:---|
| **Flight** | **Simulated Process** | `Models::Process` representing a schedulable job with PID (`flightId`). |
| **Flight Waiting Queue** | **Ready Queue** | `SchedulerComponents::ReadyQueue` holding runnable tasks. |
| **Flight Priority** | **Process Priority** | Priority scale from 1 (Highest) to 5 (Lowest). |
| **Emergency Flight** | **High-Priority Process** | Dynamic escalation to effective Priority 1 override. |
| **Runway** | **Shared OS Resource** | Critical section protected by `std::mutex` and database row locks. |
| **Runway Assignment** | **Resource Allocation** | Allocation of an exclusive shared resource to a process. |
| **Long Waiting** | **Starvation Risk** | Indefinite waiting of low-priority processes under high traffic. |
| **Priority Aging** | **Starvation Prevention** | Dynamic priority improvement over time via `AgingEngine`. |

> [!NOTE]
> **Resource Allocation vs. CPU:**  
> A runway is a **shared OS resource** subject to mutual exclusion, not a CPU core. Simulated flight processes hold exclusive access to an allocated runway resource for a defined operational service duration (`serviceTime`) before releasing it.

---

## 3. Technology Stack

| Component | Technology | Description |
|:---|:---|:---|
| **Programming Language** | **C++17** | ISO standard C++ utilizing modern memory management (RAII), `std::mutex`, `std::unique_lock`, and `std::thread`. |
| **Graphical Framework** | **Qt 6.11.2** | Native desktop UI utilizing Qt Widgets, Model-View architecture, and Signal-Slot mechanics. |
| **Relational Database** | **MySQL 8.0+** | Enterprise database running InnoDB engine with strict foreign key constraints and ACID transaction support. |
| **Database Connector** | **Qt SQL (QMYSQL)** | Native Qt driver interfacing with 64-bit `libmysql` C client libraries. |
| **Build Automation** | **CMake 3.19+ & Ninja** | Fast cross-platform build orchestration and compilation. |
| **Testing Harness** | **Qt Test & CTest** | 19 automated test suites with over 360 unit, integration, and ACID verification assertions. |
| **Version Control** | **Git & GitHub** | Distributed source code management with strict branching and milestone tagging. |

---

## 4. Project Documentation Index

SmartATC includes complete, professional technical documentation, design specifications, user manuals, and architectural diagrams located in the [`docs/`](docs/) directory:

### Core Project & Academic Documentation
1. [**Project Overview**](docs/PROJECT_OVERVIEW.md) — Problem statement, computing analogy, educational goals, and system boundaries.
2. [**Requirements Specification (SRS)**](docs/REQUIREMENTS.md) — Functional requirements (FR-AUTH to FR-DSH), RBAC matrix, and non-functional requirements.
3. [**System Architecture**](docs/SYSTEM_ARCHITECTURE.md) — 4-tier layered architecture, module responsibilities, and fault tolerance policies.
4. [**Team Contributions**](docs/TEAM_CONTRIBUTIONS.md) — Detailed individual module ownership and deliverables for all 4 team members.
5. [**Installation & Setup Guide**](docs/INSTALLATION_AND_SETUP.md) — Windows environment configuration, MinGW/Qt toolchain, MySQL setup, and deployment.
6. [**User Manual & Operator Guide**](docs/USER_MANUAL.md) — Step-by-step graphical walkthrough for Admin, ATC, and Airport Operator roles.
7. [**Viva & Technical Defense Guide**](docs/VIVA_PREPARATION.md) — 25 in-depth technical viva questions and detailed answers covering OS, DBMS, and C++ concepts.

### Final Demonstration & Evaluation Package (B.Tech Viva)
8. [**Demonstration Master Guide**](docs/demo/DEMONSTRATION_GUIDE.md) — Master runbook, timing plan, navigation index, and viva contingency matrix.
9. [**Demonstration Script & Viva Presentation**](docs/demo/DEMONSTRATION_SCRIPT.md) — Step-by-step 8-phase oral presentation script (18–20 minutes) with spoken text and cues.
10. [**Demonstration Data Catalog**](docs/demo/DEMONSTRATION_DATA.md) — Concrete BOM scenario data: credentials, fleet, runways, and baseline PCB queue.
11. [**OS Concepts Demonstration Guide**](docs/demo/OS_CONCEPT_DEMO.md) — Worked examples and verification of PCB, Priority Scheduling, Aging, and Mutex contention.
12. [**DBMS Transaction Demonstration Guide**](docs/demo/DBMS_TRANSACTION_DEMO.md) — Real MySQL transactions, pessimistic row locking (`FOR UPDATE`), atomic commit, and rollback proof.
13. [**Demonstration Reset Guide**](docs/demo/DEMO_RESET_GUIDE.md) — Non-destructive, idempotent reset instructions and SQL scripts.
14. [**Demonstration Verification Checklist**](docs/demo/DEMONSTRATION_CHECKLIST.md) — Comprehensive 38-point pre-flight and live presentation verification audit.

### Systems & Technical Design Specifications
15. [**Operating System Concepts**](docs/OS_CONCEPTS.md) — Process models, scheduling formulas (CT, TAT, WT), aging algorithms, and mutex synchronization.
16. [**Scheduler Design & Implementation**](docs/SCHEDULER_DESIGN.md) — Priority comparison logic, tie-breaking rules, emergency override, and worked scheduling examples.
17. [**Runway Resource Allocation**](docs/RESOURCE_ALLOCATION.md) — Runway compatibility matrix, dual-layer state management, and critical section protection.
18. [**DBMS Design & Relational Modeling**](docs/DBMS_DESIGN.md) — Relational algebra, 1NF/2NF/3NF normalization analysis, indexing strategy, and row locking.
19. [**Database Schema Specification**](docs/DATABASE_SCHEMA.md) — Complete 8-table relational dictionary with data types, keys, and foreign constraints.
20. [**Transactions & ACID Verification**](docs/TRANSACTION_AND_ACID.md) — Multi-statement atomic transactions, failure injection tests, and durability verification.
21. [**Module Design Specification**](docs/MODULE_DESIGN.md) — Architectural specifications for all 6 functional subsystems.
22. [**GUI & Presentation Layer**](docs/GUI_DOCUMENTATION.md) — Screen catalog, widgets, visual indicators, and live window capture instructions.
23. [**Testing & Quality Assurance Report**](docs/TESTING_REPORT.md) — Test environment, 19 CTest test suites summary, and Day 25 defect resolutions.
24. [**Security & Integrity Considerations**](docs/SECURITY_CONSIDERATIONS.md) — SHA-256 hashing, SQL injection defense, credential isolation, and audit trails.

---

## 5. Architecture Diagrams (Mermaid)

All architectural and workflow diagrams are maintained in [`docs/diagrams/`](docs/diagrams/):

| Diagram Title | Source File | Description |
|:---|:---|:---|
| **System Architecture** | [`docs/diagrams/system_architecture.mmd`](docs/diagrams/system_architecture.mmd) | Layered architecture from Qt GUI to MySQL InnoDB. |
| **Use Case Diagram** | [`docs/diagrams/use_case_diagram.mmd`](docs/diagrams/use_case_diagram.mmd) | Operational permissions for Admin, ATC, and Airport Operator. |
| **Core Class Diagram** | [`docs/diagrams/class_diagram.mmd`](docs/diagrams/class_diagram.mmd) | Domain models, scheduler, aging engine, resource manager, and database manager. |
| **Activity Diagram** | [`docs/diagrams/activity_diagram.mmd`](docs/diagrams/activity_diagram.mmd) | End-to-end `scheduleAndAllocateNext()` operational workflow. |
| **Sequence Diagram** | [`docs/diagrams/sequence_diagram.mmd`](docs/diagrams/sequence_diagram.mmd) | Chronological call sequence across UI, Service, Mutex, and Database. |
| **Entity-Relationship Diagram** | [`docs/diagrams/er_diagram.mmd`](docs/diagrams/er_diagram.mmd) | 8-table relational database schema with PK/FK cardinality. |
| **Scheduling Flowchart** | [`docs/diagrams/scheduling_flowchart.mmd`](docs/diagrams/scheduling_flowchart.mmd) | Non-preemptive priority decision logic and aging engine loop. |
| **Runway Allocation Flowchart** | [`docs/diagrams/runway_allocation_flowchart.mmd`](docs/diagrams/runway_allocation_flowchart.mmd) | Infrastructure compatibility evaluation and contention resolution. |
| **Transaction Flowchart** | [`docs/diagrams/transaction_flowchart.mmd`](docs/diagrams/transaction_flowchart.mmd) | Multi-statement database transaction and rollback execution path. |
| **Process State Machine** | [`docs/diagrams/state_machine.mmd`](docs/diagrams/state_machine.mmd) | Process transitions across `Ready`, `Running`, `Blocked`, and `Completed`. |

---

## 6. Quick Start: Build and Run

### 6.1 Prerequisites
- Windows 10/11 (64-bit)
- Qt 6.11.2 (MinGW 64-bit) & MinGW-w64 GCC 13.1.0
- CMake 3.19+ & Ninja
- MySQL Server 8.0+

### 6.2 Database Setup
Create database and application user:
```sql
CREATE DATABASE smartatc CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'smartatc_user'@'localhost' IDENTIFIED BY '<YOUR_DB_PASSWORD>';
GRANT ALL PRIVILEGES ON smartatc.* TO 'smartatc_user'@'localhost';
FLUSH PRIVILEGES;
```
Import database schema and seed data from `sql/`:
```powershell
cd sql
mysql -u smartatc_user -p smartatc < schema.sql
mysql -u smartatc_user -p smartatc < seed.sql
mysql -u smartatc_user -p smartatc < migration_day17_aging.sql
```

### 6.3 Configure Credentials (`db.conf`)
Create `db.conf` in the project root (`SmartATC/db.conf`) using the template below.
> **Note:** `db.conf` is gitignored to prevent committing database secrets.
```ini
[database]
host=localhost
port=3306
dbname=smartatc
user=smartatc_user
password=<YOUR_DB_PASSWORD>
driver=QMYSQL
```

### 6.4 Build with CMake & Ninja
```powershell
# Configure build
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.11.2/mingw_64"

# Compile project
cmake --build build --config Release

# Deploy runtime DLLs
powershell -ExecutionPolicy Bypass -File .\scripts\deploy_runtime.ps1
```

### 6.5 Run Automated Verification Tests
```powershell
cd build
ctest --output-on-failure
```
*Expected Result: 19/19 Test Suites Passed (100% pass rate).*

### 6.6 Launch the GUI Application
```powershell
.\build\bin\smartatc_gui.exe
```

**Demonstration User Accounts:**
- `admin` — Role: Administrator
- `atc_operator` — Role: Air Traffic Controller
- `runway_ops` — Role: Airport Operator

> [!WARNING]
> **Password Security Notice:**  
> The academic project uses SHA-256 password hashing. In production deployments, salted key derivation algorithms such as **Argon2id** or **bcrypt** must be utilized.

---

## 7. Team Structure & Module Ownership

| Member | Primary Module Responsibility | Core Technical Domain |
|:---|:---|:---|
| **Member 1** | User & Role Management, Authentication, Session Security | Security, Auth, RBAC, Audit Logging |
| **Member 2** | Flight & Aircraft Management, Fleet Inventory, Schedule Validation | Domain Modeling, Fleet Tracking |
| **Member 3** | Process & Priority Scheduling, Ready Queue, Priority Aging | Operating System Simulation, Starvation |
| **Member 4** | Runway Allocation, Mutual Exclusion, Threads, ACID Transactions | Concurrency, `std::mutex`, MySQL Transactions |

---

## 8. License and Academic Notice

This project was developed as a B.Tech final-year capstone project. It is licensed for academic, educational, and non-commercial research use. All simulation parameters, scheduling algorithms, and database models are open for inspection and extension.
