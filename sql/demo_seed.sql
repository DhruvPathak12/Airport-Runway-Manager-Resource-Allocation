-- =============================================================================
-- demo_seed.sql
-- Smart Air Traffic Control Management System (SmartATC)
-- Database: smartatc
-- =============================================================================
-- Purpose: Complete, idempotent, realistic demonstration seed dataset
-- Target: Live MySQL 8.0+ / InnoDB
-- =============================================================================

USE smartatc;

SET FOREIGN_KEY_CHECKS = 0;

-- -----------------------------------------------------------------------------
-- 1. ROLES
-- -----------------------------------------------------------------------------
INSERT INTO roles (role_id, role_name)
VALUES
    (1, 'Admin'),
    (2, 'ATC'),
    (3, 'Airport Operator')
ON DUPLICATE KEY UPDATE
    role_name = VALUES(role_name);

-- -----------------------------------------------------------------------------
-- 2. USERS (Verified Demo Passwords)
-- -----------------------------------------------------------------------------
-- admin_demo    : AdminPass#2026    (SHA-256: 82ace27b1eeb01d34f6f1a1fc2b19a8e64f86314f31a02698b10e2f329786551)
-- atc_demo      : AtcPass#2026      (SHA-256: 9d9f07a914c3f7ecf17a4b41d83e4b00f430d0307056aa2b72478ca801924eb2)
-- operator_demo : OperatorPass#2026 (SHA-256: c7e53beca11813ddd8bd6c7c4897550b9fd0a34a87255233c4eb36e188ee2af5)
INSERT INTO users (user_id, username, password_hash, role_id, full_name, status)
VALUES
    (1, 'admin_demo',    '82ace27b1eeb01d34f6f1a1fc2b19a8e64f86314f31a02698b10e2f329786551', 1, 'System Administrator (Demo)',   'Active'),
    (2, 'atc_demo',      '9d9f07a914c3f7ecf17a4b41d83e4b00f430d0307056aa2b72478ca801924eb2', 2, 'Radar Controller Alpha (Demo)', 'Active'),
    (3, 'operator_demo', 'c7e53beca11813ddd8bd6c7c4897550b9fd0a34a87255233c4eb36e188ee2af5', 3, 'Ground Marshall Bravo (Demo)',  'Active')
ON DUPLICATE KEY UPDATE
    password_hash = VALUES(password_hash),
    role_id       = VALUES(role_id),
    full_name     = VALUES(full_name),
    status        = VALUES(status);

-- -----------------------------------------------------------------------------
-- 3. AIRCRAFT
-- -----------------------------------------------------------------------------
-- Retain VT-AT1..4 for existing unit test compatibility and add VT-AT01..08
INSERT INTO aircraft (registration_number, aircraft_type, capacity, status)
VALUES
    -- Legacy / Test-verified baseline
    ('VT-AT1',  'Boeing 737-800',         189, 'Active'),
    ('VT-AT2',  'Airbus A320neo',         180, 'Active'),
    ('VT-AT3',  'Boeing 777-300ER',       396, 'Active'),
    ('VT-AT4',  'Bombardier Q400',         78, 'Maintenance'),
    -- Full Fictional Demonstration Fleet (VT-AT01 to VT-AT08)
    ('VT-AT01', 'Boeing 737-800',         189, 'Active'),
    ('VT-AT02', 'Airbus A320neo',         180, 'Active'),
    ('VT-AT03', 'Boeing 777-300ER',       396, 'Active'),
    ('VT-AT04', 'Bombardier Q400',         78, 'Maintenance'),
    ('VT-AT05', 'Airbus A350-900',        325, 'Active'),
    ('VT-AT06', 'Boeing 787-9 Dreamliner', 296, 'Active'),
    ('VT-AT07', 'ATR 72-600',              70, 'Active'),
    ('VT-AT08', 'Airbus A321neo',         222, 'Active')
ON DUPLICATE KEY UPDATE
    aircraft_type = VALUES(aircraft_type),
    capacity      = VALUES(capacity),
    status        = VALUES(status);

-- -----------------------------------------------------------------------------
-- 4. RUNWAYS
-- -----------------------------------------------------------------------------
-- 09L: Both (3,660m) - Occupied (Active Assignment)
-- 09R: Takeoff (2,990m) - Available
-- 27:  Landing (3,445m) - Maintenance
INSERT INTO runways (runway_id, runway_number, runway_type, status)
VALUES
    (1, '09L', 'Both',    'Occupied'),
    (2, '09R', 'Takeoff', 'Available'),
    (3, '27',  'Landing', 'Maintenance')
ON DUPLICATE KEY UPDATE
    runway_number = VALUES(runway_number),
    runway_type   = VALUES(runway_type),
    status        = VALUES(status);

-- -----------------------------------------------------------------------------
-- 5. FLIGHTS (14 Realistic Demonstrations Flights)
-- -----------------------------------------------------------------------------
-- Using deterministic natural key (flight_number, scheduled_time)
INSERT INTO flights (flight_id, flight_number, airline, aircraft_id, source, destination, flight_type, scheduled_time, priority, is_emergency, operational_status)
VALUES
    (1,  'AI-101', 'Air India', (SELECT aircraft_id FROM aircraft WHERE registration_number = 'VT-AT1' LIMIT 1),  'DEL', 'BOM', 'Arrival',   NOW() - INTERVAL 10 MINUTE, 1, 1, 'Cleared'),
    (2,  '6E-202', 'IndiGo',    (SELECT aircraft_id FROM aircraft WHERE registration_number = 'VT-AT2' LIMIT 1),  'BOM', 'BLR', 'Departure', NOW() + INTERVAL 15 MINUTE, 2, 0, 'Taxiing'),
    (3,  'AI-303', 'Air India', (SELECT aircraft_id FROM aircraft WHERE registration_number = 'VT-AT3' LIMIT 1),  'LHR', 'BOM', 'Arrival',   NOW() + INTERVAL 25 MINUTE, 1, 1, 'Scheduled'),
    (4,  'SG-404', 'SpiceJet',  (SELECT aircraft_id FROM aircraft WHERE registration_number = 'VT-AT4' LIMIT 1),  'BOM', 'DXB', 'Departure', NOW() + INTERVAL 30 MINUTE, 4, 0, 'Scheduled'),
    (5,  'UK-505', 'Vistara',   (SELECT aircraft_id FROM aircraft WHERE registration_number = 'VT-AT1' LIMIT 1),  'GOI', 'BOM', 'Arrival',   NOW() + INTERVAL 40 MINUTE, 5, 0, 'Scheduled'),
    (6,  'QP-606', 'Akasa Air', (SELECT aircraft_id FROM aircraft WHERE registration_number = 'VT-AT2' LIMIT 1),  'BOM', 'AMD', 'Departure', NOW() + INTERVAL 45 MINUTE, 4, 0, 'Scheduled'),
    (7,  'AI-202', 'Air India', (SELECT aircraft_id FROM aircraft WHERE registration_number = 'VT-AT02' LIMIT 1), 'BOM', 'JFK', 'Departure', NOW() + INTERVAL 50 MINUTE, 3, 0, 'Scheduled'),
    (8,  '6E-303', 'IndiGo',    (SELECT aircraft_id FROM aircraft WHERE registration_number = 'VT-AT01' LIMIT 1), 'CCU', 'BOM', 'Arrival',   NOW() + INTERVAL 55 MINUTE, 3, 0, 'Scheduled'),
    (9,  'SG-505', 'SpiceJet',  (SELECT aircraft_id FROM aircraft WHERE registration_number = 'VT-AT03' LIMIT 1), 'HYD', 'BOM', 'Arrival',   NOW() + INTERVAL 60 MINUTE, 3, 0, 'Scheduled'),
    (10, 'UK-606', 'Vistara',   (SELECT aircraft_id FROM aircraft WHERE registration_number = 'VT-AT04' LIMIT 1), 'BOM', 'DEL', 'Departure', NOW() + INTERVAL 65 MINUTE, 2, 0, 'Scheduled'),
    (11, 'QP-707', 'Akasa Air', (SELECT aircraft_id FROM aircraft WHERE registration_number = 'VT-AT05' LIMIT 1), 'PNQ', 'BOM', 'Arrival',   NOW() - INTERVAL 45 MINUTE, 3, 0, 'Landed'),
    (12, 'AI-808', 'Air India', (SELECT aircraft_id FROM aircraft WHERE registration_number = 'VT-AT06' LIMIT 1), 'BOM', 'LHR', 'Departure', NOW() - INTERVAL 60 MINUTE, 1, 0, 'Departed'),
    (13, '6E-909', 'IndiGo',    (SELECT aircraft_id FROM aircraft WHERE registration_number = 'VT-AT07' LIMIT 1), 'IXB', 'BOM', 'Arrival',   NOW() + INTERVAL 75 MINUTE, 4, 0, 'Scheduled'),
    (14, 'SG-110', 'SpiceJet',  (SELECT aircraft_id FROM aircraft WHERE registration_number = 'VT-AT08' LIMIT 1), 'BOM', 'MAA', 'Departure', NOW() + INTERVAL 80 MINUTE, 5, 0, 'Scheduled')
ON DUPLICATE KEY UPDATE
    airline            = VALUES(airline),
    aircraft_id        = VALUES(aircraft_id),
    source             = VALUES(source),
    destination        = VALUES(destination),
    flight_type        = VALUES(flight_type),
    scheduled_time     = VALUES(scheduled_time),
    priority           = VALUES(priority),
    is_emergency       = VALUES(is_emergency),
    operational_status = VALUES(operational_status);

-- -----------------------------------------------------------------------------
-- 6. RUNWAY ASSIGNMENTS (Strict Active Assignment on 09L Only)
-- -----------------------------------------------------------------------------
-- Ensure any active assignments on 09R (runway_id=2) or 27 (runway_id=3) are Released
UPDATE runway_assignments
SET status = 'Released', released_at = NOW()
WHERE runway_id IN (2, 3) AND status = 'Active';

-- Ensure Runway 09L has exactly one active assignment: AI-101 (flight_id = 1)
UPDATE runway_assignments
SET status = 'Released', released_at = NOW()
WHERE runway_id = 1 AND flight_id <> 1 AND status = 'Active';

-- Insert or update active assignment for AI-101 on Runway 09L
INSERT INTO runway_assignments (assignment_id, flight_id, runway_id, assigned_by, assigned_at, released_at, status)
VALUES
    (2, 1, 1, 2, NOW() - INTERVAL 10 MINUTE, NULL, 'Active')
ON DUPLICATE KEY UPDATE
    flight_id   = VALUES(flight_id),
    runway_id   = VALUES(runway_id),
    assigned_by = VALUES(assigned_by),
    assigned_at = VALUES(assigned_at),
    released_at = NULL,
    status      = 'Active';

-- -----------------------------------------------------------------------------
-- 7. SCHEDULING QUEUE (OS Process Simulation State)
-- -----------------------------------------------------------------------------
-- AI-101: Running on Runway 09L
-- AI-303: Ready (Emergency Override: base=3, effective=1)
-- UK-505: Ready (Starvation/Aging Candidate: base=5, effective=4, aging_level=1)
-- 6E-202: Ready (base=2, effective=2)
-- 6E-303: Ready (base=3, effective=3)
-- SG-404: Ready (base=4, effective=4)
-- QP-606: Ready (base=4, effective=4)
-- QP-707: Completed (Landed)
-- AI-808: Completed (Departed)
INSERT INTO scheduling_queue (flight_id, arrival_time, ready_since, base_priority, effective_priority, aging_level, service_time, waiting_time, turnaround_time, completion_time, process_state)
VALUES
    (1,  NOW() - INTERVAL 15 MINUTE, NOW() - INTERVAL 15 MINUTE, 1, 1, 0, 60,  60, 120, NULL,                      'Running'),
    (2,  NOW() - INTERVAL 8 MINUTE,  NOW() - INTERVAL 8 MINUTE,  2, 2, 0, 60,  NULL, NULL, NULL,                   'Ready'),
    (3,  NOW() - INTERVAL 5 MINUTE,  NOW() - INTERVAL 5 MINUTE,  3, 1, 0, 90,  NULL, NULL, NULL,                   'Ready'),
    (4,  NOW() - INTERVAL 12 MINUTE, NOW() - INTERVAL 12 MINUTE, 4, 4, 0, 60,  NULL, NULL, NULL,                   'Ready'),
    (5,  NOW() - INTERVAL 45 MINUTE, NOW() - INTERVAL 45 MINUTE, 5, 4, 1, 60,  NULL, NULL, NULL,                   'Ready'),
    (6,  NOW() - INTERVAL 10 MINUTE, NOW() - INTERVAL 10 MINUTE, 4, 4, 0, 60,  NULL, NULL, NULL,                   'Ready'),
    (11, NOW() - INTERVAL 60 MINUTE, NOW() - INTERVAL 60 MINUTE, 3, 3, 0, 60, 120, 180, NOW() - INTERVAL 45 MINUTE, 'Completed'),
    (12, NOW() - INTERVAL 80 MINUTE, NOW() - INTERVAL 80 MINUTE, 1, 1, 0, 60,  30,  90, NOW() - INTERVAL 60 MINUTE, 'Completed')
ON DUPLICATE KEY UPDATE
    arrival_time       = VALUES(arrival_time),
    ready_since        = VALUES(ready_since),
    base_priority      = VALUES(base_priority),
    effective_priority = VALUES(effective_priority),
    aging_level        = VALUES(aging_level),
    service_time       = VALUES(service_time),
    waiting_time       = VALUES(waiting_time),
    turnaround_time    = VALUES(turnaround_time),
    completion_time    = VALUES(completion_time),
    process_state      = VALUES(process_state);

-- -----------------------------------------------------------------------------
-- 8. AUDIT LOGS (operation_logs)
-- -----------------------------------------------------------------------------
INSERT INTO operation_logs (user_id, operation_type, flight_id, runway_id, transaction_status, message, created_at)
VALUES
    (1, 'LOGIN',              NULL, NULL, 'N/A',        'User admin_demo authenticated successfully from 127.0.0.1',        NOW() - INTERVAL 50 MINUTE),
    (2, 'LOGIN',              NULL, NULL, 'N/A',        'User atc_demo authenticated successfully from ATC Radar Console',   NOW() - INTERVAL 40 MINUTE),
    (2, 'EMERGENCY_DECLARED', 3,    NULL, 'N/A',        'Flight AI-303 declared Medical Emergency. Priority elevated to 1.', NOW() - INTERVAL 20 MINUTE),
    (2, 'RUNWAY_ASSIGN',      1,    1,    'Committed',  'Flight AI-101 assigned to Runway 09L (Both 3660m)',                 NOW() - INTERVAL 10 MINUTE),
    (3, 'RUNWAY_MAINTENANCE', NULL, 3,    'Committed',  'Runway 27 set to Maintenance by operator_demo',                    NOW() - INTERVAL 35 MINUTE),
    (2, 'TX_ROLLBACK_DEMO',   4,    1,    'Rolled Back','Atomic rollback test: Runway 09L already occupied by flight AI-101',NOW() - INTERVAL 5 MINUTE);

SET FOREIGN_KEY_CHECKS = 1;
