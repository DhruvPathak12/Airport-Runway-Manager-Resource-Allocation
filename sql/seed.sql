-- =============================================================================
-- seed.sql — Smart Air Traffic Control Management System
-- Database: smartatc
-- =============================================================================
-- Day 4 milestone: Normalization Validation + Repeatable Seed Data
-- =============================================================================
-- Engine: InnoDB  |  Charset: utf8mb4  |  Collation: utf8mb4_unicode_ci
-- =============================================================================
-- Idempotency Strategy:
--   1. Deterministic natural keys are used with INSERT ... ON DUPLICATE KEY UPDATE.
--   2. Foreign keys are resolved dynamically using stable unique identifiers
--      (e.g., username, registration_number, runway_number, flight_number + date).
--   3. No AUTO_INCREMENT ID values are hardcoded.
--   4. No DROP TABLE or TRUNCATE statements are used; existing data is preserved.
--   5. Re-running this script produces the exact same deterministic dataset
--      without creating duplicate rows or conflicting active assignments.
-- =============================================================================

USE smartatc;

-- =============================================================================
-- SECTION 1: ROLES
-- =============================================================================
-- Three required application roles: Admin, ATC, Airport Operator.
-- Natural key: role_name (UNIQUE in schema.sql).
-- =============================================================================
INSERT INTO roles (role_name)
VALUES
    ('Admin'),
    ('ATC'),
    ('Airport Operator')
ON DUPLICATE KEY UPDATE
    role_name = VALUES(role_name);


-- =============================================================================
-- SECTION 2: USERS (DEMO / PLACEHOLDER ACCOUNTS)
-- =============================================================================
-- Demo development accounts representing each of the 3 roles.
-- Security notice:
--   - Plaintext passwords are NEVER stored.
--   - The placeholder SHA-256 hashes below are dummy strings for schema demo
--     and testing only. They are NOT valid user login credentials.
--   - Accounts have status='Active' for relational demo purposes (e.g. assigned_by).
--   - Real login workflows and live password verification will be implemented
--     in Day 6 using PasswordUtils.
-- Natural key: username (UNIQUE in schema.sql).
-- =============================================================================
INSERT INTO users (username, password_hash, role_id, full_name, status)
VALUES
    (
        'admin_demo',
        'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855', -- SHA-256 of empty string placeholder
        (SELECT role_id FROM roles WHERE role_name = 'Admin' LIMIT 1),
        'System Administrator (Demo)',
        'Active'
    ),
    (
        'atc_demo',
        'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855',
        (SELECT role_id FROM roles WHERE role_name = 'ATC' LIMIT 1),
        'Radar Controller Alpha (Demo)',
        'Active'
    ),
    (
        'operator_demo',
        'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855',
        (SELECT role_id FROM roles WHERE role_name = 'Airport Operator' LIMIT 1),
        'Ground Marshall Bravo (Demo)',
        'Active'
    )
ON DUPLICATE KEY UPDATE
    full_name     = VALUES(full_name),
    status        = VALUES(status),
    role_id       = VALUES(role_id);


-- =============================================================================
-- SECTION 3: AIRCRAFT
-- =============================================================================
-- Four representative aircraft records.
-- Natural key: registration_number (UNIQUE in schema.sql).
-- Fictional educational simulation registrations (VT-* series).
-- =============================================================================
INSERT INTO aircraft (registration_number, aircraft_type, capacity, status)
VALUES
    ('VT-AT1', 'Boeing 737-800',     189, 'Active'),
    ('VT-AT2', 'Airbus A320neo',     180, 'Active'),
    ('VT-AT3', 'Boeing 777-300ER',   396, 'Active'),
    ('VT-AT4', 'Bombardier Q400',     78, 'Maintenance')
ON DUPLICATE KEY UPDATE
    aircraft_type = VALUES(aircraft_type),
    capacity      = VALUES(capacity),
    status        = VALUES(status);


-- =============================================================================
-- SECTION 4: FLIGHTS
-- =============================================================================
-- Six fictional educational flight operations.
-- Varied airlines, routes, types, scheduled times, priorities (1–5), statuses.
-- AI-101 demonstrates an emergency declaration (is_emergency=1) with priority=1.
-- AI-202 and 6E-303 demonstrate non-emergency flights with diverse priorities.
--
-- Idempotency strategy:
--   flights does not have a global UNIQUE constraint on flight_number alone
--   (flight numbers recur across dates). However, (flight_number, scheduled_time)
--   represents a deterministic scheduled instance.
--   We use an idempotent SELECT ... WHERE NOT EXISTS insert block to guarantee
--   no duplicate rows are created across repeated script executions.
-- =============================================================================
INSERT INTO flights (
    flight_number, airline, aircraft_id, source, destination,
    flight_type, scheduled_time, priority, is_emergency, operational_status
)
SELECT 'AI-101', 'Air India', a.aircraft_id, 'DEL', 'BOM', 'Arrival', '2026-10-15 10:00:00', 1, 1, 'Taxiing'
FROM aircraft a WHERE a.registration_number = 'VT-AT1'
AND NOT EXISTS (
    SELECT 1 FROM flights WHERE flight_number = 'AI-101' AND scheduled_time = '2026-10-15 10:00:00'
);

INSERT INTO flights (
    flight_number, airline, aircraft_id, source, destination,
    flight_type, scheduled_time, priority, is_emergency, operational_status
)
SELECT '6E-202', 'IndiGo', a.aircraft_id, 'BOM', 'BLR', 'Departure', '2026-10-15 10:15:00', 2, 0, 'Cleared'
FROM aircraft a WHERE a.registration_number = 'VT-AT2'
AND NOT EXISTS (
    SELECT 1 FROM flights WHERE flight_number = '6E-202' AND scheduled_time = '2026-10-15 10:15:00'
);

INSERT INTO flights (
    flight_number, airline, aircraft_id, source, destination,
    flight_type, scheduled_time, priority, is_emergency, operational_status
)
SELECT 'AI-303', 'Air India', a.aircraft_id, 'LHR', 'BOM', 'Arrival', '2026-10-15 10:30:00', 3, 0, 'Scheduled'
FROM aircraft a WHERE a.registration_number = 'VT-AT3'
AND NOT EXISTS (
    SELECT 1 FROM flights WHERE flight_number = 'AI-303' AND scheduled_time = '2026-10-15 10:30:00'
);

INSERT INTO flights (
    flight_number, airline, aircraft_id, source, destination,
    flight_type, scheduled_time, priority, is_emergency, operational_status
)
SELECT 'SG-404', 'SpiceJet', a.aircraft_id, 'BOM', 'GOI', 'Departure', '2026-10-15 10:45:00', 4, 0, 'Scheduled'
FROM aircraft a WHERE a.registration_number = 'VT-AT4'
AND NOT EXISTS (
    SELECT 1 FROM flights WHERE flight_number = 'SG-404' AND scheduled_time = '2026-10-15 10:45:00'
);

INSERT INTO flights (
    flight_number, airline, aircraft_id, source, destination,
    flight_type, scheduled_time, priority, is_emergency, operational_status
)
SELECT 'UK-505', 'Vistara', a.aircraft_id, 'DEL', 'BOM', 'Arrival', '2026-10-15 08:30:00', 3, 0, 'Landed'
FROM aircraft a WHERE a.registration_number = 'VT-AT1'
AND NOT EXISTS (
    SELECT 1 FROM flights WHERE flight_number = 'UK-505' AND scheduled_time = '2026-10-15 08:30:00'
);

INSERT INTO flights (
    flight_number, airline, aircraft_id, source, destination,
    flight_type, scheduled_time, priority, is_emergency, operational_status
)
SELECT 'QP-606', 'Akasa Air', a.aircraft_id, 'BOM', 'HYD', 'Departure', '2026-10-15 11:00:00', 5, 0, 'Scheduled'
FROM aircraft a WHERE a.registration_number = 'VT-AT2'
AND NOT EXISTS (
    SELECT 1 FROM flights WHERE flight_number = 'QP-606' AND scheduled_time = '2026-10-15 11:00:00'
);


-- =============================================================================
-- SECTION 5: RUNWAYS
-- =============================================================================
-- Three fictional runway resources.
-- Natural key: runway_number (UNIQUE in schema.sql).
-- Status values:
--   09L: Occupied    (Assigned to flight AI-101 in Section 7)
--   09R: Available   (Free shared resource)
--   27 : Maintenance (Offline resource)
-- =============================================================================
INSERT INTO runways (runway_number, runway_type, status)
VALUES
    ('09L', 'Both',    'Occupied'),
    ('09R', 'Takeoff', 'Available'),
    ('27',  'Landing', 'Maintenance')
ON DUPLICATE KEY UPDATE
    runway_type = VALUES(runway_type),
    status      = VALUES(status);


-- =============================================================================
-- SECTION 6: SCHEDULING QUEUE
-- =============================================================================
-- OS Ready Queue / PCB simulation records.
-- Natural / candidate key: flight_id (UNIQUE in schema.sql).
--
-- Seeded flights:
--   1. AI-101: Emergency process, base_priority=1, effective_priority=1, Running
--   2. 6E-202: High priority process, base_priority=2, effective_priority=2, Ready
--   3. AI-303: Normal priority process, base_priority=3, effective_priority=3, Ready
--   4. UK-505: Historical completed process, Completed, Turnaround=105s, Wait=45s
--
-- Dynamic foreign-key lookup via (flight_number, scheduled_time).
-- Safe ON DUPLICATE KEY UPDATE on flight_id.
-- =============================================================================
INSERT INTO scheduling_queue (
    flight_id, arrival_time, base_priority, effective_priority,
    service_time, waiting_time, turnaround_time, completion_time, process_state
)
VALUES
    (
        (SELECT flight_id FROM flights WHERE flight_number = 'AI-101' AND scheduled_time = '2026-10-15 10:00:00' LIMIT 1),
        '2026-10-15 09:50:00',
        1, 1,
        60,
        0, NULL, NULL,
        'Running'
    ),
    (
        (SELECT flight_id FROM flights WHERE flight_number = '6E-202' AND scheduled_time = '2026-10-15 10:15:00' LIMIT 1),
        '2026-10-15 09:55:00',
        2, 2,
        60,
        NULL, NULL, NULL,
        'Ready'
    ),
    (
        (SELECT flight_id FROM flights WHERE flight_number = 'AI-303' AND scheduled_time = '2026-10-15 10:30:00' LIMIT 1),
        '2026-10-15 10:00:00',
        3, 3,
        90,
        NULL, NULL, NULL,
        'Ready'
    ),
    (
        (SELECT flight_id FROM flights WHERE flight_number = 'UK-505' AND scheduled_time = '2026-10-15 08:30:00' LIMIT 1),
        '2026-10-15 08:15:00',
        3, 3,
        60,
        45, 105, '2026-10-15 09:00:00',
        'Completed'
    )
ON DUPLICATE KEY UPDATE
    base_priority      = VALUES(base_priority),
    effective_priority = VALUES(effective_priority),
    service_time       = VALUES(service_time),
    waiting_time       = VALUES(waiting_time),
    turnaround_time    = VALUES(turnaround_time),
    completion_time    = VALUES(completion_time),
    process_state      = VALUES(process_state);


-- =============================================================================
-- SECTION 7: RUNWAY ASSIGNMENTS
-- =============================================================================
-- Records both active and historical resource allocations.
-- Invariants enforced:
--   - Exactly one Active assignment exists: Runway 09L occupied by flight AI-101.
--   - Matches runways.status = 'Occupied' for runway 09L.
--   - Historical assignment: UK-505 on 09L earlier, status = 'Released' with release timestamp.
--
-- Idempotency strategy:
--   INSERT ... SELECT ... WHERE NOT EXISTS checking (flight_id, runway_id, assigned_at)
--   ensures repeated runs do not duplicate assignment rows.
-- =============================================================================

-- 1. Historical released assignment (UK-505 on 09L, landed earlier)
INSERT INTO runway_assignments (
    flight_id, runway_id, assigned_by, assigned_at, released_at, status
)
SELECT
    f.flight_id,
    r.runway_id,
    u.user_id,
    '2026-10-15 08:25:00',
    '2026-10-15 08:55:00',
    'Released'
FROM flights f
JOIN runways r ON r.runway_number = '09L'
LEFT JOIN users u ON u.username = 'operator_demo'
WHERE f.flight_number = 'UK-505' AND f.scheduled_time = '2026-10-15 08:30:00'
AND NOT EXISTS (
    SELECT 1 FROM runway_assignments ra
    WHERE ra.flight_id = f.flight_id
      AND ra.runway_id = r.runway_id
      AND ra.assigned_at = '2026-10-15 08:25:00'
);

-- 2. Currently active assignment (AI-101 on 09L, matches runways.status='Occupied')
INSERT INTO runway_assignments (
    flight_id, runway_id, assigned_by, assigned_at, released_at, status
)
SELECT
    f.flight_id,
    r.runway_id,
    u.user_id,
    '2026-10-15 09:58:00',
    NULL,
    'Active'
FROM flights f
JOIN runways r ON r.runway_number = '09L'
LEFT JOIN users u ON u.username = 'operator_demo'
WHERE f.flight_number = 'AI-101' AND f.scheduled_time = '2026-10-15 10:00:00'
AND NOT EXISTS (
    SELECT 1 FROM runway_assignments ra
    WHERE ra.flight_id = f.flight_id
      AND ra.runway_id = r.runway_id
      AND ra.assigned_at = '2026-10-15 09:58:00'
);


-- =============================================================================
-- SECTION 8: OPERATION LOGS
-- =============================================================================
-- Sample immutable audit records for demonstration.
-- Idempotency strategy:
--   INSERT ... SELECT ... WHERE NOT EXISTS checking (operation_type, created_at, message).
-- =============================================================================
INSERT INTO operation_logs (
    user_id, operation_type, flight_id, runway_id, transaction_status, message, created_at
)
SELECT
    u.user_id,
    'SYSTEM_INIT',
    NULL,
    NULL,
    'Committed',
    'Seeded demonstration environment initialized successfully.',
    '2026-10-15 08:00:00'
FROM users u
WHERE u.username = 'admin_demo'
AND NOT EXISTS (
    SELECT 1 FROM operation_logs
    WHERE operation_type = 'SYSTEM_INIT' AND created_at = '2026-10-15 08:00:00'
);

INSERT INTO operation_logs (
    user_id, operation_type, flight_id, runway_id, transaction_status, message, created_at
)
SELECT
    u.user_id,
    'RUNWAY_RELEASE',
    f.flight_id,
    r.runway_id,
    'Committed',
    'Runway 09L released by flight UK-505 after successful landing.',
    '2026-10-15 08:55:00'
FROM users u
CROSS JOIN flights f
CROSS JOIN runways r
WHERE u.username = 'operator_demo'
  AND f.flight_number = 'UK-505' AND f.scheduled_time = '2026-10-15 08:30:00'
  AND r.runway_number = '09L'
AND NOT EXISTS (
    SELECT 1 FROM operation_logs
    WHERE operation_type = 'RUNWAY_RELEASE' AND created_at = '2026-10-15 08:55:00'
);

INSERT INTO operation_logs (
    user_id, operation_type, flight_id, runway_id, transaction_status, message, created_at
)
SELECT
    u.user_id,
    'EMERGENCY_DECLARED',
    f.flight_id,
    NULL,
    'Committed',
    'In-flight emergency declared for AI-101. Priority promoted to highest.',
    '2026-10-15 09:52:00'
FROM users u
CROSS JOIN flights f
WHERE u.username = 'atc_demo'
  AND f.flight_number = 'AI-101' AND f.scheduled_time = '2026-10-15 10:00:00'
AND NOT EXISTS (
    SELECT 1 FROM operation_logs
    WHERE operation_type = 'EMERGENCY_DECLARED' AND created_at = '2026-10-15 09:52:00'
);

INSERT INTO operation_logs (
    user_id, operation_type, flight_id, runway_id, transaction_status, message, created_at
)
SELECT
    u.user_id,
    'RUNWAY_ASSIGN',
    f.flight_id,
    r.runway_id,
    'Committed',
    'Emergency assignment: Runway 09L allocated to flight AI-101.',
    '2026-10-15 09:58:00'
FROM users u
CROSS JOIN flights f
CROSS JOIN runways r
WHERE u.username = 'operator_demo'
  AND f.flight_number = 'AI-101' AND f.scheduled_time = '2026-10-15 10:00:00'
  AND r.runway_number = '09L'
AND NOT EXISTS (
    SELECT 1 FROM operation_logs
    WHERE operation_type = 'RUNWAY_ASSIGN' AND created_at = '2026-10-15 09:58:00'
);

-- =============================================================================
-- END OF SEED DATA
-- =============================================================================
