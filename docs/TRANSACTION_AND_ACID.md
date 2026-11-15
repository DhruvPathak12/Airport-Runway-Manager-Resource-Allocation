# Database Transactions and ACID Verification — SmartATC

## 1. Overview of Database Transactions

A database transaction is an indivisible unit of work comprising multiple SQL operations. In SmartATC, resource allocation and release operations span multiple relational tables: `runways`, `flights`, `runway_assignments`, and `operation_logs` (with concurrency verification against `scheduling_queue`).

If any single statement or validation fails (e.g., duplicate assignment conflict, foreign key constraint violation, or simulated failure injection), the entire transaction is rolled back via `QSqlDatabase::rollback()` to guarantee that partial, corrupt, or phantom records are never persisted.

---

## 2. Exact Transaction Sequence in Source Code

The transaction workflows are implemented directly in `src/services/ApplicationService.cpp` within `allocateRunway()` and `releaseRunwayResource()`.

### 2.1 Runway Allocation Transaction Sequence (`allocateRunway`)

```mermaid
sequenceDiagram
    autonumber
    participant App as ApplicationService
    participant RM as ResourceManager (Memory)
    participant DB as MySQL InnoDB Engine
    participant Log as operation_logs Table

    App->>RM: ScopedResourceLock (std::mutex)
    App->>DB: db.transaction() (START TRANSACTION)
    
    Note over App,DB: 1. Record Locking & Validation (Pessimistic Isolation)
    App->>DB: SELECT runway_number, runway_type, status FROM runways WHERE runway_id = ? FOR UPDATE
    DB-->>App: Runway row locked (must be 'Available')
    App->>DB: SELECT assignment_id, flight_number FROM runway_assignments WHERE runway_id = ? AND status = 'Active' FOR UPDATE
    DB-->>App: Confirms no active assignment on this runway
    App->>DB: SELECT flight_number, flight_type, operational_status FROM flights WHERE flight_id = ? FOR UPDATE
    DB-->>App: Flight locked (must not be Landed, Departed, or Cancelled; type compatible)
    App->>DB: SELECT assignment_id FROM runway_assignments WHERE flight_id = ? AND status = 'Active' FOR UPDATE
    DB-->>App: Confirms flight does not already hold another active assignment
    App->>DB: SELECT process_state FROM scheduling_queue WHERE flight_id = ? FOR UPDATE
    DB-->>App: Process checked (must not be 'Completed')

    Note over App,DB: 2. Data Mutations Inside Transaction
    App->>DB: INSERT INTO runway_assignments (flight_id, runway_id, assigned_by, assigned_at, status) VALUES (?, ?, ?, NOW(), 'Active')
    App->>DB: UPDATE runways SET status = 'Occupied' WHERE runway_id = ?
    opt If flight is Scheduled or Taxiing
        App->>DB: UPDATE flights SET operational_status = 'Cleared' WHERE flight_id = ?
    end
    
    Note over App,Log: 3. Operation Audit Logging (INSIDE Transaction)
    App->>Log: INSERT INTO operation_logs (user_id, operation_type, flight_id, runway_id, transaction_status, message, created_at) VALUES (?, 'RUNWAY_ASSIGN', ?, ?, 'Committed', ?, NOW())

    Note over App,DB: 4. Atomic Commit & State Reconciliation
    alt All Operations Succeeded
        App->>DB: db.commit()
        DB-->>App: Changes Persisted, Row Locks Released
        App->>RM: m_resourceManager->confirmAllocationUnlocked(runwayId, flightId)
        Note over RM: In-memory runway set to Occupied
    else Any Validation / Query Failed or Simulated Failure Triggered
        App->>DB: db.rollback()
        DB-->>App: All Uncommitted Rows Discarded, Row Locks Released
        App->>RM: m_resourceManager->rollbackAllocationUnlocked(runwayId, flightId)
        Note over RM: In-memory runway reverted to Available
    end
```

> [!IMPORTANT]
> **Audit Logging Inside the Transaction:**
> `operation_logs` insertion is executed **inside the same database transaction boundary** with `transaction_status = 'Committed'`. If the transaction rolls back, this log entry is also discarded by the database engine, ensuring audit logs reflect true committed states. If a rollback occurs, a separate in-memory record is saved in `m_lastTx` for debugging and UI reporting.

### 2.2 Runway Release Transaction Sequence (`releaseRunwayResource`)

When an occupied runway is released:
1. **Critical Section Lock:** Acquires `ResourceManager::ScopedResourceLock` (`std::mutex`).
2. **Transaction Start:** Calls `db.transaction()`.
3. **Pessimistic Record Locking:**
   - Queries `runways` row with `FOR UPDATE`: validates runway exists and is not in `Maintenance`.
   - Queries `runway_assignments` with `FOR UPDATE` for any currently `Active` assignment on this runway.
4. **Data Mutations Inside Transaction:**
   - Updates active assignment: `UPDATE runway_assignments SET status = 'Released', released_at = NOW() WHERE assignment_id = ?;`
   - Updates runway status: `UPDATE runways SET status = 'Available' WHERE runway_id = ?;`
   - Records audit log: `INSERT INTO operation_logs (..., operation_type = 'RUNWAY_RELEASE', transaction_status = 'Committed', ...);`
5. **Commit / Rollback:**
   - On success: `db.commit()` commits all updates atomically; `ResourceManager` updates in-memory runway state to `Available`.
   - On failure: `db.rollback()` discards changes.

---

## 3. The ACID Properties in SmartATC

### 3.1 Atomicity ("All or Nothing")
- **Concept:** All database mutations within a transaction succeed together or are rolled back completely.
- **Implementation:** Both allocation and release encapsulate all queries between `db.transaction()` and `db.commit()`. If an error occurs during any step, `db.rollback()` executes immediately.
- **Failure Injection Testing:** Systematically verified in `TestACIDProperties` and `TestDatabaseTransactions` via `setAllocationFailureStage()`:
  - `BeforeInsertAssignment`: Rollback before any mutation.
  - `DuringInsertAssignment`: Rollback during assignment insertion.
  - `DuringUpdateRunway`: Rollback during runway status update.
  - `DuringUpdateFlight`: Rollback during flight status update.
  - `DuringInsertOperationLog`: Rollback during audit log insertion.
  - `DuringCommit`: Rollback when `commit()` fails.

### 3.2 Consistency ("Preserving Invariants")
- **Concept:** Transactions transition the database from one valid state satisfying all relational integrity constraints to another valid state.
- **SmartATC Invariants Verified:**
  1. *Uniqueness:* At most one `Active` assignment exists per runway.
  2. *Single Assignment:* A flight cannot hold two active runway assignments.
  3. *Flight Eligibility:* Terminal flights (`Landed`, `Departed`, `Cancelled`) cannot be assigned to runways.
  4. *Foreign Key Integrity:* All assignments strictly reference existing `flight_id` and `runway_id` keys.
  5. *Priority Range:* Priority values are constrained between 1 (Highest) and 5 (Lowest).

### 3.3 Isolation ("Concurrent Independence")
- **Concept:** Concurrently executing transactions cannot observe each other's intermediate uncommitted states.
- **Implementation:**
  - Storage Engine: **MySQL InnoDB** running at default **`REPEATABLE READ`** isolation.
  - Row Locking: Explicit **`SELECT ... FOR UPDATE`** acquires pessimistic exclusive write locks on affected runway and flight rows.
  - Concurrent Isolation Test: `TestACIDProperties::testIsolationConcurrentAllocationPessimisticLocking()` opens two distinct database connections (`SmartATCConnection` and a secondary connection). When Connection A locks a runway row for update, Connection B's attempt to allocate the same runway is blocked, preventing lost updates and race conditions.

### 3.4 Durability ("Survivability of Committed Data")
- **Concept:** Once a transaction commits, its modifications are permanently recorded in non-volatile storage and survive power loss, application crashes, or database server restarts.
- **Implementation:**
  - MySQL InnoDB relies on Write-Ahead Logging (WAL) and redo logs flushed to disk.
  - Verification Test: `TestACIDProperties::testDurabilityCommittedAllocationSurvivesServiceRestart()` executes an allocation, confirms commit, terminates `ApplicationService`, disconnects the database connection, reconstructs a new service instance, reconnects, and verifies that the allocation record persists intact.

---

## 4. In-Memory State Reconciliation

SmartATC coordinates volatile in-memory state with persistent database storage:
1. **Critical Section Guard:** `ResourceManager` holds an in-memory lock during the database transaction.
2. **On Commit Success:** Calls `ResourceManager::confirmAllocationUnlocked()`, which marks the in-memory runway as `Occupied` with the assigned flight ID and emits state-change signals.
3. **On Rollback / Failure:** Calls `ResourceManager::rollbackAllocationUnlocked()`, which reverts any provisional reservation, marks the runway as `Available`, and leaves in-memory queues consistent with MySQL.
