# Smart Air Traffic Control Management System (SmartATC)

**Airport Runway Manager & Resource Allocation**  
*A comprehensive educational desktop simulation demonstrating Operating System and Database Management System concepts.*  
B.Tech Final Year Capstone Project

---

## 1. Project Overview

The **Smart Air Traffic Control Management System (SmartATC)** is an educational simulation developed in C++17 and Qt 6, integrated with a MySQL 8.0 InnoDB relational database.

The application models the core operations of an airport control tower—managing flights, scheduling arrivals/departures, allocating runways, and handling in-flight emergencies—as a direct real-world analogy to fundamental computing principles:
- **Operating Systems:** Process Control Blocks (PCB), Process Lifecycle States, Non-Preemptive Priority CPU Scheduling, Turnaround/Waiting Time Metrics, Starvation Prevention via Priority Aging, Critical Sections, Mutual Exclusion, and Multi-threaded Synchronization.
- **Database Management Systems:** Third Normal Form (3NF) Relational Modeling, Multi-Statement ACID Transactions, Pessimistic Row Locking (`SELECT ... FOR UPDATE`), Referential Integrity, and Audit Trails.

> [!IMPORTANT]
> **Educational Simulation Disclaimer:** This software is an academic simulation built strictly for demonstrating computer science concepts. It does not interface with actual radar systems, control live aircraft, or replace certified aviation infrastructure.

---

## 2. Technology Stack

| Component | Technology | Description |
|:---|:---|:---|
| **Programming Language** | **C++17** | ISO standard C++ utilizing modern memory management (RAII), `std::mutex`, `std::unique_lock`, and `std::thread`. |
| **Graphical Framework** | **Qt 6.11.2** | Native desktop UI utilizing Qt Widgets, Model-View architecture, and Signal-Slot mechanics. |
| **Relational Database** | **MySQL 8.0** | Enterprise database running InnoDB engine with strict foreign key constraints and ACID transaction support. |
| **Database Connector** | **Qt SQL (QMYSQL)** | Native Qt driver interfacing with 64-bit `libmysql` C client libraries. |
| **Build Automation** | **CMake 3.19+ & Ninja** | Fast cross-platform build orchestration and compilation. |
| **Testing Harness** | **Qt Test & CTest** | 19 automated test suites with over 360 unit, integration, and ACID verification assertions. |
| **Version Control** | **Git & GitHub** | Distributed source code management with strict branching and milestone tagging. |

---

## 3. Project Documentation Index

SmartATC includes complete, professional technical documentation, design specifications, user manuals, and architectural diagrams located in the [`docs/`](docs/) directory:

### Core Project & Academic Documentation
1. [**Project Overview**](docs/PROJECT_OVERVIEW.md) — Problem statement, computing analogy, educational goals, and system boundaries.
2. [**Requirements Specification (SRS)**](docs/REQUIREMENTS.md) — Functional requirements (FR-AUTH to FR-DSH), RBAC matrix, and non-functional requirements.
3. [**System Architecture**](docs/SYSTEM_ARCHITECTURE.md) — 4-tier layered architecture, module responsibilities, and fault tolerance policies.
4. [**Team Contributions**](docs/TEAM_CONTRIBUTIONS.md) — Detailed individual module ownership and deliverables for all 4 team members.
5. [**Installation & Setup Guide**](docs/INSTALLATION_AND_SETUP.md) — Windows environment configuration, MinGW/Qt toolchain, MySQL setup, and deployment.
6. [**User Manual & Operator Guide**](docs/USER_MANUAL.md) — Step-by-step graphical walkthrough for Admin, ATC, and Airport Operator roles.
7. [**Viva & Technical Defense Guide**](docs/VIVA_PREPARATION.md) — 25 in-depth technical viva questions and answers covering OS, DBMS, and C++ concepts.

### Systems & Technical Design Specifications
8. [**Operating System Concepts**](docs/OS_CONCEPTS.md) — Process models, scheduling formulas (CT, TAT, WT), aging algorithms, and mutex synchronization.
9. [**Scheduler Design & Implementation**](docs/SCHEDULER_DESIGN.md) — Priority comparison logic, tie-breaking rules, emergency override, and worked scheduling examples.
10. [**Runway Resource Allocation**](docs/RESOURCE_ALLOCATION.md) — Runway compatibility matrix, dual-layer state management, and critical section protection.
11. [**DBMS Design & Relational Modeling**](docs/DBMS_DESIGN.md) — Relational algebra, 1NF/2NF/3NF normalization analysis, indexing strategy, and row locking.
12. [**Database Schema Specification**](docs/DATABASE_SCHEMA.md) — Complete 8-table relational dictionary with data types, keys, and foreign constraints.
13. [**Transactions & ACID Verification**](docs/TRANSACTION_AND_ACID.md) — Multi-statement atomic transactions, failure injection tests, and durability verification.
14. [**Module Design Specification**](docs/MODULE_DESIGN.md) — Architectural specifications for all 6 functional subsystems.
15. [**GUI & Presentation Layer**](docs/GUI_DOCUMENTATION.md) — Screen catalog, widgets, visual indicators, and live window capture instructions.
16. [**Testing & Quality Assurance Report**](docs/TESTING_REPORT.md) — Test environment, 19 CTest test suites summary, and Day 25 defect resolutions.
17. [**Security & Integrity Considerations**](docs/SECURITY_CONSIDERATIONS.md) — SHA-256 hashing, SQL injection defense, credential isolation, and audit trails.

---

## 4. Architecture Diagrams (Mermaid)

All architectural and workflow diagrams are maintained in [`docs/diagrams/`](docs/diagrams/):

| Diagram Title | Source File | Description |
|:---|:---|:---|
| **System Architecture** | [`docs/diagrams/system_architecture.mmd`](docs/diagrams/system_architecture.mmd) | Layered architecture from Qt GUI to MySQL InnoDB. |
| **Use Case Diagram** | [`docs/diagrams/use_case_diagram.mmd`](docs/diagrams/use_case_diagram.mmd) | Operational permissions for Admin, ATC, and Airport Operator. |
| **Core Class Diagram** | [`docs/diagrams/class_diagram.mmd`](docs/diagrams/class_diagram.mmd) | Domain models, scheduler, aging engine, resource manager, and DAO classes. |
| **Activity Diagram** | [`docs/diagrams/activity_diagram.mmd`](docs/diagrams/activity_diagram.mmd) | End-to-end `scheduleAndAllocateNext()` operational workflow. |
| **Sequence Diagram** | [`docs/diagrams/sequence_diagram.mmd`](docs/diagrams/sequence_diagram.mmd) | Chronological call sequence across UI, Service, Mutex, and Database. |
| **Entity-Relationship Diagram** | [`docs/diagrams/er_diagram.mmd`](docs/diagrams/er_diagram.mmd) | 8-table relational database schema with PK/FK cardinality. |
| **Scheduling Flowchart** | [`docs/diagrams/scheduling_flowchart.mmd`](docs/diagrams/scheduling_flowchart.mmd) | Non-preemptive priority decision logic and aging engine loop. |
| **Runway Allocation Flowchart** | [`docs/diagrams/runway_allocation_flowchart.mmd`](docs/diagrams/runway_allocation_flowchart.mmd) | Infrastructure compatibility evaluation and contention resolution. |
| **Transaction Flowchart** | [`docs/diagrams/transaction_flowchart.mmd`](docs/diagrams/transaction_flowchart.mmd) | Multi-statement database transaction and rollback execution path. |
| **Process State Machine** | [`docs/diagrams/state_machine.mmd`](docs/diagrams/state_machine.mmd) | Process transitions across `READY`, `RUNNING`, `BLOCKED`, and `COMPLETED`. |

---

## 5. Quick Start: Build and Run

### 5.1 Prerequisites
- Windows 10/11 (64-bit)
- Qt 6.11.2 (MinGW 64-bit) & MinGW-w64 GCC 13.1.0
- CMake 3.19+ & Ninja
- MySQL Server 8.0+

### 5.2 Database Setup
```sql
CREATE DATABASE smartatc CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'smartatc_user'@'localhost' IDENTIFIED BY 'smartatc_pass';
GRANT ALL PRIVILEGES ON smartatc.* TO 'smartatc_user'@'localhost';
FLUSH PRIVILEGES;
```
Import database schema and seed data:
```powershell
cd database
mysql -u smartatc_user -psmartatc_pass smartatc < schema.sql
mysql -u smartatc_user -psmartatc_pass smartatc < seed_data.sql
mysql -u smartatc_user -psmartatc_pass smartatc < migrations/migration_day14_runways.sql
mysql -u smartatc_user -psmartatc_pass smartatc < migrations/migration_day16_emergency.sql
mysql -u smartatc_user -psmartatc_pass smartatc < migrations/migration_day17_aging.sql
```

### 5.3 Configure Credentials (`db.conf`)
Create `db.conf` in the project root:
```ini
[database]
host=localhost
port=3306
dbname=smartatc
user=smartatc_user
password=smartatc_pass
driver=QMYSQL
```

### 5.4 Build with CMake & Ninja
```powershell
# Configure build
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.11.2/mingw_64"

# Compile project
cmake --build build --config Release

# Deploy runtime DLLs
powershell -ExecutionPolicy Bypass -File .\scripts\deploy_runtime.ps1
```

### 5.5 Run Automated Verification Tests
```powershell
cd build
ctest --output-on-failure
```
*Expected Result: 19/19 Test Suites Passed (100% pass rate).*

### 5.6 Launch the GUI Application
```powershell
.\build\bin\smartatc_gui.exe
```
**Default Credentials:**
- `admin` / `admin123` (Administrator)
- `atc_operator` / `atc123` (Air Traffic Controller)
- `runway_ops` / `ops123` (Airport Operator)

---

## 6. Team Structure & Module Ownership

| Member | Primary Module Responsibility | Core Domain |
|:---|:---|:---|
| **Member 1** | User & Role Management, Authentication, Session Security | Security, Auth, RBAC, Audit Logging |
| **Member 2** | Flight & Aircraft Management, Fleet Inventory, Schedule Validation | Domain Modeling, Fleet Tracking |
| **Member 3** | Process & Priority Scheduling, Ready Queue, Priority Aging | Operating System Simulation, Starvation |
| **Member 4** | Runway Allocation, Mutual Exclusion, Threads, ACID Transactions | Concurrency, `std::mutex`, MySQL Transactions |

---

## 7. License and Academic Notice

This project was developed as a B.Tech final-year capstone project. It is licensed for academic, educational, and non-commercial research use. All simulation parameters, scheduling algorithms, and database models are open for inspection and extension.
