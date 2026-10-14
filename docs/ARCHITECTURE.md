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

## 8. Security & Authentication (Day 6)

### Password Hashing
- Stored as SHA-256 hex digest ONLY via `PasswordUtils::hash()`.
- Implemented with built-in Qt `QCryptographicHash::Sha256` with zero external dependencies (no OpenSSL).
- Plaintext passwords and digests are never retained in session state, logged, or displayed in error messages.
- **Academic Limitation**: SHA-256 alone is a fast cryptographic digest without salt or configurable work factor (unlike bcrypt, Argon2, or PBKDF2). This is a frozen academic requirement and must not be represented as production-grade authentication.

### Authentication Flow (`ApplicationService::login`)
1. Validates non-empty username and password.
2. Checks database availability via `DatabaseManager`.
3. Executes parameterized query joining `users` and `roles`:
   ```sql
   SELECT u.user_id, u.username, u.password_hash, u.full_name, u.status, r.role_name
   FROM users u
   JOIN roles r ON u.role_id = r.role_id
   WHERE u.username = :username
   ```
4. Verifies account status (`Active`). Inactive or unknown accounts are rejected.
5. Verifies submitted password hash via `PasswordUtils::verify()`.
6. Generates generic "Invalid username or password" on failure to avoid leaking user existence.
7. On success, instantiates an in-memory `Session` object holding `userId`, `username`, `fullName`, and `roleName`.
8. Emits `loginSuccessful(Session)` and notifies UI to switch view stack.

### In-Memory Session Management (`Session`)
- Exists strictly in volatile process memory while authenticated.
- Never written to disk, config files, or cookies.
- Cleared completely upon `logout()` or application termination.
- Exposes role checkers (`isAdmin()`, `isATC()`, `isOperator()`) and centralized permission queries (`hasPermission(key)`).

### Role-Based Access Control (RBAC) Matrix
| Permission Key / Page | Admin | ATC | Airport Operator |
|---|:---:|:---:|:---:|
| `Dashboard` | ✅ | ✅ | ✅ |
| `Flights` | View Only | View & Operational Controls | View Only |
| `Aircraft` | View & Manage | View Only | View & Operational Controls |
| `Runways` | View Only | Operational Allocation | Maintenance Controls |
| `Scheduling` | View & Queue | Operational & Run Queue | Denied |
| `Transactions` | View & Audit | View Transactions | Denied |
| `Logs` | View All | ATC Operations | Aircraft/Maintenance |
| `Users` | Manage & Assign | Denied | Denied |

- UI navigation dynamically hides and disables unauthorized items based on `Session::allowedNavItems()`.
- Service/Business methods perform backend permission checks prior to executing protected operations (e.g. `ApplicationService::executeSchedulerCycle()`, `allocateRunwayResource()`, `setRunwayMaintenanceStatus()`).

### Hardened Development Account Provisioning Workflow
To prevent password exposure in process listings, terminal history, and logs, passwords are **never passed as command-line arguments** and are no longer encoded using a colon-separated string.

Development account provisioning uses non-echoing console input with confirmation:
```powershell
./SmartATC.exe --provision-user <username> --role <Admin|ATC|"Airport Operator"> [--fullname "<Display Name>"]
```
**Interactive Prompts:**
- `Enter Password: ` (Masked / non-echoing console input via Win32 `SetConsoleMode` / POSIX `termios`)
- `Confirm Password: ` (Validates match before proceeding)

**Security Controls:**
- **Zero Command-Line Password Exposure**: Passwords do not appear in process inspection tools, shell logs, or history.
- **Unrestricted Characters**: Passwords safely support colons (`:`), spaces, punctuation, and unicode without parsing corruption.
- **Production Guard**: Provisioning immediately aborts with fatal error if `SMARTATC_ENV=production`.
- **Database Validation**: Validates `db.conf`, sanitizes SQL via bound parameters, and updates or provisions status `'Active'`.

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

---

## 10. Flight Management Architecture (Day 7)

```
┌────────────────────────────────────────────────────────┐
│  FlightsView (ui/FlightsView.h, .cpp)                  │
│  - QTableWidget displaying active/persisted flights    │
│  - Multi-attribute search & status filter combo        │
│  - Add / Edit / Delete action buttons (ATC role only)   │
│  - FlightDialog (ui/FlightDialog.h, .cpp) modal form   │
└───────────────────────────┬────────────────────────────┘
                            │
┌───────────────────────────▼────────────────────────────┐
│  ApplicationService (src/services/)                    │
│  - getAllFlights(), getFlightById()                   │
│  - createFlight(), updateFlight(), deleteFlight()      │
│  - canDeleteFlight(), getAvailableAircraft()           │
│  - Service-boundary RBAC: action:ManageFlights check  │
│  - Emits flightsChanged() signal on database mutations │
└───────────────────────────┬────────────────────────────┘
                            │ Parameterized Qt SQL queries
┌───────────────────────────▼────────────────────────────┐
│  DatabaseManager (src/database/) → MySQL 8.0           │
│  - flights table (PK flight_id, FK aircraft_id)        │
│  - Foreign-key protection against delete (RESTRICT)   │
└────────────────────────────────────────────────────────┘
```

### Architectural Key Decisions:
- **Zero Raw Form Concatenation**: All SQL statements use Qt SQL prepared queries (`QSqlQuery::prepare()`) and bound values (`bindValue()`).
- **Two-Tier RBAC**:
  - UI Tier: `FlightsView` hides/disables Add/Edit/Delete buttons and shows a Read-Only notice for Admin and Airport Operator sessions.
  - Service Tier: `ApplicationService` enforces `m_session.hasPermission("action:ManageFlights")` on all write/mutate operations, returning explicit authorization errors for unauthenticated or unauthorized callers.
- **Safe Delete & Referential Integrity**:
  - `runway_assignments` defines `CONSTRAINT fk_ra_flight_id FOREIGN KEY (flight_id) REFERENCES flights (flight_id) ON DELETE RESTRICT`.
  - `ApplicationService::canDeleteFlight()` and `deleteFlight()` catch foreign key constraints (MySQL error 1451) gracefully.
  - The GUI prompts controllers to transition referenced flights to `'Cancelled'` status rather than destructive deletion, keeping operational audit trails intact.

---

## 11. Aircraft Management Architecture (Day 8)

```
┌────────────────────────────────────────────────────────┐
│  AircraftView (ui/AircraftView.h, .cpp)                │
│  - QTableWidget displaying fleet inventory & flights   │
│  - Multi-attribute search & status filter combo        │
│  - Add / Delete buttons (Admin role only)              │
│  - Edit button (Admin: full; Operator: status only)    │
│  - AircraftDialog (ui/AircraftDialog.h, .cpp) modal   │
└───────────────────────────┬────────────────────────────┘
                            │
┌───────────────────────────▼────────────────────────────┐
│  ApplicationService (src/services/)                    │
│  - getAllAircraft(), getAircraftById()                │
│  - getAircraftByRegistration()                         │
│  - createAircraft(), updateAircraft()                  │
│  - updateAircraftStatus(), deleteAircraft()            │
│  - canDeleteAircraft()                                 │
│  - Service-boundary RBAC:                              │
│    * action:AdministerAircraft (Admin only)            │
│    * action:ManageAircraft (Admin / Operator)          │
│    * page:Aircraft (Admin / Operator / ATC)            │
│  - Emits aircraftChanged() & flightsChanged() signals  │
└───────────────────────────┬────────────────────────────┘
                            │ Parameterized Qt SQL queries
┌───────────────────────────▼────────────────────────────┐
│  DatabaseManager (src/database/) → MySQL 8.0           │
│  - aircraft table (PK aircraft_id, UNIQUE reg_no)      │
│  - flights table references aircraft_id (FK)           │
│  - Safe delete checks prevent deleting active fleet   │
└────────────────────────────────────────────────────────┘
```

### Architectural Key Decisions:
- **Centralized Multi-Tier RBAC**:
  - **Admin**: Has `action:AdministerAircraft` for registering new aircraft, editing all specifications, and safely deleting unreferenced aircraft.
  - **Airport Operator**: Has `action:ManageAircraft` for updating operational statuses (`Active`, `Maintenance`, `Retired`). Registration, deletion, and technical specification modifications are locked in the GUI and rejected at the service layer.
  - **ATC**: Has `page:Aircraft` read-only access for operational flight assignments; all management actions are prohibited.
  - **Unauthenticated**: Completely barred from viewing or managing fleet records.
- **Referential Integrity & Safe Deletion**:
  - Pre-deletion check verifies whether any flights in `flights` reference `aircraft_id`.
  - If references exist, hard delete is blocked, explaining the conflict to the user and offering safe retirement (`status = 'Retired'`) to preserve flight operation history.
- **Cross-Module Synchronization**:
  - `ApplicationService` emits `aircraftChanged()` and `flightsChanged()` upon aircraft record mutations, ensuring `AircraftView` and `FlightsView` (as well as flight assignment aircraft dropdowns) remain synchronized with MySQL.


