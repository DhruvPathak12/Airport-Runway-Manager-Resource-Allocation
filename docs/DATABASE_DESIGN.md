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
