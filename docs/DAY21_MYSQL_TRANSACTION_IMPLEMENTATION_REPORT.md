# Day 21 — MySQL Transaction Implementation Report

## Phase
Phase 2 — Remaining 60% of OS and DBMS Implementation

## Date & Milestone
- **Milestone:** Phase 2 / Day 21 — MySQL Transaction Implementation
- **Status:** Complete & Verified
- **Starting Git Commit:** `fdbfeea` (`feat: implement concurrent runway allocation with C++ threads`)

---

## 1. Executive Summary & Objective

In Day 21 of the Smart Air Traffic Control Management System (SmartATC), we designed and implemented proper, atomic MySQL database transactions for **runway resource allocation** and **runway resource release**.

Prior to Day 21, runway allocation and release operations executed multiple standalone SQL statements without an enclosing transaction boundary. Although in-process state was protected against concurrent thread races by Day 19 `std::mutex` and `ScopedResourceLock`, database records across `runways`, `flights`, `runway_assignments`, and `operation_logs` lacked transaction-level atomicity. If an intermediate database query failed, earlier mutations remained committed in the database, risking partial state, orphaned assignments, or inaccurate audit logs.

Day 21 resolves this gap by introducing:
- **Transaction Boundaries:** Enclosing multi-statement allocation and release workflows into single atomic MySQL transactions via Qt SQL transaction APIs (`transaction()`, `commit()`, `rollback()`).
- **Row-Level Locking:** Executing `SELECT ... FOR UPDATE` on participating InnoDB rows (`runways`, `flights`, `runway_assignments`, `scheduling_queue`) to lock candidate records during transaction validation and prevent concurrency anomalies at the database tier.
- **Fail-Safe Rollback:** Guaranteed `db.rollback()` on any failure (intermediate query error, constraint violation, or commit rejection), coupled with synchronized in-memory restoration (`m_resourceManager->rollbackAllocationUnlocked(...)`).
- **InnoDB Storage Engine Enforcement:** Verifying that all 8 normalized database tables use `ENGINE=InnoDB` to support ACID transactions and row-level locks.
- **Repeatable SQL Transaction Reference:** Creating `sql/transactions.sql` with fully documented, production-ready transaction workflows for DBA review and audit verification.
- **Zero-Regression Test Suite:** Adding a dedicated 15-point automated test suite (`TestDatabaseTransactions.cpp`), bringing total CTest suites to 16 with 100% test passage.

---

## 2. Allocation & Release Behavior Before Day 21

Before Day 21, `ApplicationService::allocateRunway` and `ApplicationService::releaseRunwayResource` executed non-transactional sequences:

### Prior Allocation Flow:
1. Runway existence check: `SELECT runway_number, runway_type, status FROM runways WHERE runway_id = :id;`
2. Active runway assignment check: `SELECT ra.assignment_id, f.flight_number FROM runway_assignments ra ... WHERE ra.runway_id = :rid AND ra.status = 'Active';`
3. Flight status check: `SELECT flight_number, flight_type, operational_status FROM flights WHERE flight_id = :fid;`
4. Duplicate flight assignment check: `SELECT ra.assignment_id, r.runway_number FROM runway_assignments ra ... WHERE ra.flight_id = :fid AND ra.status = 'Active';`
5. Process queue state check: `SELECT process_state FROM scheduling_queue WHERE flight_id = :fid;`
6. Insert assignment: `INSERT INTO runway_assignments ...`
7. Update runway: `UPDATE runways SET status = 'Occupied' WHERE runway_id = :rid;`
8. Update flight: `UPDATE flights SET operational_status = 'Cleared' WHERE flight_id = :fid;`
9. Insert audit log: `INSERT INTO operation_logs (..., 'RUNWAY_ASSIGN', ...)`

### Vulnerabilities in Prior Flow:
- If Step 7 (runway update) or Step 8 (flight update) failed, Step 6 (`runway_assignments` record) remained permanently committed in MySQL as an orphaned active assignment.
- If Step 9 (audit logging) failed or threw, the runway remained occupied without an audit trail.
- Database records were read without row locks (`FOR UPDATE`), leaving a window between checking availability and inserting the assignment where another database client could allocate the same runway.

---

## 3. Transaction Architecture Introduced

Day 21 structures runway allocation and release around a strict **Logical Business Transaction Boundary**.

```
┌────────────────────────────────────────────────────────────────────────┐
│                   ApplicationService::allocateRunway                   │
├────────────────────────────────────────────────────────────────────────┤
│ 1. RBAC & Parameter Gate (Unauthenticated / Non-ATC rejected)          │
│ 2. ScopedResourceLock (std::mutex protects in-memory state)            │
│ 3. db.transaction() (MySQL START TRANSACTION)                         │
│ 4. SELECT ... FOR UPDATE (Row locks on runways, flights, assignments)  │
│ 5. Validate business invariants inside locked transaction scope        │
│ 6. INSERT INTO runway_assignments                                      │
│ 7. UPDATE runways SET status = 'Occupied'                              │
│ 8. UPDATE flights SET operational_status = 'Cleared'                   │
│ 9. INSERT INTO operation_logs (transaction_status = 'Committed')       │
│                                                                        │
│ [Any failure?] ────► db.rollback()                                    │
│                      m_resourceManager->rollbackAllocationUnlocked()   │
│                      Return granular AllocationResult failure code     │
│                                                                        │
│ [All succeed?] ────► db.commit()                                       │
│                      m_resourceManager->confirmAllocationUnlocked()    │
│                      Emit UI refresh signals                           │
│                      Return AllocationResult::Success                  │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Allocation Transaction Sequence (Step-by-Step)

The atomic allocation transaction enforces the following sequential flow on a single `QSqlDatabase` connection instance:

1. **Connection Validation:** Ensures `checkDatabaseConnection()` is valid and `db.isOpen()` is true.
2. **In-Process Mutex Guard:** Acquires `ResourceManager::ScopedResourceLock` to guard local thread concurrency.
3. **BEGIN TRANSACTION:** Invokes `db.transaction()`. If the transaction cannot start, logs an error and returns `AllocationResult::DatabaseError`.
4. **Lock Runway Row:**
   ```sql
   SELECT runway_number, runway_type, status FROM runways WHERE runway_id = :id FOR UPDATE;
   ```
   If not found, calls `db.rollback()` and returns `RunwayNotFound`. If status != 'Available', calls `db.rollback()` and returns `RunwayUnavailable`.
5. **Lock & Verify Active Runway Assignments:**
   ```sql
   SELECT ra.assignment_id, f.flight_number
   FROM runway_assignments ra
   LEFT JOIN flights f ON ra.flight_id = f.flight_id
   WHERE ra.runway_id = :rid AND ra.status = 'Active' FOR UPDATE;
   ```
   If an active assignment exists, calls `db.rollback()` and returns `ConflictExistingAssignment`.
6. **Lock & Verify Flight Eligibility:**
   ```sql
   SELECT flight_number, flight_type, operational_status FROM flights WHERE flight_id = :fid FOR UPDATE;
   ```
   If not found, calls `db.rollback()` and returns `FlightNotFound`.
   If flight status is terminal ('Landed', 'Departed', 'Cancelled'), calls `db.rollback()` and returns `FlightIneligible`.
   If runway operation type is incompatible (e.g., Departure on Landing-only runway), calls `db.rollback()` and returns `FlightIneligible`.
7. **Lock & Verify Active Flight Assignments:**
   ```sql
   SELECT ra.assignment_id, r.runway_number
   FROM runway_assignments ra
   LEFT JOIN runways r ON ra.runway_id = r.runway_id
   WHERE ra.flight_id = :fid AND ra.status = 'Active' FOR UPDATE;
   ```
   If flight already holds another runway, calls `db.rollback()` and returns `ConflictExistingAssignment`.
8. **Verify Simulation State:**
   ```sql
   SELECT process_state FROM scheduling_queue WHERE flight_id = :fid FOR UPDATE;
   ```
   If process state is 'Completed', calls `db.rollback()` and returns `FlightIneligible`.
9. **Insert Assignment Record:**
   ```sql
   INSERT INTO runway_assignments (flight_id, runway_id, assigned_by, assigned_at, status)
   VALUES (:fid, :rid, :uid, NOW(), 'Active');
   ```
   If insertion fails, calls `db.rollback()` and reverts in-memory state.
10. **Update Runway Status:**
    ```sql
    UPDATE runways SET status = 'Occupied' WHERE runway_id = :rid;
    ```
    If update fails, calls `db.rollback()` and reverts in-memory state.
11. **Update Flight Status:**
    ```sql
    UPDATE flights SET operational_status = 'Cleared' WHERE flight_id = :fid;
    ```
    If update fails, calls `db.rollback()` and reverts in-memory state.
12. **Insert Operation Log:**
    ```sql
    INSERT INTO operation_logs (user_id, operation_type, flight_id, runway_id, transaction_status, message, created_at)
    VALUES (:uid, 'RUNWAY_ASSIGN', :fid, :rid, 'Committed', :msg, NOW());
    ```
    If logging fails, calls `db.rollback()` and reverts in-memory state.
13. **Commit Transaction:**
    Invokes `db.commit()`. If commit fails, calls `db.rollback()` and reverts in-memory state.
14. **Confirm In-Memory State & Emit Signals:**
    Only after successful commit, `m_resourceManager->confirmAllocationUnlocked(runwayId, flightId)` is called, and `runwaysChanged()`, `flightsChanged()`, and `processesChanged()` are emitted.

---

## 5. Release Transaction Sequence (Step-by-Step)

The atomic runway release transaction guarantees that assignment closure and runway availability are committed together:

1. **Authentication & Authorization Gate:** Verifies role has `action:RunwayAllocation`, `action:RunwayMaintenance`, or `Admin`.
2. **In-Process Mutex Guard:** Acquires `ScopedResourceLock`.
3. **BEGIN TRANSACTION:** Invokes `db.transaction()`.
4. **Lock Runway Row:**
   ```sql
   SELECT runway_number, status FROM runways WHERE runway_id = :id FOR UPDATE;
   ```
   If runway does not exist, calls `db.rollback()` and returns false.
   If runway is in 'Maintenance', calls `db.rollback()` (maintenance runways must be toggled via maintenance controls).
5. **Lock & Retrieve Active Assignment:**
   ```sql
   SELECT ra.assignment_id, ra.flight_id, f.flight_number
   FROM runway_assignments ra
   LEFT JOIN flights f ON ra.flight_id = f.flight_id
   WHERE ra.runway_id = :rid AND ra.status = 'Active' FOR UPDATE;
   ```
6. **Update Assignment Record:**
   ```sql
   UPDATE runway_assignments SET status = 'Released', released_at = NOW() WHERE assignment_id = :aid;
   ```
   If update fails, calls `db.rollback()` and returns false.
7. **Update Runway Status to Available:**
   ```sql
   UPDATE runways SET status = 'Available' WHERE runway_id = :rid;
   ```
   If update fails, calls `db.rollback()` and returns false.
8. **Insert Operation Log:**
   ```sql
   INSERT INTO operation_logs (user_id, operation_type, flight_id, runway_id, transaction_status, message, created_at)
   VALUES (:uid, 'RUNWAY_RELEASE', :fid, :rid, 'Committed', :msg, NOW());
   ```
   If log insertion fails, calls `db.rollback()` and returns false.
9. **Commit Transaction:**
   Invokes `db.commit()`. If commit fails, calls `db.rollback()` and returns false.
10. **In-Memory Release & Signals:**
    Calls `m_resourceManager->releaseRunwayUnlocked(runwayId)` and emits `runwaysChanged()`, `flightsChanged()`, and `processesChanged()`.

---

## 6. Row-Level Locking Strategy (InnoDB)

### Why `SELECT ... FOR UPDATE`?
In standard MySQL default isolation levels (`REPEATABLE READ` or `READ COMMITTED`), standard `SELECT` statements use consistent non-locking reads (MVCC snapshots). This means two simultaneous transactions could both read `status = 'Available'`, conclude the runway is free, and both attempt to insert an assignment.

By appending `FOR UPDATE` to candidate selection queries:
1. InnoDB acquires an **exclusive row lock (X lock)** on the target row in the index tree.
2. Any concurrent transaction attempting to read the same row with `FOR UPDATE` or `LOCK IN SHARE MODE`, or attempting to execute an `UPDATE` or `DELETE`, is blocked until the first transaction commits or rolls back.
3. Once the lock is acquired, the transaction rechecks the live, committed status of the runway and flight, ensuring decision-making is grounded in live persistent state rather than stale snapshot data.

---

## 7. Storage Engine & Database Verification

All tables involved in the transactions were inspected in `sql/schema.sql` and verified for transaction support:

| Table Name | Storage Engine | Foreign Keys | Transactional |
|---|---|---|---|
| `roles` | InnoDB | None | Yes |
| `users` | InnoDB | roles | Yes |
| `aircraft` | InnoDB | None | Yes |
| `flights` | InnoDB | aircraft | Yes |
| `runways` | InnoDB | None | Yes |
| `scheduling_queue` | InnoDB | flights | Yes |
| `runway_assignments` | InnoDB | flights, runways, users | Yes |
| `operation_logs` | InnoDB | users, flights, runways | Yes |

All 8 application tables use `ENGINE=InnoDB` with `utf8mb4` encoding. No non-transactional storage engines (such as MyISAM or MEMORY) are used. No table migrations or schema reorganizations were needed.

---

## 8. In-Memory and Database Consistency Strategy

A primary requirement of Day 21 is harmonizing in-memory OS resource state (`ResourceManager`) with persistent DBMS state (`MySQL`):

### Two-Tier Synchronization Principle:
1. **In-Process Tier (`std::mutex` + `ScopedResourceLock`):**
   Protects local C++ memory structures (`m_activeAllocations`, `m_maintenanceRunways`, `m_csStatus`) against concurrent threads inside the application process.
2. **Persistent Tier (MySQL Transactions + `FOR UPDATE`):**
   Protects persistent table rows against concurrent transactions from any database connection or external process.

### Coordination Order:
- The mutex lock is acquired **before** entering the transaction.
- In-memory reservations are **NOT confirmed** until `db.commit()` succeeds.
- If the database transaction fails at any stage (validation, query error, constraint violation, or commit failure), `db.rollback()` is executed, followed by `m_resourceManager->rollbackAllocationUnlocked(...)`.
- This ensures that a database failure never leaves an orphaned reservation in memory, and an in-memory contention failure never leaves a spurious database transaction open.

---

## 9. Error Handling & Security

- **Parameterized Queries:** All SQL queries use `prepare()` and `bindValue()`. Zero SQL string concatenation of user or parameter inputs is performed.
- **Sanitized Errors:** `DatabaseManager::setSanitizedError()` filters database credentials (passwords) from all database error strings before propagating to logs or the GUI.
- **Explicit Commit Verification:** The application explicitly checks `db.commit()`. If commit fails, `db.rollback()` is immediately attempted, and a detailed sanitized error is returned to the user.
- **No False Logs:** The `operation_logs` entry is inserted with `transaction_status = 'Committed'` *inside* the transaction. If the transaction rolls back, MySQL removes the audit log row as part of atomic rollback, guaranteeing that no successful operation log exists for a failed transaction.

---

## 10. Automated Test Results

A dedicated test suite was implemented in `tests/TestDatabaseTransactions.cpp` and integrated into CMake/CTest.

### Test Categories in `TestDatabaseTransactions`:
1. `testDriverTransactionCapability`: Driver availability and `QSqlDriver::Transactions` support.
2. `testAllocationRequiresAuthentication`: Rejection of unauthenticated users (`AllocationResult::Unauthorized`).
3. `testAllocationRequiresATCRole`: RBAC enforcement (rejection of Operator / Viewer roles).
4. `testAllocationRejectsInvalidInputs`: Input boundary validation (zero / negative runway and flight IDs).
5. `testReleaseRequiresAuthentication`: Release rejected for unauthenticated users.
6. `testReleaseRequiresAuthorizedRole`: Release rejected for unauthorized roles.
7. `testReleaseRejectsInvalidRunwayId`: Release input boundary validation.
8. `testOfflineDatabaseHandling`: Graceful offline error reporting when MySQL is disconnected.
9. `testResourceManagerRollbackIntegrity`: Verification that `rollbackAllocation` cleanly restores in-memory resource mappings.
10. `testLiveAtomicAllocationSuccess`: Live test verifying atomic commit across all 4 tables.
11. `testLiveAtomicAllocationRollbackOnDuplicateRunway`: Live test verifying rollback on second runway request.
12. `testLiveAtomicAllocationRollbackOnTerminalFlight`: Live test verifying rollback on Landed flight.
13. `testLiveAtomicReleaseSuccess`: Live test verifying release commit, runway availability, and audit log.
14. `testLiveAtomicReleaseRollbackOnMaintenance`: Live test verifying maintenance runway cannot be released.
15. `testLiveSimulatedTransactionRollbackLeavesNoOrphanRecords`: Live test proving forced `db.rollback()` leaves 0 orphaned records.

### Complete CTest Suite Results:
```
Test project D:/Projects/Airport Runway Manager &Resource Allocation/SmartATC/build
      Start  1: TestDatabaseConnectivity
 1/16 Test  #1: TestDatabaseConnectivity ................   Passed    0.18 sec
      Start  2: TestAuthAndRBAC
 2/16 Test  #2: TestAuthAndRBAC .........................   Passed    0.60 sec
      Start  3: TestFlightManagement
 3/16 Test  #3: TestFlightManagement ....................   Passed    0.59 sec
      Start  4: TestAircraftManagement
 4/16 Test  #4: TestAircraftManagement ..................   Passed    0.61 sec
      Start  5: TestProcessModel
 5/16 Test  #5: TestProcessModel ........................   Passed    0.58 sec
      Start  6: TestReadyQueueAndProcessStates
 6/16 Test  #6: TestReadyQueueAndProcessStates ..........   Passed    0.61 sec
      Start  7: TestPriorityScheduler
 7/16 Test  #7: TestPriorityScheduler ...................   Passed    0.58 sec
      Start  8: TestSchedulingMetrics
 8/16 Test  #8: TestSchedulingMetrics ...................   Passed    0.60 sec
      Start  9: TestRunwayManagement
 9/16 Test  #9: TestRunwayManagement ....................   Passed    0.58 sec
      Start 10: TestRunwayResourceAllocation
10/16 Test #10: TestRunwayResourceAllocation ............   Passed    0.62 sec
      Start 11: TestDynamicEmergencyPriority
11/16 Test #11: TestDynamicEmergencyPriority ............   Passed    0.59 sec
      Start 12: TestPriorityAging
12/16 Test #12: TestPriorityAging .......................   Passed    0.61 sec
      Start 13: TestStarvationSimulation
13/16 Test #13: TestStarvationSimulation ................   Passed    0.59 sec
      Start 14: TestCriticalSectionAndMutualExclusion
14/16 Test #14: TestCriticalSectionAndMutualExclusion ...   Passed    0.61 sec
      Start 15: TestConcurrentRunwayAllocation
15/16 Test #15: TestConcurrentRunwayAllocation ..........   Passed    0.62 sec
      Start 16: TestDatabaseTransactions
16/16 Test #16: TestDatabaseTransactions ................   Passed    0.04 sec

100% tests passed out of 16

Total Test time (real) = 8.64 sec
```

---

## 11. Deployment Verification

The standalone runtime deployment script was executed:
```powershell
powershell -ExecutionPolicy Bypass -File scripts/deploy_runtime.ps1
```
- Copied Qt 6 Core, Gui, Widgets, Sql, Test DLLs.
- Copied MinGW runtime DLLs (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`).
- Copied Qt platform plugin (`platforms/qwindows.dll`).
- Copied Qt SQL driver plugins (`sqldrivers/qsqlmysql.dll`, `sqldrivers/qsqlite.dll`).
- Copied MySQL client library (`libmysql.dll`).
- Standalone execution of `build/SmartATC.exe` verified without requiring Qt Creator or custom PATH variables.

---

## 12. Known Limitations & Day 22 Readiness

### Current Boundaries (What was accomplished in Day 21):
- Atomic database transaction boundaries for allocation and release.
- Explicit `db.transaction()`, `db.commit()`, and `db.rollback()` handling.
- InnoDB row locking using `SELECT ... FOR UPDATE`.
- Synchronized in-memory rollback on failure.
- Complete 16-suite automated test coverage.

### Remaining for Day 22 (ACID Verification & Failure Matrices):
- Multi-client concurrent transaction contention under competing MySQL connections.
- Transaction isolation level evaluation (`READ COMMITTED` vs `REPEATABLE READ` vs `SERIALIZABLE`).
- Deadlock detection, timeout simulation, and automatic retry policies.
- Comprehensive ACID proof matrices documenting Atomicity, Consistency, Isolation, and Durability under simulated crash and disconnection scenarios.
