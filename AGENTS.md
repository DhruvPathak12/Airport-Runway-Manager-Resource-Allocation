# AGENTS.md — Smart Air Traffic Control Management System

## Project Identity

| Field | Value |
|---|---|
| Application Title | Smart Air Traffic Control Management System |
| GitHub Repository | Airport Runway Manager & Resource Allocation |
| GitHub Visibility | PRIVATE |
| Root Directory | SmartATC/ |
| Database Name | airport_runway_db |

## Architecture

Source layout — use EXACTLY this structure. Do NOT introduce alternatives.

```
src/
├── models/       ← C++ domain structs (Flight, Process, Runway, Aircraft…)
├── scheduler/    ← PriorityScheduler, AgingEngine, SchedulerController
├── resource/     ← RunwayResourceManager (mutex / critical section)
├── database/     ← DatabaseManager, loadConfig(), data access classes
├── services/     ← Business logic (AuthService, FlightService…)
└── utils/        ← PasswordUtils (SHA-256), Session, helpers
ui/               ← Qt 6 screens/widgets
sql/              ← schema.sql, seed.sql, transactions.sql
```

FORBIDDEN directories — do not create:
- core/
- repositories/
- controllers/

## Technology Stack (FINAL — do not change)

- Language: C++17
- GUI: Qt 6 Widgets
- Database: MySQL 8.x (localhost)
- DB Connectivity: Qt SQL / QMYSQL driver
- Build: CMake + Ninja
- Compiler: MinGW-w64 (C:\Qt\Tools\mingw1310_64)
- Qt path: C:\Qt\6.11.2\mingw_64

## Password Hashing (FINAL)

Use QCryptographicHash::Sha256 (Qt6 built-in).
Do NOT add OpenSSL as a dependency.
Never store passwords in plaintext.

## Database Credentials (FINAL)

Never hardcode credentials.
Load from db.conf via DatabaseManager::loadConfig().
db.conf is gitignored. db.conf.example is committed.

## Scheduler (FINAL)

Phase 1: Manual only — ATC clicks "Run Scheduler" button.
Phase 2: Optional configurable auto-run QTimer.
Algorithm: Non-Preemptive Priority Scheduling.
Burst time: User-entered, default 60 seconds.

## Aging (FINAL — Phase 2 ONLY)

Do NOT implement aging in Phase 1.
Phase 2: Real-time wall-clock QTimer, configurable interval.
Formula: max(1, base_priority - floor(waiting_time_sec / agingThresholdSec))

## 29-Day Roadmap

Phase 1 (Days 1–13):
  Day 1  — Repository + CMake + Qt foundation            [CURRENT]
  Day 2  — Technical architecture + project skeleton
  Day 3  — MySQL schema + relationships
  Day 4  — Normalization + constraints + seed data
  Day 5  — C++ <-> MySQL connectivity
  Day 6  — Login + roles
  Day 7  — Flight management
  Day 8  — Aircraft management
  Day 9  — Process model
  Day 10 — Process states + ready queue
  Day 11 — Basic Priority Scheduler
  Day 12 — Scheduling metrics + Phase 1 integration
  Day 13 — Phase 1 review / hardening

Phase 2 (Days 14–29):
  Day 14 — Runway management
  Day 15 — Resource allocation
  Day 16 — Dynamic emergency priority
  Day 17 — Aging implementation
  Day 18 — Starvation demonstration / prevention
  Day 19 — Critical section + mutual exclusion
  Day 20 — C++ threads + synchronization
  Day 21 — MySQL transactions
  Day 22 — ACID + COMMIT + ROLLBACK
  Day 23 — Concurrency + complete OS/DBMS integration
  Day 24 — Dashboard + monitoring
  Day 25 — Complete testing + bug fixing
  Day 26 — Documentation + diagrams
  Day 27 — Final demo scenario
  Day 28 — GitHub cleanup + final verification
  Day 29 — Viva + individual contribution

## Team Ownership

Member 1: src/utils/ (Session, PasswordUtils), src/services/AuthService,
           src/database/UserRepository, ui/ (LoginDialog, UserMgmtWidget, ProfileWidget)

Member 2: src/models/ (Flight, Aircraft), src/services/ (FlightService, AircraftService),
           src/database/ (FlightRepository, AircraftRepository),
           ui/ (FlightMgmtWidget, AircraftMgmtWidget)

Member 3: src/models/ (Process), src/scheduler/ (all),
           src/database/SchedulingRepository,
           ui/ (SchedulingQueueWidget, ProcessMonitorWidget)

Member 4: src/models/ (Runway, RunwayAssignment), src/resource/ (all),
           src/database/ (RunwayRepository, TransactionManager),
           ui/ (RunwayMgmtWidget, TransactionMonitorWidget, OperationLogsWidget, DashboardWidget)

## Honest Reporting Rule

Never fabricate build results, test results, Git commits, commit hashes,
GitHub operations, or application execution results.
If something cannot be completed, report NOT COMPLETED with the actual reason.
