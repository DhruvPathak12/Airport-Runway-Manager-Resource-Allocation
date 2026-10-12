-- =============================================================================
-- schema.sql
-- Smart Air Traffic Control Management System
-- Database: smartatc
-- =============================================================================
-- Day 3  — Schema + relationships
-- Day 4  — Seed data (sql/seed.sql)
-- Day 5  — Qt SQL / C++ connectivity
-- Day 21 — Transaction workflows (sql/transactions.sql)
-- =============================================================================
-- MySQL 8.0+   |   InnoDB   |   utf8mb4
-- =============================================================================

-- -----------------------------------------------------------------------------
-- Database
-- Create if not present; select it for all subsequent DDL.
-- -----------------------------------------------------------------------------
CREATE DATABASE IF NOT EXISTS smartatc
    CHARACTER SET  utf8mb4
    COLLATE        utf8mb4_unicode_ci;

USE smartatc;

-- =============================================================================
-- TABLE 1: roles
-- =============================================================================
-- Stores the three application roles.
-- Application roles: Admin | ATC | Airport Operator
--
-- Normalization note:
--   Separating roles into their own table (rather than a VARCHAR column on
--   users) satisfies 3NF: role_name is a fact about role_id, not about
--   user_id.  It also enables referential integrity and easy addition of
--   future roles.
-- =============================================================================
CREATE TABLE IF NOT EXISTS roles (
    role_id     INT             NOT NULL AUTO_INCREMENT,
    role_name   VARCHAR(50)     NOT NULL,

    PRIMARY KEY (role_id),
    UNIQUE  KEY uq_roles_role_name (role_name)
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  COLLATE=utf8mb4_unicode_ci
  COMMENT='Application role definitions (Admin, ATC, Airport Operator)';


-- =============================================================================
-- TABLE 2: users
-- =============================================================================
-- Stores application accounts.
--
-- Security rules (enforced at application layer — see PasswordUtils):
--   password_hash stores only the SHA-256 hex string (64 chars).
--   Plaintext passwords are NEVER stored.
--
-- Normalization note:
--   role_id is a FK to roles.  Storing only the FK (not the role name string)
--   satisfies 3NF: role_name is not transitively dependent on user_id.
-- =============================================================================
CREATE TABLE IF NOT EXISTS users (
    user_id         INT             NOT NULL AUTO_INCREMENT,
    username        VARCHAR(50)     NOT NULL,
    password_hash   CHAR(64)        NOT NULL
                        COMMENT 'SHA-256 hex digest — never store plaintext',
    role_id         INT             NOT NULL,
    full_name       VARCHAR(100)    NOT NULL DEFAULT '',
    -- Account lifecycle: Active | Inactive | Suspended
    status          ENUM('Active','Inactive','Suspended')
                                    NOT NULL DEFAULT 'Active',
    created_at      DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,

    PRIMARY KEY (user_id),
    UNIQUE  KEY uq_users_username   (username),

    -- FK: each user belongs to exactly one role
    CONSTRAINT fk_users_role_id
        FOREIGN KEY (role_id) REFERENCES roles (role_id)
        ON DELETE RESTRICT   -- prevent role deletion while users exist
        ON UPDATE CASCADE
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  COLLATE=utf8mb4_unicode_ci
  COMMENT='Application user accounts; passwords stored as SHA-256 hex only';


-- =============================================================================
-- TABLE 3: aircraft
-- =============================================================================
-- Stores aircraft registered in the system.
--
-- Normalization note:
--   Aircraft attributes (type, capacity, status) are facts about the aircraft
--   alone.  Separating aircraft from flights avoids repeating these attributes
--   on every flight record and satisfies 2NF/3NF.
-- =============================================================================
CREATE TABLE IF NOT EXISTS aircraft (
    aircraft_id         INT             NOT NULL AUTO_INCREMENT,
    registration_number VARCHAR(20)     NOT NULL
                            COMMENT 'ICAO/IATA registration, e.g. VT-ANB',
    aircraft_type       VARCHAR(50)     NOT NULL DEFAULT '',
    capacity            SMALLINT UNSIGNED
                                        NOT NULL DEFAULT 0,
    -- Lifecycle: Active | Maintenance | Retired
    status              ENUM('Active','Maintenance','Retired')
                                        NOT NULL DEFAULT 'Active',

    PRIMARY KEY (aircraft_id),
    UNIQUE  KEY uq_aircraft_registration (registration_number),

    CONSTRAINT chk_aircraft_capacity CHECK (capacity >= 0)
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  COLLATE=utf8mb4_unicode_ci
  COMMENT='Registered aircraft; one row per physical aircraft';


-- =============================================================================
-- TABLE 4: flights
-- =============================================================================
-- Stores individual flight operations.
--
-- IMPORTANT — Emergency vs operational status:
--   is_emergency is a BOOLEAN column independent of operational_status.
--   is_emergency=TRUE means this flight has an active emergency declaration.
--   When is_emergency is set, the scheduler assigns effective_priority=1
--   (overriding the base priority) — implemented Day 11/16.
--   This separation prevents conflation of emergency state with lifecycle
--   status (e.g., Scheduled, Landed, Cancelled).
--
-- IMPORTANT — Priority scale (frozen):
--   1 = highest ordinary priority
--   5 = lowest ordinary priority
--   Lower number = higher scheduling priority.
--   Emergency override is handled by is_emergency flag + scheduler logic,
--   NOT by a reserved priority value of 0.
--
-- IMPORTANT — flight_number uniqueness:
--   flight_number is NOT globally unique because airlines reuse flight
--   numbers across operating dates (e.g., AI-202 flies daily).
--   Uniqueness is not enforced here; a composite index on
--   (flight_number, scheduled_time) is added for lookup performance.
--
-- Normalization note:
--   aircraft_id FK prevents repeating aircraft attributes on every flight.
--   Scheduling attributes live in scheduling_queue, not here, to avoid
--   partial dependency on attributes not determined by flight_id alone
--   until the flight enters the scheduling system.
-- =============================================================================
CREATE TABLE IF NOT EXISTS flights (
    flight_id           INT             NOT NULL AUTO_INCREMENT,
    flight_number       VARCHAR(10)     NOT NULL
                            COMMENT 'Not globally unique; reused across dates',
    airline             VARCHAR(100)    NOT NULL DEFAULT '',
    aircraft_id         INT                 NULL
                            COMMENT 'NULL if aircraft not yet assigned',
    source              VARCHAR(100)    NOT NULL DEFAULT '',
    destination         VARCHAR(100)    NOT NULL DEFAULT '',
    -- Arrival | Departure
    flight_type         ENUM('Arrival','Departure')
                                        NOT NULL DEFAULT 'Arrival',
    scheduled_time      DATETIME        NOT NULL,
    -- Priority 1 (highest) to 5 (lowest). Enforced by CHECK.
    priority            TINYINT UNSIGNED
                                        NOT NULL DEFAULT 3,
    -- Emergency flag — independent of operational_status (see notes above)
    is_emergency        TINYINT(1)      NOT NULL DEFAULT 0,
    -- Lifecycle: Scheduled | Taxiing | Cleared | Landed | Departed | Cancelled
    operational_status  ENUM('Scheduled','Taxiing','Cleared',
                             'Landed','Departed','Cancelled')
                                        NOT NULL DEFAULT 'Scheduled',
    created_at          DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,

    PRIMARY KEY (flight_id),

    -- Performance index: look up flights by number + date
    KEY idx_flights_number_time (flight_number, scheduled_time),
    -- Performance index: filter by status
    KEY idx_flights_operational_status (operational_status),

    -- FK: aircraft reference (nullable — aircraft may not be assigned yet)
    CONSTRAINT fk_flights_aircraft_id
        FOREIGN KEY (aircraft_id) REFERENCES aircraft (aircraft_id)
        ON DELETE SET NULL   -- preserve flight record if aircraft is removed
        ON UPDATE CASCADE,

    CONSTRAINT chk_flights_priority
        CHECK (priority BETWEEN 1 AND 5)
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  COLLATE=utf8mb4_unicode_ci
  COMMENT='Flight operations; priority 1=highest, 5=lowest; is_emergency is independent';


-- =============================================================================
-- TABLE 5: runways
-- =============================================================================
-- Represents shared runway resources.
--
-- OS Concept Mapping:
--   Each runway is analogous to a shared OS resource (critical section).
--   Mutual exclusion is enforced at the application level (Day 19) and
--   at the database level through SELECT ... FOR UPDATE (Day 21).
--   The status column reflects current resource ownership:
--     Available  — runway is free; any flight may request it
--     Occupied   — runway is held by one active flight
--     Maintenance — runway is taken offline; no allocation permitted
--
-- Normalization note:
--   Runway attributes are independent of any particular flight.
--   Assignment history is kept in runway_assignments, not here.
-- =============================================================================
CREATE TABLE IF NOT EXISTS runways (
    runway_id       INT             NOT NULL AUTO_INCREMENT,
    runway_number   VARCHAR(10)     NOT NULL
                        COMMENT 'E.g. 09L, 27R',
    -- Landing | Takeoff | Both
    runway_type     ENUM('Landing','Takeoff','Both')
                                    NOT NULL DEFAULT 'Both',
    -- Available | Occupied | Maintenance
    status          ENUM('Available','Occupied','Maintenance')
                                    NOT NULL DEFAULT 'Available',

    PRIMARY KEY (runway_id),
    UNIQUE  KEY uq_runways_runway_number (runway_number)
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  COLLATE=utf8mb4_unicode_ci
  COMMENT='Shared runway resources; status managed via transactions (Day 21)';


-- =============================================================================
-- TABLE 6: scheduling_queue
-- =============================================================================
-- Holds scheduling / OS process-simulation attributes for flights.
--
-- OS Concept Mapping:
--   Each row represents the PCB (Process Control Block) of a flight-as-process.
--   flight_id      → Process ID
--   base_priority  → Static scheduling priority (1–5, as in flights.priority)
--   effective_priority → Dynamically adjusted priority (aging, Phase 2)
--   service_time   → CPU burst time (user-entered, default 60 seconds)
--   waiting_time   → Computed: time in ready queue (Day 11)
--   turnaround_time → Computed: completion_time − arrival_time (Day 11)
--   process_state  → Ready | Running | Completed | Blocked
--
-- Design decision — one active queue record per flight:
--   The UNIQUE constraint on flight_id enforces a 1:0..1 relationship.
--   A flight cannot have two simultaneous scheduling records.
--   Historical scheduling data (e.g., re-queued flights) can be accommodated
--   later by relaxing this constraint; for Phase 1 one record per flight is
--   the correct design.
--
-- Priority scale:
--   Consistent with flights.priority: 1=highest, 5=lowest.
--   In Phase 1 effective_priority = base_priority (no aging).
--   In Phase 2 effective_priority is adjusted by AgingEngine.
--   base_priority is copied from flights.priority at queue-insert time.
--
-- Normalization note:
--   Scheduling attributes (burst time, wait, turnaround, state) are not
--   determined by flight_id alone until the flight enters the scheduler.
--   Keeping them in a separate table avoids sparse NULLs on the flights
--   table for un-scheduled flights and satisfies 2NF.
-- =============================================================================
CREATE TABLE IF NOT EXISTS scheduling_queue (
    queue_id            INT             NOT NULL AUTO_INCREMENT,
    flight_id           INT             NOT NULL,

    -- Timestamp when this flight entered the ready queue
    arrival_time        DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,

    -- Static priority copied from flights.priority at insert time (1–5)
    base_priority       TINYINT UNSIGNED
                                        NOT NULL DEFAULT 3,
    -- Dynamic priority; equals base_priority in Phase 1; adjusted by aging in Phase 2
    effective_priority  TINYINT UNSIGNED
                                        NOT NULL DEFAULT 3,

    -- Service/burst time in seconds; user-entered when adding to queue; default 60
    service_time        SMALLINT UNSIGNED
                                        NOT NULL DEFAULT 60
                            COMMENT 'CPU burst / service time in seconds; default 60',

    -- Computed by scheduler (Day 11); NULL until scheduling cycle runs
    waiting_time        SMALLINT UNSIGNED   NULL DEFAULT NULL
                            COMMENT 'Seconds spent waiting in ready queue',
    turnaround_time     SMALLINT UNSIGNED   NULL DEFAULT NULL
                            COMMENT 'waiting_time + service_time (seconds)',
    completion_time     DATETIME            NULL DEFAULT NULL
                            COMMENT 'Timestamp when process reached Completed state',

    -- Process state: Ready | Running | Completed | Blocked
    process_state       ENUM('Ready','Running','Completed','Blocked')
                                        NOT NULL DEFAULT 'Ready',

    PRIMARY KEY (queue_id),

    -- Enforce 1:0..1 — one active scheduling record per flight (see design notes)
    UNIQUE KEY uq_sq_flight_id (flight_id),

    -- Performance index on effective_priority for scheduler sort
    KEY idx_sq_effective_priority (effective_priority, arrival_time),

    CONSTRAINT fk_sq_flight_id
        FOREIGN KEY (flight_id) REFERENCES flights (flight_id)
        ON DELETE CASCADE    -- removing a flight removes its queue entry
        ON UPDATE CASCADE,

    CONSTRAINT chk_sq_base_priority
        CHECK (base_priority BETWEEN 1 AND 5),
    CONSTRAINT chk_sq_effective_priority
        CHECK (effective_priority BETWEEN 1 AND 5),
    CONSTRAINT chk_sq_service_time
        CHECK (service_time > 0)
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  COLLATE=utf8mb4_unicode_ci
  COMMENT='OS ready-queue / PCB simulation; one active record per flight (Phase 1)';


-- =============================================================================
-- TABLE 7: runway_assignments
-- =============================================================================
-- Records runway allocation history.
--
-- Design decision — historical records:
--   A simple UNIQUE(runway_id) would prevent the same runway ever being
--   re-assigned after its first use, which is incorrect.
--   Instead, application logic (Day 21 transactions) enforces the invariant:
--     "A runway with status=Occupied must not be assigned to another flight
--      while its current assignment has status=Active."
--   This is achieved at runtime via SELECT ... FOR UPDATE inside a transaction.
--   Historical records (status=Released) are preserved for audit purposes.
--
-- Concurrency invariant (to be enforced by application transactions, Day 21):
--   At most one runway_assignments row with (runway_id, status='Active')
--   may exist at any time for a given runway_id.
--   This is NOT enforced by a unique index because it would block history.
--   It IS enforced by the 5-step ACID transaction pattern.
-- =============================================================================
CREATE TABLE IF NOT EXISTS runway_assignments (
    assignment_id   INT             NOT NULL AUTO_INCREMENT,
    flight_id       INT             NOT NULL,
    runway_id       INT             NOT NULL,
    assigned_by     INT                 NULL
                        COMMENT 'user_id of the operator who made the assignment',
    assigned_at     DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    released_at     DATETIME            NULL DEFAULT NULL
                        COMMENT 'NULL until the runway is released',
    -- Active: runway currently occupied by this flight
    -- Released: runway has been freed
    -- Cancelled: assignment was cancelled before use
    status          ENUM('Active','Released','Cancelled')
                                    NOT NULL DEFAULT 'Active',

    PRIMARY KEY (assignment_id),

    -- Index for common queries: "what is assigned to this runway right now?"
    KEY idx_ra_runway_status    (runway_id, status),
    -- Index: "what runways has this flight used?"
    KEY idx_ra_flight_id        (flight_id),

    CONSTRAINT fk_ra_flight_id
        FOREIGN KEY (flight_id) REFERENCES flights (flight_id)
        ON DELETE RESTRICT   -- do not silently delete assignment history
        ON UPDATE CASCADE,

    CONSTRAINT fk_ra_runway_id
        FOREIGN KEY (runway_id) REFERENCES runways (runway_id)
        ON DELETE RESTRICT
        ON UPDATE CASCADE,

    CONSTRAINT fk_ra_assigned_by
        FOREIGN KEY (assigned_by) REFERENCES users (user_id)
        ON DELETE SET NULL   -- preserve history even if user account is removed
        ON UPDATE CASCADE
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  COLLATE=utf8mb4_unicode_ci
  COMMENT='Runway allocation history; active-assignment invariant enforced by transactions';


-- =============================================================================
-- TABLE 8: operation_logs
-- =============================================================================
-- Immutable audit trail for all significant application operations.
--
-- Design decisions:
--   user_id   — SET NULL on delete: logs survive user account removal.
--   flight_id — SET NULL on delete: logs survive flight removal.
--   runway_id — SET NULL on delete: logs survive runway removal.
--   Rows are never updated or deleted by normal application logic.
--
-- OS/DBMS connection:
--   Every transaction (COMMIT or ROLLBACK), scheduling event, emergency
--   declaration, and runway assignment change is recorded here.
--   This provides a verifiable audit trail for DBMS demonstrations.
-- =============================================================================
CREATE TABLE IF NOT EXISTS operation_logs (
    log_id              INT             NOT NULL AUTO_INCREMENT,

    -- Actor — nullable; SET NULL when the user account is removed
    user_id             INT                 NULL,

    -- Operation classification (e.g. LOGIN, FLIGHT_ADD, RUNWAY_ASSIGN,
    -- SCHEDULER_RUN, TX_COMMIT, TX_ROLLBACK, EMERGENCY_DECLARED)
    operation_type      VARCHAR(50)     NOT NULL,

    -- Optional subject references — SET NULL when the referenced row is removed
    flight_id           INT                 NULL,
    runway_id           INT                 NULL,

    -- Transaction outcome, if applicable: Committed | Rolled Back | N/A
    transaction_status  ENUM('Committed','Rolled Back','N/A')
                                        NOT NULL DEFAULT 'N/A',

    message             TEXT                NULL
                            COMMENT 'Human-readable description of the operation',

    created_at          DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,

    PRIMARY KEY (log_id),

    -- Performance indexes for common audit queries
    KEY idx_ol_operation_type   (operation_type),
    KEY idx_ol_created_at       (created_at),
    KEY idx_ol_user_id          (user_id),
    KEY idx_ol_flight_id        (flight_id),

    CONSTRAINT fk_ol_user_id
        FOREIGN KEY (user_id) REFERENCES users (user_id)
        ON DELETE SET NULL
        ON UPDATE CASCADE,

    CONSTRAINT fk_ol_flight_id
        FOREIGN KEY (flight_id) REFERENCES flights (flight_id)
        ON DELETE SET NULL
        ON UPDATE CASCADE,

    CONSTRAINT fk_ol_runway_id
        FOREIGN KEY (runway_id) REFERENCES runways (runway_id)
        ON DELETE SET NULL
        ON UPDATE CASCADE
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  COLLATE=utf8mb4_unicode_ci
  COMMENT='Immutable audit log; FKs use SET NULL to preserve history on row removal';

-- =============================================================================
-- END OF SCHEMA
-- No sample data in this file — see sql/seed.sql (Day 4).
-- No transaction workflows in this file — see sql/transactions.sql (Day 21).
-- =============================================================================
