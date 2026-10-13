# DATABASE_DESIGN.md — Smart Air Traffic Control Management System

Database: `smartatc`  
Engine: InnoDB  
Charset/Collation: `utf8mb4` / `utf8mb4_unicode_ci`  

---

## 1. Table Specifications & Responsibilities

The database schema consists of exactly eight core tables designed to model both airport ground/air operations and OS-simulation concepts.

### 1.1 `roles`
- **Purpose**: Defines application access roles for RBAC (Role-Based Access Control).
- **Primary Key**: `role_id` (INT AUTO_INCREMENT)
- **Attributes**:
  - `role_name` VARCHAR(50) NOT NULL UNIQUE (`Admin`, `ATC`, `Airport Operator`)
- **Foreign Keys**: None

### 1.2 `users`
- **Purpose**: Stores authenticated user accounts.
- **Primary Key**: `user_id` (INT AUTO_INCREMENT)
- **Attributes**:
  - `username` VARCHAR(50) NOT NULL UNIQUE
  - `password_hash` CHAR(64) NOT NULL (SHA-256 hex string, plaintext strictly prohibited)
  - `role_id` INT NOT NULL (FK -> `roles.role_id`)
  - `full_name` VARCHAR(100) NOT NULL DEFAULT ''
  - `status` ENUM('Active', 'Inactive', 'Suspended') NOT NULL DEFAULT 'Active'
  - `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
- **Foreign Keys**:
  - `role_id` references `roles(role_id)` ON DELETE RESTRICT ON UPDATE CASCADE

### 1.3 `aircraft`
- **Purpose**: Stores physical aircraft registered in the system.
- **Primary Key**: `aircraft_id` (INT AUTO_INCREMENT)
- **Attributes**:
  - `registration_number` VARCHAR(20) NOT NULL UNIQUE (e.g. `VT-ANB`)
  - `aircraft_type` VARCHAR(50) NOT NULL DEFAULT ''
  - `capacity` SMALLINT UNSIGNED NOT NULL DEFAULT 0 (CHECK `capacity >= 0`)
  - `status` ENUM('Active', 'Maintenance', 'Retired') NOT NULL DEFAULT 'Active'
- **Foreign Keys**: None

### 1.4 `flights`
- **Purpose**: Tracks operational flights arriving or departing the airport.
- **Primary Key**: `flight_id` (INT AUTO_INCREMENT)
- **Attributes**:
  - `flight_number` VARCHAR(10) NOT NULL (Non-unique, reused across dates)
  - `airline` VARCHAR(100) NOT NULL DEFAULT ''
  - `aircraft_id` INT NULL (FK -> `aircraft.aircraft_id`)
  - `source` VARCHAR(100) NOT NULL DEFAULT ''
  - `destination` VARCHAR(100) NOT NULL DEFAULT ''
  - `flight_type` ENUM('Arrival', 'Departure') NOT NULL DEFAULT 'Arrival'
  - `scheduled_time` DATETIME NOT NULL
  - `priority` TINYINT UNSIGNED NOT NULL DEFAULT 3 (CHECK `priority BETWEEN 1 AND 5`)
  - `is_emergency` TINYINT(1) NOT NULL DEFAULT 0
  - `operational_status` ENUM('Scheduled', 'Taxiing', 'Cleared', 'Landed', 'Departed', 'Cancelled') NOT NULL DEFAULT 'Scheduled'
  - `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
- **Foreign Keys**:
  - `aircraft_id` references `aircraft(aircraft_id)` ON DELETE SET NULL ON UPDATE CASCADE
- **Indexes**:
  - `idx_flights_number_time` (`flight_number`, `scheduled_time`)
  - `idx_flights_operational_status` (`operational_status`)
- **Important Design Choices**:
  - `is_emergency` is decoupled from `operational_status`. A flight can be in emergency while in any status (e.g., `Taxiing` or `Cleared`).
  - Priority scale follows frozen project rules: `1` = Highest ordinary priority, `5` = Lowest ordinary priority.

### 1.5 `runways`
- **Purpose**: Represents runways as shared resources in the airport.
- **Primary Key**: `runway_id` (INT AUTO_INCREMENT)
- **Attributes**:
  - `runway_number` VARCHAR(10) NOT NULL UNIQUE (e.g. `09L`, `27R`)
  - `runway_type` ENUM('Landing', 'Takeoff', 'Both') NOT NULL DEFAULT 'Both'
  - `status` ENUM('Available', 'Occupied', 'Maintenance') NOT NULL DEFAULT 'Available'
- **Foreign Keys**: None

### 1.6 `scheduling_queue`
- **Purpose**: Stores simulated OS ready-queue attributes and process scheduling metadata.
- **Primary Key**: `queue_id` (INT AUTO_INCREMENT)
- **Attributes**:
  - `flight_id` INT NOT NULL UNIQUE (FK -> `flights.flight_id`, enforces 1:0..1 relationship)
  - `arrival_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
  - `base_priority` TINYINT UNSIGNED NOT NULL DEFAULT 3 (CHECK `base_priority BETWEEN 1 AND 5`)
  - `effective_priority` TINYINT UNSIGNED NOT NULL DEFAULT 3 (CHECK `effective_priority BETWEEN 1 AND 5`)
  - `service_time` SMALLINT UNSIGNED NOT NULL DEFAULT 60 (CHECK `service_time > 0`, in seconds)
  - `waiting_time` SMALLINT UNSIGNED NULL DEFAULT NULL
  - `turnaround_time` SMALLINT UNSIGNED NULL DEFAULT NULL
  - `completion_time` DATETIME NULL DEFAULT NULL
  - `process_state` ENUM('Ready', 'Running', 'Completed', 'Blocked') NOT NULL DEFAULT 'Ready'
- **Foreign Keys**:
  - `flight_id` references `flights(flight_id)` ON DELETE CASCADE ON UPDATE CASCADE
- **Indexes**:
  - `idx_sq_effective_priority` (`effective_priority`, `arrival_time`)
- **Important Design Choices**:
  - `flight_id` has a UNIQUE constraint, guaranteeing that a flight cannot have more than one active scheduling record.
  - Metrics (`waiting_time`, `turnaround_time`, `completion_time`) are computed upon execution by the scheduler algorithm.

### 1.7 `runway_assignments`
- **Purpose**: Maintains the audit history and current state of runway allocations.
- **Primary Key**: `assignment_id` (INT AUTO_INCREMENT)
- **Attributes**:
  - `flight_id` INT NOT NULL (FK -> `flights.flight_id`)
  - `runway_id` INT NOT NULL (FK -> `runways.runway_id`)
  - `assigned_by` INT NULL (FK -> `users.user_id`)
  - `assigned_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
  - `released_at` DATETIME NULL DEFAULT NULL
  - `status` ENUM('Active', 'Released', 'Cancelled') NOT NULL DEFAULT 'Active'
- **Foreign Keys**:
  - `flight_id` references `flights(flight_id)` ON DELETE RESTRICT ON UPDATE CASCADE
  - `runway_id` references `runways(runway_id)` ON DELETE RESTRICT ON UPDATE CASCADE
  - `assigned_by` references `users(user_id)` ON DELETE SET NULL ON UPDATE CASCADE
- **Indexes**:
  - `idx_ra_runway_status` (`runway_id`, `status`)
  - `idx_ra_flight_id` (`flight_id`)

### 1.8 `operation_logs`
- **Purpose**: System-wide immutable audit trail for operations, events, and transactions.
- **Primary Key**: `log_id` (INT AUTO_INCREMENT)
- **Attributes**:
  - `user_id` INT NULL (FK -> `users.user_id`)
  - `operation_type` VARCHAR(50) NOT NULL
  - `flight_id` INT NULL (FK -> `flights.flight_id`)
  - `runway_id` INT NULL (FK -> `runways.runway_id`)
  - `transaction_status` ENUM('Committed', 'Rolled Back', 'N/A') NOT NULL DEFAULT 'N/A'
  - `message` TEXT NULL
  - `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
- **Foreign Keys**:
  - `user_id` references `users(user_id)` ON DELETE SET NULL ON UPDATE CASCADE
  - `flight_id` references `flights(flight_id)` ON DELETE SET NULL ON UPDATE CASCADE
  - `runway_id` references `runways(runway_id)` ON DELETE SET NULL ON UPDATE CASCADE
- **Indexes**:
  - `idx_ol_operation_type` (`operation_type`)
  - `idx_ol_created_at` (`created_at`)
  - `idx_ol_user_id` (`user_id`)
  - `idx_ol_flight_id` (`flight_id`)

---

## 2. Relationship Cardinalities

```text
roles (1) ─────────────< (N) users
users (1) ─────────────< (N) operation_logs (actor, nullable)
users (1) ─────────────< (N) runway_assignments (operator, nullable)

aircraft (1) ──────────< (N) flights
flights (1) ───────────- (0..1) scheduling_queue (enforced by UNIQUE flight_id)
flights (1) ───────────< (N) runway_assignments
flights (1) ───────────< (N) operation_logs (subject, nullable)

runways (1) ───────────< (N) runway_assignments
runways (1) ───────────< (N) operation_logs (subject, nullable)
```

- `roles` 1:N `users`: Every user belongs to exactly one role. Roles cannot be deleted if referenced (`ON DELETE RESTRICT`).
- `aircraft` 1:N `flights`: An aircraft operates multiple flights over time. If an aircraft record is removed, flight history is preserved with `aircraft_id = NULL` (`ON DELETE SET NULL`).
- `flights` 1:0..1 `scheduling_queue`: A flight enters the ready queue when active; uniqueness ensures no duplicate queuing.
- `flights` 1:N `runway_assignments`: A flight may have assignments across different stages or revisions. Deleting flights with active assignments is restricted (`ON DELETE RESTRICT`).
- `runways` 1:N `runway_assignments`: A runway has a full history of past and active flight assignments.
- `operation_logs`: Preserves historical audit records even if referenced users, flights, or runways are deleted (`ON DELETE SET NULL`).

---

## 3. Normalization Rationale (1NF, 2NF, 3NF)

### First Normal Form (1NF)
- All columns contain atomic, indivisible values (e.g., flight numbers, dates, status enums).
- No repeating groups or multivalued arrays.
- Every table has a clearly defined primary key.

### Second Normal Form (2NF)
- The schema is in 1NF.
- All tables use single-column primary keys (`table_id` surrogate keys).
- As a consequence of single-attribute candidate keys, there are no partial functional dependencies: every non-key attribute is fully functionally dependent on the primary key.

### Third Normal Form (3NF)
- The schema is in 2NF.
- There are no transitive functional dependencies ($X \to Y \to Z$ where $Z$ is non-prime):
  - **Roles**: Extracted into `roles` rather than embedding `role_name`, permissions, or role descriptions inside `users`.
  - **Aircraft**: Aircraft specifications (`aircraft_type`, `capacity`) depend on `aircraft_id`, not `flight_id`. Separating `aircraft` avoids data redundancy and update anomalies in `flights`.
  - **Runway Assignments**: Separation of assignment history from `runways` and `flights` ensures flight/runway entities do not store transitory allocation timestamps.

---

## 4. Architectural Separation Decisions

### 4.1 Separation of `runway_assignments` from `runways` and `flights`
- Placing current assignment information directly on `runways` (e.g., `current_flight_id`, `assigned_at`) would erase allocation history once the runway is released.
- Placing runway information on `flights` prevents modeling runway reuse across multiple flights over time.
- `runway_assignments` acts as an associative entity capturing temporal state (`assigned_at`, `released_at`, `status`), enabling historical queries and auditing.

### 4.2 Separation of `scheduling_queue` from `flights`
- Not all flights in the database are actively in the OS ready queue (e.g., flights scheduled hours ahead, completed flights, cancelled flights).
- Storing burst time, queue arrival, dynamic effective priority, and turnaround time directly in `flights` would cause sparse columns with extensive `NULL` values.
- Separating `scheduling_queue` isolates the OS scheduling simulation lifecycle from the static flight metadata.

### 4.3 Preservation of Audit History in `operation_logs`
- Operations, system errors, and transaction outcomes must remain immutable.
- Foreign keys in `operation_logs` explicitly use `ON DELETE SET NULL`. If an operator or test flight is cleaned up, the audit event log remains intact with timestamp, operation type, and descriptive message.

---

## 5. Concurrency & Transactional Invariants

While foreign keys and unique constraints protect relational integrity, certain critical multi-row and multi-table business invariants cannot be enforced through static DDL constraints alone and are designed to be enforced through DBMS ACID transactions (implemented in Days 21–22):

1. **Exclusive Runway Occupancy**:
   - A runway can have only one `Active` assignment at any given time.
   - Statically placing `UNIQUE(runway_id)` on `runway_assignments` would incorrectly prevent historical records (`status = 'Released'`).
   - *Transactional Enforcement (Day 21)*:
     ```sql
     START TRANSACTION;
     SELECT status FROM runways WHERE runway_id = ? FOR UPDATE;
     -- Check runway is 'Available' and no active assignment exists
     UPDATE runways SET status = 'Occupied' WHERE runway_id = ?;
     INSERT INTO runway_assignments (flight_id, runway_id, status) VALUES (?, ?, 'Active');
     COMMIT;
     ```
2. **Atomic Scheduler Dispatch**:
   - Moving a process state from `Ready` to `Running` in `scheduling_queue` while updating `flights.operational_status` must execute as an atomic transaction.

---

## 6. Day 4 Normalization Review Findings

On Day 4, a formal normalization audit was performed on `sql/schema.sql`:

1. **First Normal Form (1NF) Compliance**:
   - Every column in all 8 tables stores strictly atomic, single values. No multivalued columns, JSON arrays, or delimited strings are used.
   - Every table possesses an explicit primary key (`INT AUTO_INCREMENT` surrogate key).
2. **Second Normal Form (2NF) Compliance**:
   - Because all 8 tables use single-column primary keys (`table_id`), partial functional dependency on a composite key is structurally impossible.
   - Alternate candidate keys:
     - `roles(role_name)`: Single-column candidate key.
     - `users(username)`: Single-column candidate key.
     - `aircraft(registration_number)`: Single-column candidate key.
     - `runways(runway_number)`: Single-column candidate key.
     - `scheduling_queue(flight_id)`: Single-column candidate key.
     - `flights(flight_number, scheduled_time)`: Composite index / natural candidate key for scheduled instances. All flight attributes (airline, aircraft_id, source, destination, priority, status) depend on the flight instance as a whole, not merely on the recurring `flight_number`.
   - Full functional dependency is satisfied across all tables.
3. **Third Normal Form (3NF) Compliance**:
   - No non-key attribute transitively depends on another non-key attribute:
     - `users`: Roles are factored into `roles(role_id, role_name)`. `role_name` does not reside in `users`.
     - `flights`: Aircraft specifications (`capacity`, `aircraft_type`, `status`) reside in `aircraft`, referenced solely by `aircraft_id`.
     - `scheduling_queue`: Scheduling simulation metrics (`service_time`, `waiting_time`, `turnaround_time`, `process_state`) are factored out of `flights` to prevent sparse nulls and transitive dependencies.
     - `runway_assignments`: Temporal assignment states (`assigned_at`, `released_at`, `status`, `assigned_by`) are factored out of `runways` and `flights`.
4. **Specific Design Invariant Review**:
   - `flights.aircraft_id`: Nullable FK (`ON DELETE SET NULL`), correctly handling unassigned aircraft or fleet adjustments without deleting flight operations.
   - `flight_number` Non-Uniqueness: Correctly left non-unique to accommodate recurring commercial flight numbers across operational dates. Lookup efficiency is preserved by the composite index `idx_flights_number_time`.
   - `is_emergency` Decoupling: Properly maintained as an independent `TINYINT(1)` flag. This enables emergency priority escalation (effective_priority = 1) without mutating or corrupting physical operational statuses (`Taxiing`, `Cleared`, etc.).
   - Frozen Priority Scale: Enforced strictly with CHECK constraints `BETWEEN 1 AND 5` (1 = Highest, 5 = Lowest).
   - One-Current-Record Queue: Enforced by `UNIQUE(flight_id)` in `scheduling_queue`.
   - Audit Trail Preservation: Foreign keys on `operation_logs` use `ON DELETE SET NULL`, ensuring the audit trail remains immutable even if users, flights, or runways are deleted.

**Conclusion:** The schema is fully normalized up to 3NF. No schema modifications or corrective DDL changes were required.

---

## 7. Seed Data & Idempotency Strategy (Day 4)

The seed script `sql/seed.sql` provides repeatable sample data for all 8 tables:

### 7.1 Data Volume & Coverage
- **Roles (3)**: `Admin`, `ATC`, `Airport Operator`.
- **Users (3)**: `admin_demo`, `atc_demo`, `operator_demo` assigned to their respective roles.
  - *Security Limitation*: Passwords use a dummy placeholder SHA-256 hash. These are not valid login credentials and must be generated via Day 6 `PasswordUtils` authentication workflows.
- **Aircraft (4)**: `VT-AT1` (B737-800, Active), `VT-AT2` (A320neo, Active), `VT-AT3` (B777-300ER, Active), `VT-AT4` (Q400, Maintenance).
- **Flights (6)**: Fictional operations (`AI-101`, `6E-202`, `AI-303`, `SG-404`, `UK-505`, `QP-606`) spanning varied origins/destinations, priorities 1 through 5, and statuses (`Taxiing`, `Cleared`, `Scheduled`, `Landed`). Includes emergency flight `AI-101` (`is_emergency = 1`, `priority = 1`).
- **Runways (3)**: `09L` (Occupied by `AI-101`), `09R` (Available), `27` (Maintenance).
- **Scheduling Queue (4)**: Simulated process control blocks across states:
  - `AI-101`: Running, base=1, effective=1, burst=60s.
  - `6E-202`: Ready, base=2, effective=2, burst=60s.
  - `AI-303`: Ready, base=3, effective=3, burst=90s.
  - `UK-505`: Completed, base=3, effective=3, wait=45s, turnaround=105s.
- **Runway Assignments (2)**:
  - Historical: Runway `09L` assigned to `UK-505` earlier, `Released`.
  - Active: Runway `09L` assigned to `AI-101`, `Active`. Concurrency invariant maintained (only one active assignment for runway `09L`).
- **Operation Logs (4)**: Immutable audit records for `SYSTEM_INIT`, `RUNWAY_RELEASE`, `EMERGENCY_DECLARED`, and `RUNWAY_ASSIGN`.

### 7.2 Repeatability & Idempotency
- **No AUTO_INCREMENT hardcoding**: All foreign keys are dynamically resolved using natural keys (`role_name`, `username`, `registration_number`, `runway_number`, `flight_number` + `scheduled_time`).
- **Upsert Patterns**:
  - Tables with unique keys (`roles`, `users`, `aircraft`, `runways`, `scheduling_queue`) use `INSERT ... ON DUPLICATE KEY UPDATE`.
  - Tables with temporal or associative records (`flights`, `runway_assignments`, `operation_logs`) use `INSERT ... SELECT ... WHERE NOT EXISTS`.
- **Zero Data Loss**: The script does not issue `DROP TABLE` or `TRUNCATE TABLE`. Re-running `sql/seed.sql` produces the identical dataset without duplicating rows.

### 7.3 How to Execute Seed Data
To apply the seed script via an authorized MySQL session:
```powershell
# Interactive MySQL CLI
mysql -u <authorized_user> -p smartatc < sql/seed.sql
```
Or execute `sql/seed.sql` directly within MySQL Workbench.

### 7.4 Live Validation vs Static Review Status
- **Static Review**: PASS (All table relationships, constraint compliance, and idempotency logic verified).
- **Live MySQL Validation**: NOT TESTED (Local MySQL 8.0.46 service is active, but root access requires a password that is not stored in project configuration per security rules. No credentials were exposed or modified).

