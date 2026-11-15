# SmartATC — Demonstration Reset & Re-Run Guide

## 1. Safety Notice & Reset Policy

> [!CAUTION]
> **DO NOT USE DESTRUCTIVE COMMANDS**
> Never execute `DROP DATABASE`, `DROP TABLE`, or destructive `TRUNCATE` operations on the `smartatc` database.
> The SmartATC demo environment preserves user accounts, aircraft registrations, runway infrastructure, and system configuration.

This reset guide is **non-destructive**, **idempotent**, and **predictable**. It cleanly resets operational states (flights, runway statuses, active assignments, and scheduling queues) back to the standard B.Tech evaluation baseline without wiping database schema or security credentials.

---

## 2. Pre-Reset State Inspection

Before resetting the system, inspect current database records to identify unreleased runways or pending processes:

```sql
-- Step 1: Check Current Runway Allocations & Statuses
SELECT runway_id, runway_number, status, runway_type 
FROM runways 
ORDER BY runway_id ASC;

-- Step 2: Check Active Runway Assignments
SELECT assignment_id, flight_id, runway_id, assigned_at, released_at, status 
FROM runway_assignments 
WHERE status = 'Active';

-- Step 3: Check Operational Status & Emergency Flags of Demo Flights
SELECT flight_id, flight_number, airline, operational_status, is_emergency, priority 
FROM flights 
WHERE flight_number IN ('AI-101', '6E-202', 'AI-303', 'SG-404', 'UK-505');

-- Step 4: Check Queued Scheduling Processes
SELECT queue_id, flight_id, process_state, base_priority, effective_priority, waiting_time 
FROM scheduling_queue 
ORDER BY queue_id ASC;
```

---

## 3. Idempotent Reset Script (`demo_reset.sql`)

Run the following SQL transaction in MySQL Workbench, MySQL CLI, or via `DatabaseManager`:

```sql
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

-- 4. Reset in-database scheduling queue state
DELETE FROM scheduling_queue WHERE flight_id IN (
    SELECT flight_id FROM flights WHERE flight_number IN ('AI-101', '6E-202', 'AI-303', 'SG-404', 'UK-505')
);

INSERT INTO scheduling_queue (flight_id, arrival_time, ready_since, base_priority, effective_priority, aging_level, service_time, process_state)
SELECT flight_id, NOW(), NOW(), priority, priority, 0, 60, 'Ready'
FROM flights
WHERE flight_number IN ('AI-101', '6E-202', 'AI-303', 'SG-404', 'UK-505')
ORDER BY priority ASC, flight_id ASC;

-- 5. Record autonomous audit log entry for the reset operation
INSERT INTO operation_logs (user_id, operation_type, transaction_status, message, created_at)
VALUES (
    (SELECT user_id FROM users WHERE username = 'admin_demo' LIMIT 1),
    'SYSTEM_RESET',
    'Committed',
    'Demonstration baseline state successfully re-established.',
    NOW()
);

COMMIT;
```

---

## 4. Post-Reset State Verification

Execute this query block to verify that the system has successfully returned to the demonstration baseline:

```sql
-- Verification Query
SELECT 
    'Runways' AS Entity,
    COUNT(*) AS TotalCount,
    SUM(CASE WHEN status = 'Available' THEN 1 ELSE 0 END) AS AvailableCount,
    SUM(CASE WHEN status = 'Occupied' THEN 1 ELSE 0 END) AS OccupiedCount,
    SUM(CASE WHEN status = 'Maintenance' THEN 1 ELSE 0 END) AS MaintenanceCount
FROM runways
UNION ALL
SELECT 
    'Active Assignments' AS Entity,
    COUNT(*),
    0, 0, 0
FROM runway_assignments WHERE status = 'Active'
UNION ALL
SELECT 
    'Demo Flights' AS Entity,
    COUNT(*),
    SUM(CASE WHEN is_emergency = 1 THEN 1 ELSE 0 END) AS EmergencyCount,
    SUM(CASE WHEN operational_status = 'Scheduled' THEN 1 ELSE 0 END) AS ScheduledCount,
    0
FROM flights WHERE flight_number IN ('AI-101', '6E-202', 'AI-303', 'SG-404', 'UK-505');
```

**Target Validation Matrix:**
| Entity | TotalCount | AvailableCount / EmergencyCount | OccupiedCount / ScheduledCount | MaintenanceCount |
|---|---|---|---|---|
| **Runways** | 3 | 2 (09L, 09R) | 0 (No occupied runways) | 1 (Runway 27) |
| **Active Assignments** | 0 | 0 | 0 | 0 |
| **Demo Flights** | 5 | 0 (Zero emergencies) | 5 (All Scheduled) | 0 |

---

## 5. Application In-Memory State Synchronization

Because SmartATC maintains an active in-memory cache and state manager (`ResourceManager`, `PriorityScheduler`, `ReadyQueue`):

1. **If SmartATC GUI is currently running during SQL reset:**
   - Click the **Refresh** button on the Top Navigation Bar or Dashboard.
   - Alternatively, press `Ctrl+R` to trigger `ApplicationService::refreshAllData()`.
   - The UI tables and monitoring gauges will reload the MySQL state immediately.
2. **Recommended Clean Start for Official Evaluation:**
   - Close the `SmartATC.exe` application window.
   - Run the reset SQL script above.
   - Launch `SmartATC.exe` fresh from the build directory:
     ```powershell
     cd "SmartATC\build"
     .\SmartATC.exe
     ```
   - All in-memory data structures (`ReadyQueue`, `ResourceManager`) will cleanly hydrate directly from the fresh MySQL baseline state upon startup.
