# PROJECT_SPEC.md — Smart Air Traffic Control Management System

## Project Type
Desktop application — NOT a website or web service.

## Official Titles
- Application/project title: Smart Air Traffic Control Management System
- GitHub repository name: Airport Runway Manager & Resource Allocation

## Technology Stack (FINAL — do not change)

| Component | Technology |
|---|---|
| Language | C++17 |
| GUI | Qt 6 (Widgets) |
| Database | MySQL 8.x |
| DB Connectivity | Qt SQL / QMYSQL driver |
| Build | CMake + Ninja |
| Compiler | MinGW-w64 (C:\Qt\Tools\mingw1310_64) |
| Qt | C:\Qt\6.11.2\mingw_64 |
| IDE | Qt Creator |
| Version Control | Git + GitHub |

## Three Roles
1. ATC (Air Traffic Controller) — Scheduling, queue, emergency
2. Airport Operator — Runway assignment, resource management
3. Admin — User management, system logs, full access

## Eight Database Tables
1. users
2. roles
3. flights
4. aircraft
5. runways
6. scheduling_queue
7. runway_assignments
8. operation_logs

## Scheduling Algorithm
Non-Preemptive Priority Scheduling
- Lower number = higher priority
- Phase 1: Manual trigger (ATC clicks "Run Scheduler")
- Phase 2: Optional configurable auto-run QTimer

## Burst Time
- User-entered when adding flight to scheduling queue
- Default: 60 seconds
- UI label: "Service Time (seconds)"
- DB column: scheduling_queue.burst_time_sec

## Password Hashing
- QCryptographicHash::Sha256 (Qt6 built-in)
- No OpenSSL dependency
- Never store passwords in plaintext

## Database Credentials
- Never hardcoded in source code
- Loaded from db.conf via DatabaseManager::loadConfig()
- db.conf is gitignored
- db.conf.example (template with placeholders) is committed

## Aging (Phase 2 ONLY)
- Formula: max(1, base_priority - floor(waiting_time_sec / agingThresholdSec))
- Real-time wall-clock QTimer, configurable interval
- Zero aging code in Phase 1

## GitHub Repository
- Name: Airport Runway Manager & Resource Allocation
- Visibility: PRIVATE
- Local root directory: SmartATC/

## Six Modules
1. Auth & User Management (Member 1)
2. Flight & Aircraft Management (Member 2)
3. Process & Scheduling (Member 3)
4. Runway Resource Management + Transactions (Member 4)
5. Dashboard & Monitoring (Member 4)
6. Core Infrastructure — DatabaseManager, Session, PasswordUtils (Member 1)

## Phase 1 — Days 1–13
40% OS concepts + 40% DBMS concepts
Manual scheduler only. No aging. No transactions. No runway assignment.
Ends with Phase 1 review gate — must be approved before Phase 2 begins.

## Phase 2 — Days 14–29
Remaining OS + DBMS + full integration + docs + viva prep
Aging, emergency priority, mutex, transactions, ACID, full dashboard.
