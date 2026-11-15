# SmartATC — MySQL Transactions & ACID Properties Viva Preparation Guide

## 1. What is a Database Transaction?

A **Transaction** is a logical unit of database processing that includes one or more database access operations (read, insert, update, delete). To ensure the integrity of the database, all operations within a transaction must be treated as an atomic whole: either all operations execute to completion, or the database is left completely unchanged.

### 1.1 Why Do We Need Transactions in SmartATC?
Allocating a runway is not a simple isolated query; it involves updating four interrelated tables:
1. `runway_assignments` (Record active resource allocation)
2. `runways` (Change status from `Available` to `Occupied`)
3. `flights` (Advance operational status from `Scheduled` to `Cleared`)
4. `operation_logs` (Record immutable audit journal entry)

If these statements were executed independently without transactions, a sudden network disconnection, database constraint failure, or crash between Statement 2 and Statement 3 would leave the database in a corrupted state (e.g. the runway is marked Occupied, but the flight was never cleared and no assignment record exists). Wrapping all four operations in a transaction guarantees state consistency.

---

## 2. Step-by-Step Transaction Execution Sequence

The exact sequence implemented in `ApplicationService::allocateRunway()` (`src/services/ApplicationService.cpp`):

```
1. START TRANSACTION (db.transaction())
   │
   ▼
2. PESSIMISTIC ROW LOCKING (Acquires exclusive X-locks):
   SELECT runway_number, runway_type, status FROM runways 
   WHERE runway_id = :id FOR UPDATE;
   SELECT operational_status FROM flights 
   WHERE flight_id = :id FOR UPDATE;
   │
   ▼
3. BUSINESS VALIDATION UNDER ROW LOCK:
   - Verify runway exists and status == 'Available'.
   - Verify flight exists, is not in terminal status ('Landed','Departed','Cancelled'),
     and is operationally compatible (e.g. Departure flight on Takeoff runway).
   - If validation fails: db.rollback() -> Return error (Type A Rejection).
   │
   ▼
4. MULTI-TABLE WRITE MUTATIONS:
   a. INSERT INTO runway_assignments (flight_id, runway_id, assigned_by, assigned_at, status)
      VALUES (:fid, :rid, :uid, NOW(), 'Active');
   b. UPDATE runways SET status = 'Occupied' WHERE runway_id = :rid;
   c. UPDATE flights SET operational_status = 'Cleared' WHERE flight_id = :fid;
   d. INSERT INTO operation_logs (user_id, operation_type, flight_id, runway_id, transaction_status, message, created_at)
      VALUES (:uid, 'RUNWAY_ASSIGN', :fid, :rid, 'Committed', :msg, NOW());
   │
   ▼
5. TRANSACTION COMPLETION:
   - If all statements succeed: db.commit() -> Flushes redo log, releases row locks.
   - If any statement or failure-injection hook triggers an error:
     db.rollback() -> InnoDB undo logs revert all intermediate writes (Type B Rollback).
   │
   ▼
6. IN-MEMORY SYNCHRONIZATION:
   - On commit: confirm in-memory ResourceManager reservation.
   - On rollback: revert in-memory ResourceManager reservation.
```

---

## 3. The 4 ACID Properties in SmartATC

| Property | Theoretical Definition | SmartATC Implementation & Exact Scope of Verification |
|---|---|---|
| **Atomicity** | "All or nothing" — all operations succeed together or all are undone. | **Verified via Failure Injection (Type B):** Using test failure injection (`setAllocationFailureStage(DuringUpdateFlight)`), an artificial error was introduced after inserting the assignment and updating the runway. Calling `db.rollback()` caused MySQL's InnoDB undo logs to erase the inserted assignment and restore the runway status to `Available`. Zero partial records persisted. |
| **Consistency** | The database transitions from one valid state to another, satisfying all constraints, schema types, and foreign keys. | **Verified via Schema Constraints & Foreign Keys:** Enforced via `fk_ra_flight_id`, `fk_ra_runway_id`, status enums (`Available`, `Occupied`, `Maintenance`), and `chk_flights_priority` (1–5). If an invalid foreign key is passed, MySQL rejects the insert, aborting the transaction and maintaining valid state. |
| **Isolation** | Uncommitted transactions are invisible to others; concurrent transactions do not interfere destructively. | **Verified via Pessimistic Row Locking (`FOR UPDATE`):** SmartATC runs under MySQL InnoDB's default `REPEATABLE READ` transaction isolation level. Executing `SELECT ... FOR UPDATE` acquires exclusive X-locks on target runway rows, forcing concurrent transactions on the same runway to block until the active transaction commits or rolls back. |
| **Durability** | Once committed, changes persist permanently and survive system restarts and failures. | **Verified via Application Restart:** Once an allocation committed, the `SmartATC.exe` application process was terminated and relaunched. Upon reconnection, MySQL returned the committed status (`Occupied`), confirming that InnoDB write-ahead redo logs committed the data to non-volatile disk. *(Note: This verifies application-level persistence across restarts. Server crash-recovery relies on InnoDB redo logs and doublewrite buffer).* |

---

## 4. Academic Distinction: Validation Rejection vs. Genuine Rollback

In a viva examination, examiners frequently test whether students understand the difference between:

1. **Type A: Validation Rejection (Pre-Mutation):**
   - The user requests Runway 09L, but 09L is already `Occupied` or in `Maintenance`.
   - The transaction begins, reads the status via `SELECT ... FOR UPDATE`, detects incompatibility, and aborts immediately.
   - **No write operations ever occurred in MySQL.**
2. **Type B: Genuine Transaction Rollback (Post-Mutation):**
   - The user requests an available runway.
   - Step 1 executes: `runway_assignments` record is inserted.
   - Step 2 executes: `runways.status` is updated to `'Occupied'`.
   - Step 3 encounters an unexpected error (e.g., flight status update constraint violation or synthetic failure injection).
   - `db.rollback()` is invoked.
   - **Crucial Demonstration:** MySQL's InnoDB storage engine consults its **undo logs** to physically roll back the inserted assignment and revert the runway occupancy update.
   - Proves true relational **Atomicity**.

---

## 5. Key DBMS Concurrency Concepts Explained

### 5.1 What is a Dirty Read?
A **Dirty Read** occurs when Transaction A reads data that has been modified by Transaction B, but Transaction B has **not yet committed**. If Transaction B rolls back, Transaction A was operating on phantom data that never legally existed.
- *How SmartATC Prevents It:* MySQL InnoDB's default `REPEATABLE READ` isolation level uses Multi-Version Concurrency Control (MVCC) and read views, making dirty reads impossible.

### 5.2 What is a Lost Update?
A **Lost Update** occurs when two concurrent transactions read the same initial record and then both calculate and write back updates. The second transaction overwrites the changes made by the first without considering them.
- *How SmartATC Prevents It:* SmartATC uses explicit pessimistic locking with `SELECT ... FOR UPDATE`. When Transaction 1 reads Runway 09L, it acquires an exclusive X-lock. Transaction 2 attempting to read Runway 09L `FOR UPDATE` is forced to wait, completely serializing execution and eliminating lost updates.

### 5.3 What is a Deadlock?
A **Deadlock** is a circular wait condition where Transaction 1 holds Lock A and requests Lock B, while Transaction 2 holds Lock B and requests Lock A. Neither transaction can proceed.
- *How SmartATC Prevents Deadlock:* We enforce a strict linear lock hierarchy in `ApplicationService`:
  1. Always lock `runways` row first.
  2. Then lock `flights` row second.
  Because both resources are always acquired in the exact same deterministic order, circular wait condition is mathematically impossible.

### 5.4 Why InnoDB Instead of MyISAM?
- **MyISAM:** Legacy MySQL storage engine. Lacks transaction support (`COMMIT` / `ROLLBACK`), lacks crash recovery, and uses coarse table-level locking (locking the entire table even for single-row updates).
- **InnoDB:** Modern ACID-compliant storage engine. Supports row-level locking, foreign keys, MVCC, redo logs for durability, and undo logs for atomic rollbacks.
