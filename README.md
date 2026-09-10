# Smart Air Traffic Control Management System

**Airport Runway Manager & Resource Allocation**
B.Tech Final Year Project — OS & DBMS Concepts Demonstration

---

## About

This is an **educational desktop application** that simulates Air Traffic Control
operations to demonstrate core Operating System and Database Management System concepts.

It is **not a real-world ATC system**. It does not communicate with actual aircraft,
provide aviation certification, or replace licensed air traffic control systems.

### OS Concepts Demonstrated
- Non-Preemptive Priority Scheduling
- Process States and Ready Queues (PCB model)
- Aging and Starvation Prevention
- Mutual Exclusion and Critical Sections (Runway as shared resource)
- Thread Synchronization (C++ std::mutex / std::thread)

### DBMS Concepts Demonstrated
- Relational Schema Design (8-table normalized database)
- 1NF / 2NF / 3NF Normalization
- ACID Transactions (BEGIN / COMMIT / ROLLBACK)
- Isolation Level Demonstration (SELECT ... FOR UPDATE)
- Referential Integrity (Foreign Keys, Constraints)

---

## Technology Stack

| Component       | Technology                    |
|-----------------|-------------------------------|
| Language        | C++17                         |
| GUI             | Qt 6 (Widgets)                |
| Database        | MySQL 8.x                     |
| DB Connectivity | Qt SQL — QMYSQL driver        |
| Build System    | CMake 3.19+                   |
| Version Control | Git + GitHub                  |

---

## Prerequisites

- **Qt 6.x** — Widgets module + SQL module (with QMYSQL driver)
- **CMake 3.19** or later
- **MySQL 8.x** running on localhost
- **C++17-compatible compiler** (MinGW-w64 / MSVC / GCC / Clang)
- **Ninja** (optional, speeds up build)

---

## Build Instructions

### 1. Clone the repository

```bash
git clone https://github.com/YOUR_USERNAME/Airport-Runway-Manager-Resource-Allocation.git
cd SmartATC
```

### 2. Set up database credentials

```bash
# Copy the template and fill in your MySQL credentials
cp db.conf.example db.conf
# Edit db.conf and replace the placeholder values
# IMPORTANT: db.conf is gitignored — never commit it
```

### 3. Configure with CMake

```bash
# Using Qt's bundled CMake and MinGW (Windows example):
cmake -B build -S . \
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.2/mingw_64" \
  -DCMAKE_C_COMPILER="C:/Qt/Tools/mingw1310_64/bin/gcc.exe" \
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe" \
  -G Ninja
```

### 4. Build

```bash
cmake --build build
```

### 5. Run

```bash
# Windows
build\SmartATC.exe
```

---

## Database Setup (Available from Day 3)

```sql
-- Create the database (run once)
CREATE DATABASE airport_runway_db
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;
```

```bash
-- Apply schema and seed data
mysql -u YOUR_USER -p airport_runway_db < sql/schema.sql
mysql -u YOUR_USER -p airport_runway_db < sql/seed.sql
```

---

## Project Structure

```
SmartATC/
├── CMakeLists.txt
├── db.conf.example          ← Credential template (committed)
├── db.conf                  ← Your local credentials (gitignored)
├── src/
│   ├── main.cpp
│   ├── models/              ← C++ domain structs (Flight, Process, Runway…)
│   ├── scheduler/           ← Priority scheduler, aging engine
│   ├── resource/            ← Runway resource manager (mutex / critical section)
│   ├── database/            ← DatabaseManager, data access
│   ├── services/            ← Business logic services
│   └── utils/               ← Shared utilities (PasswordUtils, Session…)
├── ui/                      ← Qt 6 screens and widgets
├── sql/
│   ├── schema.sql           ← Full 8-table DDL
│   ├── seed.sql             ← Sample data
│   └── transactions.sql     ← Transaction demonstration scripts
├── docs/                    ← Documentation, ER diagrams, DFDs
└── tests/                   ← Unit and integration tests
```

---

## Team Members

| Member   | Module                                    |
|----------|-------------------------------------------|
| Member 1 | User & Role Management (Auth, Session)    |
| Member 2 | Flight & Aircraft Management              |
| Member 3 | Process & Scheduling (Scheduler, Aging)   |
| Member 4 | Runway Resource Management + Transactions |

---

## Development Phases

**Phase 1 (Days 1–13):** Project foundation, MySQL schema, C++ ↔ MySQL connectivity,
login & roles, flight & aircraft management, process model, manual priority scheduler.

**Phase 2 (Days 14–29):** Runway resource management, aging, starvation prevention,
critical section, ACID transactions, full OS/DBMS integration, dashboard, testing,
documentation, final demonstration, viva preparation.

---

> **Disclaimer**: This system is purely for educational purposes as part of a
> B.Tech academic project. It simulates ATC concepts to demonstrate OS and DBMS
> principles. It is not intended for, nor suitable for, any real aviation use.
