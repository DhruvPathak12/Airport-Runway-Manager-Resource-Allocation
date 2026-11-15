# Runway Resource Allocation Specification — SmartATC

## 1. The Runway as a Shared OS Resource

In an operating system, hardware resources like dedicated communication channels or exclusive peripheral devices can only service one task at a time. In SmartATC, the **Airport Runway** represents this shared resource.

### Operational Constraints:
1. **Physical Exclusivity:** A runway can accommodate only one aircraft movement at a time.
2. **Safety Criticality:** Resource allocation must be atomic; any ambiguity in assignment status could allow two flights to enter the active runway simultaneously.
3. **Operational Specialization:** Runways possess designated capabilities optimized for specific flight operations (`Landing`, `Takeoff`, or `Both`).

---

## 2. Runway States & Compatibility Matrix

### 2.1 Runway Operational Statuses
Every runway in SmartATC exists in one of three states:
- **`Available`:** The runway is clear of traffic, operational, and ready for immediate allocation.
- **`Occupied`:** An aircraft is actively landing, rolling out, or executing a takeoff run. The runway is an active critical section.
- **`Maintenance`:** The surface is closed for inspection, snow clearance, or repairs. All allocation requests are rejected.

### 2.2 Compatibility Matrix
`ApplicationService::findCompatibleAvailableRunway()` matches flight operation requirements against runway capabilities:

| Flight Operation | Runway Type: `Landing` | Runway Type: `Takeoff` | Runway Type: `Both` |
|---|:---:|:---:|:---:|
| **`Arrival`** | **Compatible** | Incompatible | **Compatible** |
| **`Departure`** | Incompatible | **Compatible** | **Compatible** |

---

## 3. Allocation Eligibility & Dual-Layer State Management

To prevent stale allocations, SmartATC enforces state synchronization across two layers:
1. **In-Memory Layer (`ResourceManager`):** Uses C++ `std::mutex` and map lookups for microsecond-level conflict rejection and multi-threaded synchronization.
2. **Database Layer (`MySQL InnoDB`):** Uses relational tables (`runways`, `runway_assignments`, `flights`) with pessimistic row-level locks for persistent ACID reliability.

### 3.1 Eligibility Criteria:
A flight is eligible for runway allocation if and only if:
1. It is not in a terminal state (`Landed`, `Departed`, `Cancelled`).
2. It does not already possess an active runway assignment.
3. A compatible runway exists that is currently `Available` in MySQL and not reserved in `ResourceManager`.

---

## 4. Runway Allocation Flowchart

```mermaid
flowchart TD
    Start(["Runway Allocation Requested (flightId, runwayId)"]) --> CheckAuth{"User Authenticated & Role Authorized?"}
    CheckAuth -- No --> RejectAuth["Reject: Access Denied"]
    CheckAuth -- Yes --> CheckInMem{"Is Runway Reserved in ResourceManager?"}
    CheckInMem -- Yes --> RejectContention["Reject: Resource Contention (Runway Occupied)"]
    CheckInMem -- No --> CheckMaint{"Is Runway Flagged Maintenance?"}
    CheckMaint -- Yes --> RejectMaint["Reject: Runway Under Maintenance"]
    CheckMaint -- No --> StartTx["Start MySQL Database Transaction: db.transaction()"]
    
    StartTx --> RowLock["SELECT runway & flight FOR UPDATE (Exclusive Lock)"]
    RowLock --> VerifyDB{"Is Runway Still 'Available' in DB?"}
    VerifyDB -- No --> RollbackBusy["Rollback: Runway Occupied in Database"]
    VerifyDB -- Yes --> CheckFlight{"Is Flight Eligible (Scheduled/Taxiing)?"}
    CheckFlight -- No --> RollbackFlight["Rollback: Flight Ineligible or Terminal"]
    CheckFlight -- Yes --> InsertAssign["1. INSERT INTO runway_assignments (Status: 'Active')"]
    InsertAssign --> UpdateRW["2. UPDATE runways SET status = 'Occupied'"]
    UpdateRW --> UpdateFLT["3. UPDATE flights SET operational_status = 'Cleared'"]
    UpdateFLT --> InsertLog["4. INSERT INTO operation_logs (Transaction: 'Committed')"]
    InsertLog --> Commit["COMMIT Database Transaction: db.commit()"]
    
    Commit --> SyncInMem["Confirm in ResourceManager: Set In-Memory Occupied"]
    SyncInMem --> Success(["Allocation Succeeded: Flight Cleared for Runway"])
    
    RollbackBusy --> RollbackCleanup["Rollback Transaction & Revert In-Memory State"]
    RollbackFlight --> RollbackCleanup
    RollbackCleanup --> Failure(["Allocation Failed"])
```

---

## 5. Unavailability Handling Policy

When `scheduleAndAllocateNext()` executes, it may encounter a scenario where the highest-priority flight in the `ReadyQueue` has **no compatible available runway**:

1. **State Preservation:** The process **remains in the `ReadyQueue`** in `ProcessState::Ready`. It is not dropped, canceled, or marked failed.
2. **Service Protection:** The process is not dispatched to `Running`.
3. **Continuous Aging:** The flight continues accumulating elapsed waiting time in `Ready` state, ensuring its priority continues to age.
4. **Non-Blocking Operation:** The system reports `"No compatible available runway found"`, allowing the controller to resolve runway congestion or wait for a runway to be released.

---

## 6. Runway Release Lifecycle

When an aircraft completes its landing rollout or departure:
1. **Trigger:** An authorized operator triggers `ApplicationService::releaseRunway(runwayId)`.
2. **Transaction Sequence:**
   - Active assignment record in `runway_assignments` is updated to `status = 'Released'`, and `released_at = NOW()`.
   - `runways.status` is reset to `'Available'`.
   - An audit log is inserted into `operation_logs` (`operation_type = 'RUNWAY_RELEASE'`, `transaction_status = 'Committed'`).
   - `db.commit()` executes atomically.
3. **In-Memory Clearance:** `ResourceManager::releaseRunway()` unlocks the runway mutex, clears internal ownership maps, and emits `runwaysChanged()`.
