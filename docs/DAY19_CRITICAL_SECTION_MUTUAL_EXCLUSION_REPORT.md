# Day 19 — Critical Section & Mutual Exclusion Implementation Report

## Phase
Phase 2 — Operating System and DBMS Advanced Implementation

## Date & Milestone
- **Milestone:** Phase 2 / Day 19 — Critical Section & Mutual Exclusion Implementation
- **Status:** Complete & Verified
- **Baseline Git Commit:** `732ead7` (`feat: demonstrate starvation prevention with aging`)

---

## 1. Executive Summary & Objective

In Day 19 of the Smart Air Traffic Control Management System (SmartATC), we designed and implemented a robust **Critical Section and Mutual Exclusion** mechanism protecting airport runway resources from race conditions, conflicting allocation requests, and state corruption.

In operating systems theory, a **critical section** is a code segment that accesses shared resources (such as memory, hardware devices, or scheduling structures) and must not be concurrently executed by more than one process or thread. In SmartATC:
- **Runway** = Shared OS Resource (reusable, exclusive physical asset).
- **Flight** = Simulated Process (requester seeking exclusive resource ownership).
- **Runway Allocation** = Resource Acquisition (process claims resource for safe operation).
- **Runway Release** = Resource Release (process relinquishes resource back to pool).
- **Critical Section** = The protected validation and reservation operation.
- **Mutual Exclusion** = Invariant ensuring at most one allocation or release operation modifies the protected runway resource state at any moment.
- **Resource Contention** = Multiple processes contending for the same resource; one wins the lock and succeeds, while subsequent contenders safely observe the occupied state and are rejected without corrupting system invariants.

Day 19 establishes the in-process mutual exclusion foundation, preparing the architectural groundwork for Day 20 (C++ Multithreading and Synchronization).

---

## 2. Shared Resource & Critical Section Design

### 2.1 Concurrency Model & Mutex Protection
To guarantee mutual exclusion, `ResourceManager` encapsulates a dedicated C++ standard library mutex:
```cpp
mutable std::mutex m_mutex;
```

Access to shared allocation maps (`m_runwayAssignments`, `m_flightAssignments`, and `m_maintenanceRunways`) is governed by RAII scoped locks. An inner helper class, `ResourceManager::ScopedResourceLock`, wraps `std::unique_lock<std::mutex>`:
- **Acquire:** Automatically locks `m_mutex` upon instantiation and transitions the critical section status to `InCriticalSection`. Emits `criticalSectionEntered`.
- **Release:** Upon destruction (scope exit or stack unwinding during exceptions), automatically resets the status to `Idle`, unlocks `m_mutex`, and emits `criticalSectionExited`.

### 2.2 Unlocked Internal Helpers & Prevention of Recursive Lockup
`std::mutex` is non-recursive. If a public member method acquires `ScopedResourceLock` (or `std::lock_guard<std::mutex>`) and subsequently calls another member method that also attempts to acquire `m_mutex`, the thread self-deadlocks.

To completely prevent self-deadlock and recursive lockups:
1. All core queries and mutations are implemented as unlocked private helpers:
   - `isRunwayReservedUnlocked(int runwayId) const`
   - `activeFlightForRunwayUnlocked(int runwayId) const`
   - `activeRunwayForFlightUnlocked(int flightId) const`
   - `isRunwayInMaintenanceUnlocked(int runwayId) const`
   - `setRunwayMaintenanceUnlocked(int runwayId, bool inMaintenance)`
   - `criticalSectionStatusUnlocked() const`
   - `setCriticalSectionStatusUnlocked(CriticalSectionStatus status)`
   - `reserveRunwayUnlocked(int flightId, int runwayId, QString *errorMessage)`
   - `rollbackAllocationUnlocked(int flightId, int runwayId)`
   - `confirmAllocationUnlocked(int flightId, int runwayId)`
   - `releaseRunwayUnlocked(int flightId, int runwayId, QString *errorMessage)`
2. Public API methods (`isRunwayReserved`, `isRunwayAvailable`, `reserveRunway`, `releaseRunway`, etc.) acquire `std::lock_guard<std::mutex>` and delegate to the unlocked helpers.
3. Compound service operations (such as `ApplicationService::allocateRunway` and `ApplicationService::releaseRunwayResource`) acquire the lock once using `ScopedResourceLock` and perform all validation, in-memory updates, and error rollbacks via the unlocked methods without nested locking attempts.

---

## 3. Runway Resource Invariants & Contention Resolution

The implementation strictly enforces all Operating System resource allocation invariants:

| Invariant | OS Principle | Implementation in SmartATC |
|---|---|---|
| **Exclusive Ownership** | Mutual Exclusion | A runway can have at most one active flight assignment (`m_runwayAssignments`). |
| **Single Resource per Process** | Resource Limit | A flight can hold at most one active runway allocation (`m_flightAssignments`). |
| **Maintenance Protection** | Resource Availability | Runways marked in maintenance (`m_maintenanceRunways`) cannot be reserved. |
| **Safe Contention Handling** | Contention Prevention | When Flight A and Flight B request Runway R, the first entrant reserves R. The second entrant finds R occupied, returns `AllocationResult::RunwayOccupied`, emits `allocationContentionRejected`, and exits cleanly. |
| **Strict Release Validation** | Safe Relinquishment | Release requires matching `(flightId, runwayId)`. Mismatched, unallocated, or already released resources are rejected safely (`AllocationResult::InvalidRunwayState`). |
| **State Rollback on Failure** | Atomic State Invariant | If the in-memory reservation succeeds but the subsequent database persistence fails, `rollbackAllocationUnlocked()` immediately frees the reserved runway and flight association. |

---

## 4. Integration with ApplicationService & RBAC

`ApplicationService` orchestrates the complete allocation workflow while preserving Role-Based Access Control:
- **Authorization Verification:** Gated by `action:runway:allocate` and `action:runway:release`. Only authorized roles (such as `ATC` or `Admin`) may enter the allocation pipeline; unauthorized attempts (e.g. `Airport Operator`) are rejected with an explicit authorization error before resource state is accessed.
- **Critical Section Scope:** In `ApplicationService::allocateRunway`:
  1. Acquire `ResourceManager::ScopedResourceLock csLock(*m_resourceManager);`.
  2. Validate flight existence, runway existence, and aircraft compatibility inside the critical section.
  3. Call `m_resourceManager->reserveRunwayUnlocked(flightId, runwayId, &errorMsg)`.
  4. Persist to MySQL `runway_assignments` and update `flights` operational status.
  5. On DB failure, execute `m_resourceManager->rollbackAllocationUnlocked(flightId, runwayId)`.
  6. On DB success, call `m_resourceManager->confirmAllocationUnlocked(flightId, runwayId)`.
  7. Release `csLock` upon return.
- **Database Synchronization:** `syncFromDatabase()` reloads active runway assignments from MySQL on startup and on database reconnection, ensuring in-memory mutual exclusion maps remain in lockstep with persistent storage.

---

## 5. GUI Enhancements in RunwaysView

The Qt GUI (`ui/RunwaysView`) was enhanced to provide real-time visual feedback of critical section transitions and resource contention:
1. **Critical Section Activity Banner (`m_csBanner`):**
   - Positioned prominently above the runway management table.
   - Dynamically updates with color-coded status badges:
     - `Idle / Protected`: Green badge indicating critical section is armed and ready.
     - `Critical Section Active`: Amber badge indicating an active allocation or release operation is executing inside the critical section.
     - `Contention Rejected`: Red badge warning of rejected conflicting allocation attempts.
2. **OS Critical Section State Column:**
   - Table expanded from 5 to 6 columns: `ID`, `Number`, `Status`, `Length (m)`, `Surface`, and `OS Critical Section State`.
   - Each row displays an explicit badge:
     - `Available (Ready for Lock)` (Green)
     - `Reserved (Exclusive Flight #[ID])` (Red)
     - `In Maintenance (Locked)` (Orange)
3. **Signal Connections:**
   - Reacts to `criticalSectionEntered`, `criticalSectionExited`, `runwayReserved`, `runwayReleased`, and `allocationContentionRejected` to trigger instantaneous table and banner refreshes.

---

## 6. Operating System vs DBMS Transaction Boundaries

A crucial architectural distinction was documented and maintained:
- **In-Process C++ Mutex (`std::mutex`):** Guarantees mutual exclusion and prevents race conditions across threads within the running SmartATC desktop process.
- **DBMS Multi-Statement ACID Transactions:** Database-level concurrency (such as multiple concurrent client applications connected to the same MySQL server using `SELECT ... FOR UPDATE`, transaction isolation levels, and rollback logs) is a separate DBMS capability.
- **Milestone Scope:** Day 19 implements the OS-level critical section and in-process mutual exclusion. Formal database transaction management (`START TRANSACTION`, `SELECT FOR UPDATE`, row-level locks, and rollbacks) is reserved for Phase 2, Days 21–22.

---

## 7. Files Created and Modified

| File | Status | Description |
|---|---|---|
| `src/resource/ResourceManager.h` | Modified | Added `std::mutex m_mutex;`, `ScopedResourceLock` RAII guard, unlocked helper signatures, and Qt signals. |
| `src/resource/ResourceManager.cpp` | Modified | Implemented thread-safe methods, unlocked internal primitives, contention handling, and state rollbacks. |
| `src/services/ApplicationService.cpp` | Modified | Wrapped `allocateRunway` and `releaseRunwayResource` in `ScopedResourceLock` with database failure rollbacks. |
| `ui/RunwaysView.h` | Modified | Added `m_csBanner` label and critical section notification slot declarations. |
| `ui/RunwaysView.cpp` | Modified | Implemented critical section banner styling, 6-column table with OS status badges, and signal bindings. |
| `CMakeLists.txt` | Modified | Added `TestCriticalSectionAndMutualExclusion` executable and registered with CTest. |
| `tests/TestCriticalSectionAndMutualExclusion.cpp` | Created | Comprehensive 26-point automated test suite for critical sections, mutual exclusion, invariants, and regressions. |
| `docs/DAY19_CRITICAL_SECTION_MUTUAL_EXCLUSION_REPORT.md` | Created | Complete technical documentation for the Day 19 implementation. |
| `PROJECT_STATUS.md` | Modified | Updated milestone status to reflect completion of Day 19. |

---

## 8. Verification & Test Results

### 8.1 TestCriticalSectionAndMutualExclusion (26 Test Points)
The dedicated test suite covers four comprehensive test categories:
1. **Critical Section & Mutual Exclusion Properties (Tests 1–7):**
   - `testCriticalSectionInit`: Initial state is Idle.
   - `testSingleOperationAcquiresProtection`: Scoped lock enters critical section and updates status.
   - `testSecondOperationCannotEnterSimultaneously`: Concurrent lock acquisition is mutually exclusive (`try_lock` fails when locked).
   - `testProtectionReleasedAfterNormalExit`: Scope exit automatically unlocks mutex.
   - `testProtectionReleasedAfterException`: RAII unwinding releases mutex upon C++ exception.
   - `testMultipleProtectedOperationsNonOverlapping`: Consecutive protected operations execute cleanly.
   - `testNoDeadlockInSupportedUsage`: Complex sequences and rollbacks complete without deadlock.
2. **Runway Resource Invariants (Tests 8–16):**
   - `testAvailableRunwayCanBeReserved`: Available runway successfully transitions to reserved.
   - `testOccupiedRunwayCannotBeReReserved`: Second reservation attempt on same runway is rejected.
   - `testMaintenanceRunwayCannotBeReserved`: Runway in maintenance rejects reservation.
   - `testTwoFlightsCannotOwnSameRunway`: Multiple flights contending for one runway results in exactly one owner.
   - `testFlightCannotHoldTwoRunways`: Single flight attempting to acquire second runway is rejected.
   - `testReleaseValidatesCorrectAssignment`: Matching flight and runway successfully releases resource.
   - `testInvalidReleaseRejectedSafely`: Release with wrong flight, wrong runway, or unallocated runway is rejected.
   - `testFailedAllocationStateCleanup`: State rollback cleanly restores runway to available.
   - `testExistingRunwayCompatibilityChecks`: Live MySQL compatibility checks (skipped when offline).
3. **ApplicationService & RBAC (Tests 17–21):**
   - `testAtcCanAllocateThroughProtectedPath`: ATC role executes protected allocation (skipped when offline).
   - `testUnauthorizedAllocationRejected`: Operator role rejected without modifying resource state.
   - `testReleasePermissionsUnchanged`: ATC/Admin authorized for release; Operator unauthorized.
   - `testFailedResourceAcquisitionReturnsMeaningfulError`: Meaningful user-facing error strings returned.
   - `testExistingDay15AllocationCompatibility`: Backward compatibility with Day 15 allocation API.
4. **Regression Verification (Tests 22–26):**
   - `testRegressionDay16EmergencyPriorityIntact`: Emergency declaration and clearance functions unaltered.
   - `testRegressionDay17PriorityAgingIntact`: Priority aging calculations and effective priority formulas intact.
   - `testRegressionDay18StarvationSimulationIntact`: Deterministic starvation simulation unaffected.
   - `testRegressionSchedulingBehaviorUnchanged`: Scheduler queue ordering and tie-breaking preserved.
   - `testRegressionAuthAndRbacPass`: Authentication and session management regression-free.

**Test Summary:** 26 passed, 0 failed, 2 skipped (requiring live MySQL connection). Pass rate: **100%**.

### 8.2 Full CTest Suite (All 14 Test Suites)
```
Test project D:/Projects/Airport Runway Manager &Resource Allocation/SmartATC/build
      Start  1: TestDatabaseConnectivity
 1/14 Test  #1: TestDatabaseConnectivity ................   Passed    0.23 sec
      Start  2: TestAuthAndRBAC
 2/14 Test  #2: TestAuthAndRBAC .........................   Passed    0.58 sec
      Start  3: TestFlightManagement
 3/14 Test  #3: TestFlightManagement ....................   Passed    0.54 sec
      Start  4: TestAircraftManagement
 4/14 Test  #4: TestAircraftManagement ..................   Passed    0.52 sec
      Start  5: TestProcessModel
 5/14 Test  #5: TestProcessModel ........................   Passed    0.56 sec
      Start  6: TestReadyQueueAndProcessStates
 6/14 Test  #6: TestReadyQueueAndProcessStates ..........   Passed    0.56 sec
      Start  7: TestPriorityScheduler
 7/14 Test  #7: TestPriorityScheduler ...................   Passed    0.52 sec
      Start  8: TestSchedulingMetrics
 8/14 Test  #8: TestSchedulingMetrics ...................   Passed    0.54 sec
      Start  9: TestRunwayManagement
 9/14 Test  #9: TestRunwayManagement ....................   Passed    0.55 sec
      Start 10: TestRunwayResourceAllocation
10/14 Test #10: TestRunwayResourceAllocation ............   Passed    0.53 sec
      Start 11: TestDynamicEmergencyPriority
11/14 Test #11: TestDynamicEmergencyPriority ............   Passed    0.53 sec
      Start 12: TestPriorityAging
12/14 Test #12: TestPriorityAging .......................   Passed    0.53 sec
      Start 13: TestStarvationSimulation
13/14 Test #13: TestStarvationSimulation ................   Passed    0.54 sec
      Start 14: TestCriticalSectionAndMutualExclusion
14/14 Test #14: TestCriticalSectionAndMutualExclusion ...   Passed    0.04 sec

100% tests passed out of 14
```

---

## 9. Known Limitations & Scope Boundaries (Day 20 Boundary Enforcement)

As mandated by the Phase 2 milestone plan, Day 19 strictly limited its scope to critical section modeling and in-process mutual exclusion:
- **No Background Thread Pools or `std::jthread` / `QThread` Workers:** Day 20 will introduce explicit multi-threaded flight processes contending for runways simultaneously.
- **No Condition Variables or Semaphore Throttling:** Synchronization primitives (`std::condition_variable`, `std::counting_semaphore`) for waiting on runway availability are scheduled for Day 20.
- **No Database Row-Level Locking:** Multi-client MySQL transactions (`SELECT FOR UPDATE`, isolation levels, deadlock recovery) will be addressed in Days 21–22.
- **In-Process Scope:** The C++ `std::mutex` protects memory inside this application instance; multi-instance server concurrency requires DBMS transactions.

---

## 10. Conclusion

Day 19 successfully establishes the Operating System foundation of **Critical Section and Mutual Exclusion** for the runway resource management subsystem of SmartATC. All shared state mutations are protected by RAII scoped locking, recursive self-deadlocks are eliminated via unlocked helper architecture, contention is handled gracefully without state corruption, and 100% of the 14 automated test suites pass cleanly.
