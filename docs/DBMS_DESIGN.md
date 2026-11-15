# Database Management System (DBMS) Design — SmartATC

## 1. Overview & Rationale for MySQL 8

SmartATC employs **MySQL Server 8.0.x** (specifically verified against MySQL 8.0.46) as its relational persistence engine. 

### Why MySQL?
1. **Full ACID Transactional Support:** Via the **InnoDB** storage engine, providing write-ahead logging (WAL), crash recovery, row-level locking, and multi-version concurrency control (MVCC).
2. **Pessimistic Concurrency Control:** Support for explicit row-level locking via `SELECT ... FOR UPDATE`, essential for preventing race conditions during runway allocations.
3. **Cross-Platform Enterprise Standard:** Universally supported in industrial and academic environments, linking seamlessly with Qt 6 via the native `QMYSQL` driver.
4. **Strict SQL Standards Compliance:** Robust implementation of declarative foreign key constraints (`ON DELETE RESTRICT` / `CASCADE`), check constraints, composite unique indexes, and generated default timestamps.

---

## 2. Relational Database Concepts & Implementation

### 2.1 Keys & Constraints
- **Primary Keys:** Every table defines a surrogate integer primary key (`*_id`) utilizing `AUTO_INCREMENT` for high indexing efficiency and stable B-Tree indexing.
- **Foreign Keys:** Physical relational integrity is strictly enforced across tables. For example:
  - `flights.aircraft_id` references `aircraft.aircraft_id` with `ON DELETE SET NULL`.
  - `runway_assignments.runway_id` references `runways.runway_id` with `ON DELETE CASCADE`.
  - `runway_assignments.flight_id` references `flights.flight_id` with `ON DELETE CASCADE`.
  - `operation_logs.user_id` references `users.user_id` with `ON DELETE SET NULL`.
- **Unique Constraints:** Enforces 1:0..1 and domain cardinality:
  - `users.username` (`UNIQUE`): Prohibits duplicate user login handles.
  - `aircraft.registration` (`UNIQUE`): Prohibits duplicate airframe tail numbers.
  - `runways.runway_number` (`UNIQUE`): Prohibits duplicate runway identifiers (e.g. `09L`).
  - `scheduling_queue.flight_id` (`UNIQUE`): Enforces that a flight cannot have more than one active scheduling PCB.
- **NOT NULL Constraints:** Critical attributes (such as `flight_number`, `status`, `scheduled_time`, `role_name`) strictly forbid null values, eliminating indeterminate three-valued logic (`NULL`) in relational predicates.
- **CHECK Constraints:** Declarative constraints enforce valid numerical ranges:
  - `chk_aircraft_capacity CHECK (capacity >= 0)`
  - `chk_flight_priority CHECK (priority BETWEEN 1 AND 5)`

---

## 3. Database Normalization Analysis

The SmartATC schema is strictly normalized through **Third Normal Form (3NF)** and **Boyce-Codd Normal Form (BCNF)**:

### 3.1 First Normal Form (1NF)
- All attributes are atomic (single-valued).
- No repeating groups or comma-separated lists exist (e.g., flight operations and runway assignments are stored in separate dedicated tables).
- Each table possesses a designated unique primary key.

### 3.2 Second Normal Form (2NF)
- The schema is in 1NF.
- Every non-key attribute is fully functionally dependent on the entire primary key.
- Because all tables utilize single-column surrogate primary keys (`queue_id`, `flight_id`, `runway_id`), partial key dependencies are mathematically impossible.
- Flight scheduling attributes (burst time, waiting time, turnaround time) are deliberately segregated into `scheduling_queue` rather than remaining in `flights`, avoiding partial dependencies on unscheduled flights.

### 3.3 Third Normal Form (3NF) & BCNF
- The schema is in 2NF.
- There are no transitive dependencies: non-key attributes depend only on the primary key, and not on other non-key attributes.
- Example: User roles are factored into a dedicated `roles` lookup table (`roles.role_id`), preventing user records from duplicating role descriptions. Aircraft attributes (model, capacity) reside in `aircraft`, eliminating transitive redundancy in `flights`.

---

## 4. Query Performance & Indexing Strategy

To guarantee high responsiveness under heavy flight volumes, explicit B-Tree indexes are deployed across primary search and join paths:

| Table | Index Name | Indexed Columns | Design Purpose |
|---|---|---|---|
| `users` | `idx_users_role` | `role_id` | Fast join on role lookup during authentication. |
| `flights` | `idx_flights_aircraft` | `aircraft_id` | Foreign key traversal from aircraft to flights. |
| `flights` | `idx_flights_status` | `operational_status` | Fast filtering for active vs terminal flights. |
| `flights` | `idx_flights_lookup` | `(flight_number, scheduled_time)` | Composite index for timetable searches. |
| `runways` | `idx_runways_status` | `status` | Instant filtering of available runways. |
| `scheduling_queue`| `idx_sched_effective_priority` | `effective_priority` | Accelerates priority candidate scans. |
| `scheduling_queue`| `idx_sched_state` | `process_state` | Accelerates ready-queue population. |
| `runway_assignments`| `idx_assignments_runway_status` | `(runway_id, status)` | Fast verification of active runway occupancy. |
| `runway_assignments`| `idx_assignments_flight_status` | `(flight_id, status)` | Fast verification of active flight assignments. |
| `operation_logs` | `idx_logs_created_at` | `created_at` | Rapid chronological audit log pagination. |

---

## 5. Security & Parameterized Query Implementation

To eliminate SQL Injection attacks (OWASP Top 10 A03), **raw SQL string concatenation is strictly forbidden** across the codebase. All database interactions utilize prepared statements with strongly typed parameter bindings:

```cpp
// Example: Safe parameterized runway assignment query
QSqlQuery q(db);
q.prepare(QStringLiteral(
    "INSERT INTO runway_assignments (runway_id, flight_id, status, assigned_at) "
    "VALUES (:runwayId, :flightId, 'Active', NOW());"
));
q.bindValue(QStringLiteral(":runwayId"), runwayId);
q.bindValue(QStringLiteral(":flightId"), flightId);
if (!q.exec()) {
    *outError = q.lastError().text();
    return false;
}
```

---

## 6. Concurrency Control & The InnoDB Engine

### 6.1 Transaction Isolation Level
SmartATC operates on MySQL's default isolation level: **`REPEATABLE READ`**.
- Prevents **Dirty Reads** (reading uncommitted changes from concurrent transactions).
- Prevents **Non-Repeatable Reads** (MVCC ensures consistent snapshot reads throughout a transaction).

### 6.2 Pessimistic Row Locking (`SELECT ... FOR UPDATE`)
During runway allocation, multiple ATC controllers or automated threads may simultaneously attempt to claim the same runway. To prevent race conditions:
```sql
-- Acquire exclusive write-lock on target runway row
SELECT runway_id, status, runway_type 
FROM runways 
WHERE runway_id = ? 
FOR UPDATE;
```
1. `FOR UPDATE` applies an exclusive lock ($X$-lock) to the specific index record in InnoDB.
2. Any competing transaction attempting to inspect or lock that runway is blocked until the first transaction executes `COMMIT` or `ROLLBACK`.
3. If the first transaction commits, the runway status becomes `Occupied`, causing the second transaction to immediately read the updated status and abort cleanly.

---

## 7. Service-to-Database Mapping

`ApplicationService` acts as the single gateway to the database:
- Connections are acquired from `DatabaseManager::instance()`.
- Unconnected environments gracefully fall back to in-memory mock models.
- Database operations emit Qt signals (`flightsChanged`, `runwaysChanged`, `processesChanged`), decoupling database writes from GUI updates.
