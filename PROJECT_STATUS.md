# PROJECT STATUS

---

## DAY 1 — Repository + CMake + Qt Foundation ✅ COMPLETE

### Files Created
- CMakeLists.txt
- src/main.cpp
- README.md
- .gitignore
- db.conf.example
- AGENTS.md
- PROJECT_SPEC.md
- docs/.gitkeep, sql/.gitkeep, tests/.gitkeep, ui/.gitkeep
- src/models/.gitkeep, src/scheduler/.gitkeep, src/resource/.gitkeep
- src/database/.gitkeep, src/services/.gitkeep, src/utils/.gitkeep

### Tests
1. CMake configure — PASS
2. CMake build — PASS
3. Application launch — PASS
4. .gitignore verification — PASS
5. Git verification — PASS

### Git
- Branch: main
- Commits: c196c39, 4908d64

### GitHub Repository
- Repository: Airport Runway Manager & Resource Allocation (PRIVATE)
- Link: https://github.com/DhruvPathak12/Airport-Runway-Manager-Resource-Allocation
- Push Status: PUSHED SUCCESSFULLY

---

## DAY 2 — Technical Architecture + Project Skeleton ✅ COMPLETE

### Files Created/Modified
- CMakeLists.txt (updated — all Day 2 sources added)
- src/main.cpp (updated — uses ApplicationService + MainWindow)
- ui/MainWindow.h (NEW)
- ui/MainWindow.cpp (NEW)
- src/services/ApplicationService.h (NEW)
- src/services/ApplicationService.cpp (NEW)
- src/database/DatabaseManager.h (NEW)
- src/database/DatabaseManager.cpp (NEW)
- src/scheduler/Scheduler.h (NEW)
- src/scheduler/Scheduler.cpp (NEW)
- src/resource/ResourceManager.h (NEW)
- src/resource/ResourceManager.cpp (NEW)
- src/utils/PasswordUtils.h (NEW)
- src/utils/PasswordUtils.cpp (NEW)
- src/models/Flight.h (NEW)
- src/models/Aircraft.h (NEW)
- src/models/Runway.h (NEW)
- src/models/User.h (NEW)
- sql/schema.sql (placeholder)
- sql/seed.sql (placeholder)
- sql/transactions.sql (placeholder)
- docs/ARCHITECTURE.md (NEW)

### Architecture Established
- GUI Layer: MainWindow with sidebar navigation + QStackedWidget (8 placeholder pages)
- Service Layer: ApplicationService coordinator between UI and sub-systems
- OS Simulation Layer: Scheduler (future priority scheduling) + ResourceManager (future mutex)
- DBMS Layer: DatabaseManager singleton (future Qt SQL/MySQL connection)
- Model Layer: Flight, Aircraft, Runway, User (C++17 structs)
- Utils: PasswordUtils (SHA-256, implemented Day 6)

### Tests
1. CMake configure (clean) — PASS
2. CMake build (clean, 10/10 units) — PASS
3. Application launch — PASS (PID confirmed running)
4. .gitignore verification — NOT TESTED (no new db.conf risk)
5. Future-feature audit — PASS (no Day 3+ code implemented)

### Known Issues
None.

### Git
- Commit: feat: establish Day 2 application architecture
- Branch: main → origin/main

---

## DAY 3 — MySQL Schema + Relationships ✅ COMPLETE

### Files Created/Modified
- `sql/schema.sql` (Implemented complete 8-table DDL)
- `sql/seed.sql` (Documented deferral to Day 4)
- `sql/transactions.sql` (Documented deferral to Days 21–22)
- `docs/DATABASE_DESIGN.md` (NEW — full database design, 1NF/2NF/3NF rationale, cardinalities, invariants)
- `PROJECT_STATUS.md` (Updated)

### Schema Implemented
- Eight relational tables under database `smartatc`:
  1. `roles` (PK `role_id`, UNIQUE `role_name`)
  2. `users` (PK `user_id`, UNIQUE `username`, FK `role_id`)
  3. `aircraft` (PK `aircraft_id`, UNIQUE `registration_number`)
  4. `flights` (PK `flight_id`, FK `aircraft_id`, non-unique `flight_number`, separate `is_emergency`, CHECK priority 1–5)
  5. `runways` (PK `runway_id`, UNIQUE `runway_number`)
  6. `scheduling_queue` (PK `queue_id`, FK `flight_id` UNIQUE enforcing 1:0..1, CHECK priorities 1–5, CHECK service_time > 0)
  7. `runway_assignments` (PK `assignment_id`, FK `flight_id`, FK `runway_id`, FK `assigned_by`, history preserved)
  8. `operation_logs` (PK `log_id`, FK `user_id`, FK `flight_id`, FK `runway_id`, ON DELETE SET NULL)
- Character set: `utf8mb4`, Collation: `utf8mb4_unicode_ci`, Engine: `InnoDB`

### Documentation Created
- `docs/DATABASE_DESIGN.md`: Detailed breakdown of 8 tables, 1NF/2NF/3NF normalization rationale, decoupling justifications, and concurrency invariants for future transactions.

### Tests
1. Static review of `sql/schema.sql` — PASS
2. Verify eight core tables defined — PASS
3. Primary keys verification — PASS
4. Foreign keys & deletion rules verification — PASS
5. Unique constraints & indexes verification — PASS
6. Priority convention consistency (1–5 scale) — PASS
7. Emergency decoupled from operational status — PASS
8. Historical runway assignments supported without blocking uniqueness — PASS
9. Premature seed data deferred — PASS
10. Premature transaction scripts deferred — PASS
11. Live MySQL execution — NOT TESTED (MySQL 8.0.46 running locally; password required for root access without configured db.conf)
12. C++ application build safety — PASS (10/10 units, 0 errors)
13. Git status and diff verification — PASS

### Known Issues
None.

### Git
- Commit: `feat: implement Day 3 MySQL schema and relationships`
- Branch: `main` → `origin/main`

---

## DAY 4 — Normalization Validation + Seed Data ✅ COMPLETE

### Normalization Review
- **1NF Review**: PASS — All columns across all 8 tables store atomic values. Explicit surrogate PKs on every table.
- **2NF Review**: PASS — Single-attribute surrogate PKs preclude partial key dependencies. Natural candidate keys analyzed and verified.
- **3NF Review**: PASS — No transitive dependencies. Roles, aircraft specs, scheduling metrics, and runway assignments isolated into dedicated tables.
- **Schema Modifications**: None required. Day 3 schema adheres cleanly to 3NF and project specifications.

### Seed Data Implemented (`sql/seed.sql`)
- Database: `smartatc`
- **Table Coverage (8/8 tables)**:
  1. `roles`: 3 rows (`Admin`, `ATC`, `Airport Operator`)
  2. `users`: 3 demo accounts (`admin_demo`, `atc_demo`, `operator_demo`) with placeholder SHA-256 hashes
  3. `aircraft`: 4 aircraft (`VT-AT1`, `VT-AT2`, `VT-AT3`, `VT-AT4` with `Active`/`Maintenance` statuses)
  4. `flights`: 6 flights (`AI-101`, `6E-202`, `AI-303`, `SG-404`, `UK-505`, `QP-606`) covering priorities 1–5, emergency flag (`AI-101`), and varied lifecycle statuses
  5. `runways`: 3 runways (`09L` Occupied, `09R` Available, `27` Maintenance)
  6. `scheduling_queue`: 4 PCB records spanning `Running`, `Ready`, and `Completed` process states
  7. `runway_assignments`: 2 assignment records (1 `Released` historical assignment, 1 `Active` non-conflicting assignment on `09L`)
  8. `operation_logs`: 4 audit records covering `SYSTEM_INIT`, `RUNWAY_RELEASE`, `EMERGENCY_DECLARED`, and `RUNWAY_ASSIGN`

### Repeatability & Safety Design
- Idempotent upserts (`ON DUPLICATE KEY UPDATE` and `INSERT ... SELECT ... WHERE NOT EXISTS`).
- Dynamic foreign key resolution based on natural keys (usernames, registration numbers, runway numbers, flight number + date).
- No hardcoded `AUTO_INCREMENT` IDs.
- Zero destructive statements (`DROP TABLE` and `TRUNCATE` strictly prohibited).
- Safe re-execution preserves existing records and prevents duplicate entries.

### Tests
1. Normalization review completed — PASS
2. All eight expected tables considered — PASS
3. Three required roles seeded — PASS
4. At least four aircraft included — PASS (4 aircraft)
5. At least six flights included — PASS (6 flights)
6. At least three runways included — PASS (3 runways)
7. Scheduling entries satisfy schema constraints — PASS
8. Assignment references valid — PASS
9. No conflicting active assignment seeded — PASS (Only 1 active assignment for 09L)
10. Operation-log references valid — PASS
11. Seed script designed for repeatable execution — PASS
12. Seed script executed successfully against MySQL — NOT TESTED (MySQL 8.0.46 running locally; password required for root access without configured db.conf)
13. Seed script executed a second time without duplicates — NOT TESTED (MySQL auth blocker)
14. Referential integrity verified against live MySQL — NOT TESTED (MySQL auth blocker)
15. Existing schema and data preserved — PASS
16. Qt project build safety — PASS (Build unimpacted, static schema validation)
17. Git diff and status reviewed — PASS

### Known Issues
- Live MySQL testing remains blocked until authorized credentials are provided in a local `db.conf`.

### Git
- Commit: `feat: validate normalization and add Day 4 seed data`
- Branch: `main` → `origin/main`

---

## CURRENT MILESTONE
Day 4 — COMPLETE

## NEXT MILESTONE
Day 5 — C++ to MySQL Connectivity

