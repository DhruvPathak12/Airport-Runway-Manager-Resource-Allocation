-- ====================================================================
-- SmartATC Non-Destructive Demonstration Reset Script
-- Target Database: smartatc (MySQL 8 / InnoDB)
-- Purpose: Re-establish pristine baseline for B.Tech Viva Demonstration
-- ====================================================================

START TRANSACTION;

-- 1. Close any active runway assignments
UPDATE runway_assignments
SET status = 'Released',
    released_at = NOW()
WHERE status = 'Active';

-- 2. Restore runway physical operational statuses
UPDATE runways
SET status = 'Available'
WHERE id IN (1, 2);

UPDATE runways
SET status = 'Maintenance'
WHERE id = 3;

-- 3. Reset demo flights to baseline Scheduled states
UPDATE flights
SET operational_status = 'Scheduled',
    emergency_flag = 0,
    effective_priority = base_priority
WHERE flight_number IN ('AI-101', '6E-202', 'AI-303', 'SG-404', 'UK-505');

-- Specifically verify priorities:
UPDATE flights SET base_priority = 2, effective_priority = 2 WHERE flight_number = 'AI-101';
UPDATE flights SET base_priority = 3, effective_priority = 3 WHERE flight_number = '6E-202';
UPDATE flights SET base_priority = 3, effective_priority = 3 WHERE flight_number = 'AI-303';
UPDATE flights SET base_priority = 4, effective_priority = 4 WHERE flight_number = 'SG-404';
UPDATE flights SET base_priority = 5, effective_priority = 5 WHERE flight_number = 'UK-505';

-- 4. Re-populate scheduling queue in Ready state
DELETE FROM scheduling_queue WHERE flight_id IN (
    SELECT id FROM flights WHERE flight_number IN ('AI-101', '6E-202', 'AI-303', 'SG-404', 'UK-505')
);

INSERT INTO scheduling_queue (flight_id, state, current_priority, wait_time_seconds, created_at)
SELECT id, 'Ready', base_priority, 0, NOW()
FROM flights
WHERE flight_number IN ('AI-101', '6E-202', 'AI-303', 'SG-404', 'UK-505')
ORDER BY base_priority ASC;

-- 5. Record autonomous audit log entry
INSERT INTO operation_logs (user_id, action, details, transaction_status, created_at)
VALUES (1, 'SYSTEM_RESET', 'Demonstration baseline state successfully re-established.', 'Committed', NOW());

COMMIT;
