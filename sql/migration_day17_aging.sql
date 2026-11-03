-- =============================================================================
-- SmartATC Database Migration — Day 17: Priority Aging Support
-- =============================================================================
-- Migration: Add ready_since and aging_level columns to scheduling_queue table.
-- Safety: Repeatable and safe for existing records.
-- =============================================================================

USE smartatc;

-- 1. Add ready_since column if not present
ALTER TABLE scheduling_queue
    ADD COLUMN IF NOT EXISTS ready_since DATETIME NULL DEFAULT NULL
        COMMENT 'Timestamp when process entered continuous READY waiting period'
        AFTER arrival_time;

-- 2. Add aging_level column if not present
ALTER TABLE scheduling_queue
    ADD COLUMN IF NOT EXISTS aging_level TINYINT UNSIGNED NOT NULL DEFAULT 0
        COMMENT 'Number of completed aging intervals accrued in READY state'
        AFTER effective_priority;

-- 3. Backfill ready_since for existing READY processes where ready_since is NULL
UPDATE scheduling_queue
SET ready_since = arrival_time
WHERE ready_since IS NULL AND process_state = 'Ready';

