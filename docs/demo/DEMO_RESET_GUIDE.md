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
SELECT id, runway_code, status, runway_type 
FROM runways 
ORDER BY id ASC;

-- Step 2: Check Active Runway Assignments
SELECT id, flight_id, runway_id, assigned_at, released_at, status 
FROM runway_assignments 
WHERE status = 'Active';

-- Step 3: Check Operational Status & Emergency Flags of Demo Flights
SELECT id, flight_number, airline, operational_status, emergency_flag, base_priority, effective_priority 
FROM flights 
WHERE flight_number IN ('AI-101', '6E-202', 'AI-303', 'SG-404', 'UK-505');

-- Step 4: Check Queued Scheduling Processes
SELECT id, flight_id, state, current_priority, wait_time_seconds 
FROM scheduling_queue 
ORDER BY id ASC;
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
WHERE id IN (1, 2);

UPDATE runways
SET status = 'Maintenance'
WHERE id = 3;

-- 3. Reset demo flights to baseline Scheduled / Queued states
-- Clear emergency flags and reset effective priorities to base priorities
UPDATE flights
SET operational_status = 'Scheduled',
    emergency_flag = 0,
    effective_priority = base_priority
WHERE flight_number IN ('AI-101', '6E-202', 'AI-303', 'SG-404', 'UK-505');

-- Specifically set AI-303 to base priority 3 (Normal) and not emergency
UPDATE flights
SET base_priority = 3,
    effective_priority = 3,
    emergency_flag = 0
WHERE flight_number = 'AI-303';

-- Specifically ensure baseline priorities:
-- AI-101 (Priority 2)
UPDATE flights SET base_priority = 2, effective_priority = 2 WHERE flight_number = 'AI-101';
-- 6E-202 (Priority 3)
UPDATE flights SET base_priority = 3, effective_priority = 3 WHERE flight_number = '6E-202';
-- SG-404 (Priority 4)
UPDATE flights SET base_priority = 4, effective_priority = 4 WHERE flight_number = 'SG-404';
-- UK-505 (Priority 5)
UPDATE flights SET base_priority = 5, effective_priority = 5 WHERE flight_number = 'UK-505';

-- 4. Reset in-database scheduling queue state
-- Delete temporary transient demo queues or reset process states to Ready
DELETE FROM scheduling_queue WHERE flight_id IN (
    SELECT id FROM flights WHERE flight_number IN ('AI-101', '6E-202', 'AI-303', 'SG-404', 'UK-505')
);

INSERT INTO scheduling_queue (flight_id, state, current_priority, wait_time_seconds, created_at)
SELECT id, 'Ready', base_priority, 0, NOW()
FROM flights
WHERE flight_number IN ('AI-101', '6E-202', 'AI-303', 'SG-404', 'UK-505')
ORDER BY base_priority ASC;

-- 5. Record autonomous audit log entry for the reset operation
INSERT INTO operation_logs (user_id, action, details, transaction_status, created_at)
VALUES (1, 'SYSTEM_RESET', 'Demonstration baseline state successfully re-established.', 'Committed', NOW());

COMMIT;
```

---

## 4. Post-Reset State Verification

Execute this query block to verify that the system has successfully returned to the golden demonstration baseline:

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
    SUM(CASE WHEN emergency_flag = 1 THEN 1 ELSE 0 END) AS EmergencyCount,
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
   - Launch `SmartATC.exe` fresh from the build directory or terminal:
     ```powershell
     cd "d:\Projects\Airport Runway Manager &Resource Allocation\SmartATC\build"
     .\SmartATC.exe
     ```
   - All in-memory data structures (`ReadyQueue`, `ResourceManager`) will cleanly hydrate directly from the fresh MySQL baseline state upon startup.

---

## 6. Fast Reset CLI Script (PowerShell Automation)

For rapid reset between practice runs without opening MySQL Workbench:

```powershell
# Run from repository root in PowerShell
mysql -u root -p smartatc -e "source SmartATC/docs/demo/demo_reset.sql"
```

*(Enter the MySQL root password when prompted. The reset executes in < 0.2 seconds).*
