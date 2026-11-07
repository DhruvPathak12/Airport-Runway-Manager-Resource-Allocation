-- =============================================================================
-- transactions.sql — Smart Air Traffic Control Management System
-- Database: smartatc
-- Phase 2 / Day 21: MySQL Transaction Implementation
--
-- This file documents and provides explicit, repeatable MySQL database
-- transactions for runway resource allocation and release.
--
-- DBMS Principles Demonstrated:
--   - ACID: Atomicity (all-or-nothing execution)
--   - Transaction Boundaries: START TRANSACTION ... COMMIT / ROLLBACK
--   - Row-Level Locking: SELECT ... FOR UPDATE on InnoDB tables
--   - Database Consistency: Synchronized states across runways, flights,
--     runway_assignments, and operation_logs
-- =============================================================================

USE smartatc;

-- =============================================================================
-- 1. TRANSACTION: ATOMIC RUNWAY RESOURCE ALLOCATION
-- =============================================================================
-- Context: ATC assigns Runway 1 ('09L') to Flight 10 ('AI-202').
-- All mutations (runway status, assignment record, flight status, audit log)
-- execute inside a single atomic transaction. If any precondition or update
-- fails, the entire transaction is rolled back with ROLLBACK.
-- =============================================================================

START TRANSACTION;

-- Step 1: Verify and lock the target runway row
-- Prevents concurrent transactions from reading or mutating runway state
SELECT runway_id, runway_number, runway_type, status
FROM runways
WHERE runway_id = 1
FOR UPDATE;

-- Step 2: Check for any existing active assignment on this runway
SELECT assignment_id, flight_id
FROM runway_assignments
WHERE runway_id = 1 AND status = 'Active'
FOR UPDATE;

-- Step 3: Verify and lock the target flight row
SELECT flight_id, flight_number, flight_type, operational_status
FROM flights
WHERE flight_id = 10
FOR UPDATE;

-- Step 4: Check that the flight does not already hold another active assignment
SELECT assignment_id, runway_id
FROM runway_assignments
WHERE flight_id = 10 AND status = 'Active'
FOR UPDATE;

-- Step 5: Verify simulation process state in scheduling_queue (if present)
SELECT process_state
FROM scheduling_queue
WHERE flight_id = 10
FOR UPDATE;

-- Step 6: Insert active assignment record into runway_assignments
INSERT INTO runway_assignments (
    flight_id,
    runway_id,
    assigned_by,
    assigned_at,
    status
) VALUES (
    10,             -- flight_id
    1,              -- runway_id
    1,              -- assigned_by (user_id of controller)
    NOW(),
    'Active'
);

-- Step 7: Update runway status to Occupied
UPDATE runways
SET status = 'Occupied'
WHERE runway_id = 1;

-- Step 8: Update flight operational status to Cleared (if Scheduled or Taxiing)
UPDATE flights
SET operational_status = 'Cleared'
WHERE flight_id = 10
  AND operational_status IN ('Scheduled', 'Taxiing');

-- Step 9: Insert immutable audit log record with transaction_status = 'Committed'
INSERT INTO operation_logs (
    user_id,
    operation_type,
    flight_id,
    runway_id,
    transaction_status,
    message,
    created_at
) VALUES (
    1,
    'RUNWAY_ASSIGN',
    10,
    1,
    'Committed',
    'Runway 09L allocated to flight AI-202 by atc_controller.',
    NOW()
);

-- Step 10: Commit atomic transaction to permanently persist all changes
COMMIT;


-- =============================================================================
-- 2. TRANSACTION: ATOMIC RUNWAY RESOURCE RELEASE
-- =============================================================================
-- Context: Controller or Airport Operator releases Runway 1 ('09L').
-- The active assignment is marked 'Released' with timestamp, the runway returns
-- to 'Available', and an immutable audit log is committed atomically.
-- =============================================================================

START TRANSACTION;

-- Step 1: Verify and lock target runway row
SELECT runway_id, runway_number, status
FROM runways
WHERE runway_id = 1
FOR UPDATE;

-- Step 2: Find and lock active assignment record
SELECT assignment_id, flight_id
FROM runway_assignments
WHERE runway_id = 1 AND status = 'Active'
FOR UPDATE;

-- Step 3: Update assignment record to Released with current timestamp
UPDATE runway_assignments
SET status = 'Released',
    released_at = NOW()
WHERE runway_id = 1
  AND status = 'Active';

-- Step 4: Return runway status to Available
UPDATE runways
SET status = 'Available'
WHERE runway_id = 1;

-- Step 5: Insert immutable audit log record with transaction_status = 'Committed'
INSERT INTO operation_logs (
    user_id,
    operation_type,
    flight_id,
    runway_id,
    transaction_status,
    message,
    created_at
) VALUES (
    1,
    'RUNWAY_RELEASE',
    10,
    1,
    'Committed',
    'Runway 09L released from flight AI-202 by atc_controller.',
    NOW()
);

-- Step 6: Commit atomic transaction
COMMIT;


-- =============================================================================
-- 3. TRANSACTION ROLLBACK REFERENCE SCENARIO
-- =============================================================================
-- When an intermediate conflict, constraint violation, or business rule failure
-- occurs (e.g. runway already Occupied, flight in terminal status, or duplicate
-- active assignment), the entire transaction aborts cleanly via ROLLBACK.
-- Zero orphaned rows remain, and no false audit log is committed.
-- =============================================================================

START TRANSACTION;

-- Lock and inspect runway
SELECT runway_id, runway_number, status
FROM runways
WHERE runway_id = 1
FOR UPDATE;

-- If status != 'Available' (e.g., 'Occupied' or 'Maintenance') or active assignment exists:
-- Abort transaction immediately. All previous locks are released and state is intact.
ROLLBACK;


-- =============================================================================
-- 4. VERIFICATION QUERIES FOR DBA / AUDIT INSPECTION
-- =============================================================================

-- Query 1: Verify that no orphaned active assignments exist without Occupied runway
SELECT ra.assignment_id, ra.runway_id, r.runway_number, r.status AS runway_status
FROM runway_assignments ra
JOIN runways r ON ra.runway_id = r.runway_id
WHERE ra.status = 'Active' AND r.status != 'Occupied';

-- Query 2: Verify that no Occupied runways exist without an active assignment
SELECT r.runway_id, r.runway_number, r.status
FROM runways r
LEFT JOIN runway_assignments ra ON r.runway_id = ra.runway_id AND ra.status = 'Active'
WHERE r.status = 'Occupied' AND ra.assignment_id IS NULL;

-- Query 3: Inspect recent transaction audit logs
SELECT log_id, user_id, operation_type, flight_id, runway_id, transaction_status, message, created_at
FROM operation_logs
WHERE operation_type IN ('RUNWAY_ASSIGN', 'RUNWAY_RELEASE')
ORDER BY log_id DESC
LIMIT 10;
