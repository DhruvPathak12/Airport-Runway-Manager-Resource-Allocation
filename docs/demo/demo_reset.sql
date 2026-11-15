-- ====================================================================
-- SmartATC Non-Destructive Demonstration Reset Script
-- Target Database: smartatc (MySQL 8 / InnoDB)
-- Purpose: Re-establish pristine baseline for B.Tech Viva Demonstration
-- ====================================================================

START TRANSACTION;

-- 1. Close any active runway assignments (mark Released with current timestamp)
UPDATE runway_assignments
SET status = 'Released',
    released_at = NOW()
WHERE status = 'Active';

-- 2. Restore runway physical operational statuses
-- Runway 1 (09L): Available (Both Landing/Takeoff)
-- Runway 2 (09R): Available (Takeoff only)
-- Runway 3 (27):  Maintenance (Simulated closed runway)
UPDATE runways
SET status = 'Available'
WHERE runway_id IN (1, 2);

UPDATE runways
SET status = 'Maintenance'
WHERE runway_id = 3;

-- 3. Reset demo flights to baseline Scheduled states and clear emergency flags
UPDATE flights
SET operational_status = 'Scheduled',
    is_emergency = 0
WHERE flight_number IN ('AI-101', '6E-202', 'AI-303', 'SG-404', 'UK-505');

-- Specifically verify priorities:
-- AI-101 (Priority 2)
UPDATE flights SET priority = 2 WHERE flight_number = 'AI-101';
-- 6E-202 (Priority 3)
UPDATE flights SET priority = 3 WHERE flight_number = '6E-202';
-- AI-303 (Priority 3)
UPDATE flights SET priority = 3 WHERE flight_number = 'AI-303';
-- SG-404 (Priority 4)
UPDATE flights SET priority = 4 WHERE flight_number = 'SG-404';
-- UK-505 (Priority 5)
UPDATE flights SET priority = 5 WHERE flight_number = 'UK-505';

-- 4. Re-populate scheduling queue in Ready state
DELETE FROM scheduling_queue WHERE flight_id IN (
    SELECT flight_id FROM flights WHERE flight_number IN ('AI-101', '6E-202', 'AI-303', 'SG-404', 'UK-505')
);

INSERT INTO scheduling_queue (flight_id, arrival_time, ready_since, base_priority, effective_priority, aging_level, service_time, process_state)
SELECT flight_id, NOW(), NOW(), priority, priority, 0, 60, 'Ready'
FROM flights
WHERE flight_number IN ('AI-101', '6E-202', 'AI-303', 'SG-404', 'UK-505')
ORDER BY priority ASC, flight_id ASC;

-- 5. Record autonomous audit log entry
INSERT INTO operation_logs (user_id, operation_type, transaction_status, message, created_at)
VALUES (
    (SELECT user_id FROM users WHERE username = 'admin_demo' LIMIT 1),
    'SYSTEM_RESET',
    'Committed',
    'Demonstration baseline state successfully re-established.',
    NOW()
);

COMMIT;
