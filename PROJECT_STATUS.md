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

## CURRENT MILESTONE
Day 3 — COMPLETE

## NEXT MILESTONE
Day 4 — Normalization Validation + Seed Data
