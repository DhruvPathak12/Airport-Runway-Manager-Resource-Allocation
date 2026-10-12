# ARCHITECTURE.md — Smart Air Traffic Control Management System

## Overview

This document describes the technical architecture of the Smart ATC system
established on Day 2. It is the reference for all future implementation days.

---

## 1. Overall Architecture

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  GUI LAYER — Qt 6 Widgets                                                     │
│  ui/MainWindow + placeholder pages → functional modules (Day 6–14)            │
└─────────────────────────────────────┬────────────────────────────────────────┘
                                      │
┌─────────────────────────────────────▼────────────────────────────────────────┐
│  SERVICE LAYER — src/services/ApplicationService                               │
│  Coordinator between UI and all sub-systems.                                   │
│  GUI never calls the DB or scheduler directly.                                 │
└──────────────────┬──────────────────────────────────┬─────────────────────────┘
                   │                                  │
┌──────────────────▼──────────────┐   ┌──────────────▼───────────────────────── ┐
│  OS SIMULATION LAYER             │   │  DBMS SERVICE LAYER                      │
│  src/scheduler/Scheduler         │   │  src/database/DatabaseManager            │
│  src/resource/ResourceManager    │   │  (future: repository classes)            │
│                                  │   │                                          │
│  Non-Preemptive Priority Sched.  │   │  Qt SQL / QMYSQL driver                  │
│  Aging (Phase 2)                 │   │  MySQL 8.x — airport_runway_db           │
│  Mutex / Critical Section (Ph2)  │   │  ACID Transactions (Phase 2)             │
└──────────────────────────────────┘   └──────────────────────────────────────── ┘
```

---

## 2. Directory Responsibilities

| Directory | Responsibility |
|---|---|
| `src/models/` | Plain C++17 structs: Flight, Aircraft, Runway, User |
| `src/scheduler/` | Priority scheduler + future AgingEngine |
| `src/resource/` | Runway resource manager + future mutex/transaction logic |
| `src/database/` | DatabaseManager singleton — Qt SQL connection + future repositories |
| `src/services/` | ApplicationService — coordinator, future AuthService/FlightService etc. |
| `src/utils/` | PasswordUtils (SHA-256), future Session, shared helpers |
| `ui/` | MainWindow + all Qt UI widgets/dialogs |
| `sql/` | MySQL DDL (schema.sql), seed data (seed.sql), transactions (transactions.sql) |
| `docs/` | Architecture, ER diagrams, DFDs, project reports |
| `tests/` | Unit and integration tests |

**FROZEN — Do not introduce:** `core/`, `repositories/`, `controllers/`

---

## 3. GUI Layer (`ui/`)

### Current State (Day 2)
- `MainWindow` — sidebar navigation (QListWidget) + QStackedWidget content area.
- 8 navigation items, each with a labeled placeholder page.

### Future Modules (per-day schedule)
| Navigation Item | Implementation Day | Description |
|---|---|---|
| Dashboard | Day 24 | Real-time overview — queue, runways, metrics |
| Flights | Day 7 | Add, update, cancel flights; scheduling queue entry |
| Aircraft | Day 8 | Register, update, maintain aircraft |
| Runways | Day 14 | Status overview, manual assignment |
| Scheduling | Day 11–12 | Ready queue display, "Run Scheduler" button, metrics |
| Transactions | Day 21–22 | COMMIT/ROLLBACK demo, isolation viewer |
| Logs | Day 7+ | Operation audit trail |
| Users | Day 6 | Login dialog, user management (Admin role) |

---

## 4. Service Layer (`src/services/`)

`ApplicationService` is the single gateway the UI uses to interact with
business logic. The UI must never call `DatabaseManager` or `Scheduler` directly.

```
MainWindow
    ↓ calls
ApplicationService
    ├── scheduler()        → Scheduler
    ├── resourceManager()  → ResourceManager
    └── (Day 6) authService(), flightService(), ...
```

Future services (Days 6–8):
- `AuthService` — login, password verification, session management.
- `FlightService` — flight CRUD, add to scheduling queue.
- `AircraftService` — aircraft CRUD.

---

## 5. OS Simulation Layer

### Scheduler (`src/scheduler/Scheduler`)

Implements **Non-Preemptive Priority Scheduling**.

| ATC Concept | OS Concept |
|---|---|
| Flight | Process (PCB) |
| Flight ID | Process ID (PID) |
| `priority` field | Scheduling priority (lower = higher) |
| Scheduling queue | Ready queue |
| `burst_time_sec` | CPU burst time |
| Completion time | CT |
| TAT = CT − arrival | Turnaround time |
| WT = TAT − burst | Waiting time |

Priority scale: 0 (Emergency), 1 (Highest), 2 (High), 3 (Normal), 4 (Low), 5–10 (Very Low).

Phase 1 (Day 11): Manual trigger only ("Run Scheduler" button).
Phase 2 (Day 17): Aging — `max(1, base_priority − ⌊waiting_sec / threshold⌋)`.

### ResourceManager (`src/resource/ResourceManager`)

Manages runway access as a **shared OS resource**.

| ATC Concept | OS Concept |
|---|---|
| Runway | Shared resource (printer, critical section) |
| `allocateRunway()` | Acquire resource / enter critical section |
| `releaseRunway()` | Release resource / exit critical section |

Phase 2 (Day 19–20): One `std::mutex` per runway ID enforces mutual exclusion.
`std::timed_mutex` used to prevent deadlock via timeout.

---

## 6. DBMS Layer (`src/database/`)

### DatabaseManager

Singleton that manages the single named Qt SQL connection.

Credentials loaded from `db.conf` (gitignored) via `loadConfig()`.
Never hardcoded. Template: `db.conf.example`.

Database: `airport_runway_db` on `localhost:3306`.

### 8-Table Schema (defined Day 3)

`roles` → `users` → `flights` ← `aircraft`
`flights` → `scheduling_queue` (1:1)
`flights` + `runways` + `users` → `runway_assignments`
`users` → `operation_logs` (ON DELETE SET NULL)

All tables: InnoDB engine (required for ACID transactions).

### ACID Transactions (Phase 2 — Day 21)

5-step atomic runway assignment:
1. `SELECT status FROM runways WHERE id=? FOR UPDATE` — row-level isolation
2. `UPDATE runways SET status='Occupied' WHERE status='Available'`
3. `INSERT INTO runway_assignments ...`
4. `UPDATE flights SET operational_status ...`
5. `INSERT INTO operation_logs ...`
6. COMMIT / ROLLBACK

---

## 7. Model Layer (`src/models/`)

Plain C++17 structs — no Qt base class, no database logic embedded.

| Struct | DB Table | OS Mapping |
|---|---|---|
| `Flight` | `flights` | Process (PCB) |
| `Aircraft` | `aircraft` | — |
| `Runway` | `runways` | Shared resource |
| `User` | `users` | — |

---

## 8. Security

- Passwords stored as SHA-256 hex hash ONLY.
- Implemented via `PasswordUtils::hash()` using `QCryptographicHash::Sha256`.
- No OpenSSL dependency.
- `db.conf` is gitignored — credentials never committed.

---

## 9. Future Data Flow (Phase 2, Day 23)

```
ATC clicks "Run Scheduler"
    → MainWindow → ApplicationService.scheduler().runScheduler()
    → Scheduler: sorts ready queue by (priority ASC, arrival_time ASC)
    → Scheduler: for each flight → DatabaseManager executes UPDATE scheduling_queue
    → Scheduler emits schedulingComplete()
    → MainWindow refreshes scheduling queue widget

ATC assigns runway
    → MainWindow → ApplicationService.resourceManager().allocateRunway(rId, fId)
    → ResourceManager acquires std::mutex for runway rId
    → ResourceManager: BEGIN TRANSACTION (DatabaseManager)
        → SELECT ... FOR UPDATE
        → UPDATE runways SET status='Occupied'
        → INSERT INTO runway_assignments
        → UPDATE flights SET operational_status
        → INSERT INTO operation_logs
    → COMMIT → release mutex
    → ResourceManager emits runwayStatusChanged()
    → MainWindow refreshes runway widget
```
