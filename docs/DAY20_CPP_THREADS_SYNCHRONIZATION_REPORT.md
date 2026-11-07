# Day 20 — C++ Threads & Synchronization Implementation Report

## Phase
Phase 2 — Operating System and DBMS Advanced Implementation

## Date & Milestone
- **Milestone:** Phase 2 / Day 20 — C++ Threads & Synchronization Implementation
- **Status:** Complete & Verified
- **Starting Git Commit:** `e21fd2a` (`feat: implement runway critical section and mutual exclusion`)

---

## 1. Executive Summary & Objective

In Day 20 of the Smart Air Traffic Control Management System (SmartATC), we designed and implemented a production-grade, controlled multithreading and synchronization architecture using standard C++17 threading primitives.

Building upon Day 19's in-process Critical Section and Mutual Exclusion implementation inside `ResourceManager`, Day 20 introduces actual concurrent C++ worker threads (`std::thread`) that compete simultaneously for shared airport runway resources. The implementation proves that mutual exclusion prevents race conditions, guarantees that no two flights can successfully occupy the same runway simultaneously, manages thread lifecycles deterministically without detached threads or memory leaks, and delivers live progress to the Qt GUI via thread-safe queued signal dispatching without freezing the user interface.

---

## 2. Threading Architecture & Lifecycle Management

### 2.1 Concurrency Model
The system uses standard C++17 threading facilities:
- `std::thread`: Represents concurrent worker threads simulating simultaneous flight allocation requests.
- `std::mutex`: Central synchronization mechanism protecting shared runway allocation state in `ResourceManager`, and protecting result collection in `ConcurrentAllocationDemo`.
- `std::unique_lock<std::mutex>` / `std::lock_guard<std::mutex>`: RAII scoped lock managers ensuring deterministic lock release and exception safety.
- `std::condition_variable`: Implements a start barrier coordinating all worker threads to launch their resource acquisition requests simultaneously.
- `std::atomic<int>` & `std::atomic<bool>`: Atomic flags (`m_readyCount`, `m_startFlag`, `m_abortFlag`, `m_isRunning`) for lock-free state synchronization and cancellation.

### 2.2 Worker Thread Lifecycle
Worker threads follow a strictly managed lifecycle:
1. **Creation:** Worker threads are created inside `ConcurrentAllocationDemo::execute()` with dedicated, value-copied `ConcurrentAllocationDemo::Request` objects, avoiding dangling references.
2. **Start Barrier Waiting:** Each thread records its native system thread ID (`std::this_thread::get_id()`), increments the atomic ready counter (`m_readyCount`), notifies the coordinator, and awaits simultaneous release via `m_startCv.wait()`.
3. **Execution under Critical Section:** Once released, each thread transitions to `"Requesting Resource"` and invokes `ResourceManager::allocateRunway(runwayId, flightId, &outError)`.
4. **Exception Containment:** Every worker thread wraps its execution in comprehensive `try ... catch (const std::exception &ex)` and `catch (...)` handlers, recording failure messages into its `ThreadResult` without ever allowing an uncaught exception to escape and trigger `std::terminate`.
5. **Deterministic Joining:** Every started thread is explicitly joined via `t.join()` in the coordinator before the demonstration completes or before `ConcurrentAllocationDemo` is destroyed.
6. **Zero Detached Threads:** `std::thread::detach()` is never invoked. The coordinator joins all worker threads, ensuring zero unmanaged background threads or thread leaks.

---

## 3. Condition Variable & Start Barrier Synchronization

To avoid "faking" concurrency through sequential calls or arbitrary `sleep()` invocations, a `std::condition_variable` start barrier coordinates simultaneous contention:
```cpp
// 1. Worker threads register readiness and wait:
{
    std::unique_lock<std::mutex> lk(m_startMutex);
    m_readyCount++;
    m_startCv.notify_all();
    m_startCv.wait(lk, [this]() {
        return m_startFlag.load() || m_abortFlag.load();
    });
}

// 2. Coordinator waits until all N worker threads are spawned and waiting:
{
    std::unique_lock<std::mutex> lk(m_startMutex);
    m_startCv.wait(lk, [this, totalWorkers]() {
        return m_readyCount.load() >= totalWorkers || m_abortFlag.load();
    });

    // Release all worker threads at the exact same moment:
    m_startFlag.store(true);
    m_startCv.notify_all();
}
```
This guarantees that all competing threads contend for `ResourceManager`'s `std::mutex m_mutex;` at the exact same instant.

---

## 4. Concurrent Runway Request Demonstration (`ConcurrentAllocationDemo`)

The dedicated engine `ConcurrentAllocationDemo` coordinates the contention scenario:
- **Scenario:** $N$ concurrent worker threads simultaneously request the same runway (e.g. Runway 09R) for different flights (AI-101, 6E-202, AI-303, etc.).
- **Expected & Verified Outcome:**
  - Exactly one thread acquires `m_mutex`, validates availability, reserves the runway, records ownership, and reports `"Allocation Successful"`.
  - All remaining $N-1$ threads acquire `m_mutex` subsequently, detect `m_activeAllocations.contains(runwayId)`, receive `"Resource Contention: Runway X already allocated to flight Y"`, and report `"Allocation Rejected"`.
  - Final runway ownership is held by exactly one flight.
  - Zero data races (`hasDataRace == false`).
  - Max concurrent entries observed in the critical section is strictly $\le 1$.
- **Data Isolation:** Operates 100% in-memory on `ResourceManager` without creating mock/test records in the persistent MySQL database.

---

## 5. ApplicationService Integration & RBAC

`ApplicationService` exposes the concurrency demonstration while preserving Role-Based Access Control:
- **Methods:**
  - `ConcurrentAllocationDemo::DemoReport runConcurrentAllocationDemo(int runwayId, int threadCount = 3, QString *outError = nullptr);`
  - `ConcurrentAllocationDemo::DemoReport runConcurrentAllocationDemo(int runwayId, const QList<ConcurrentAllocationDemo::Request> &customRequests, QString *outError = nullptr);`
- **Authorization Gating:** Requires active session with `action:RunwayAllocation` or `action:ManageRunways`, or `ATC` / `Admin` roles. Unauthenticated requests or unauthorized roles (e.g. `Airport Operator`) are rejected with an explicit error before any thread is spawned.

---

## 6. GUI Integration in RunwaysView & Thread Safety

The Qt GUI (`ui/RunwaysView`) was upgraded to a two-tab interface using `QTabWidget`:
1. **Tab 1: Live Runways & Critical Section:**
   - Retains all operational runway management, table filtering, search, maintenance toggles, and Day 19 critical section activity banner.
2. **Tab 2: Concurrent Allocation Demonstration:**
   - **Controls Toolbar:**
     - Target Runway selector dropdown (`m_demoRunwayCombo`).
     - Worker thread count spinbox (`m_demoThreadSpin`, 2 to 10 threads, default 3).
     - Action buttons: `"▶ Start Concurrent Test"`, `"↺ Reset Demo"`, `"🔄 Refresh Resource State"`.
   - **Status Banner (`m_demoStatusLabel`):** Dynamically updates with color-coded feedback (`🚀 Spawning & Waiting at Barrier`, `✅ Mutual Exclusion Verified`, `❌ Race Condition Detected`).
   - **Metrics Dashboard Cards:**
     - Total Requests (e.g. `3 Requests`)
     - Successful Allocations (e.g. `1 Winner`)
     - Rejected Allocations (e.g. `2 Conflicts`)
     - Final Runway Owner (e.g. `Flight AI-101`)
   - **Real-Time Thread Results Table (`m_demoTable`):**
     - 7 columns: `Thread Label`, `OS System Thread ID`, `Flight ID / Number`, `Requested Runway`, `Request Status`, `Allocation Result`, `Outcome / Contention Message`.
3. **GUI Thread Safety Invariant:**
   - Worker threads **never touch Qt widgets directly**.
   - Asynchronous execution runs via `ConcurrentAllocationDemo::startAsyncDefault()`, which offloads coordination to a background thread (`m_coordinatorThread`).
   - Thread state transitions and final reports are delivered to the main GUI thread via `QMetaObject::invokeMethod(this, [this, res]() { emit threadStateChanged(res); }, Qt::QueuedConnection);`.
   - The GUI remains 100% responsive throughout concurrent execution without UI freezing or blocking joins on the main thread.

---

## 7. Operating System & DBMS Concepts

### 7.1 Operating System Concepts Demonstrated
- **Multithreading:** User-space C++ concurrent execution (`std::thread`).
- **Thread Lifecycle Management:** Safe spawning, argument passing, barrier synchronization, graceful exception catching, and deterministic joining without dangling resources.
- **Shared Resources & Contention:** Airport runway modeled as an exclusive physical OS resource contended by multiple processes.
- **Race Condition Prevention:** The critical section inside `ResourceManager::allocateRunway` is guarded by `std::mutex m_mutex;`, ensuring serialized state evaluation and mutation.
- **Start Barrier Synchronization:** `std::condition_variable` ensures threads execute simultaneously rather than sequentially.
- **Deadlock Prevention:** Non-recursive locking discipline with unlocked private helpers (`*Unlocked`) eliminates self-deadlock.

### 7.2 DBMS Concurrency & Boundaries
- **In-Process Mutex vs. Database Transactions:**
  - `std::mutex` provides synchronization across threads within the **same SmartATC process**.
  - A C++ mutex does **not** provide database transaction atomicity, row-level locks across independent MySQL client connections, or multi-statement rollback guarantees.
  - Full multi-statement ACID transactions (`START TRANSACTION`, `SELECT ... FOR UPDATE`, `COMMIT`, `ROLLBACK`) are scheduled for Phase 2, Days 21–22.
- **Isolated Demo Execution:** The concurrency demonstration operates purely in memory on `ResourceManager`, guaranteeing zero persistent database pollution.

---

## 8. Files Created and Modified

| File | Status | Description |
|---|---|---|
| `src/resource/ConcurrentAllocationDemo.h` | Created | Header for concurrent allocation engine, data structures, start barrier primitives, and signals. |
| `src/resource/ConcurrentAllocationDemo.cpp` | Created | Implementation of thread lifecycle, condition variable start barrier, worker routine, and result collection. |
| `src/services/ApplicationService.h` | Modified | Declared `runConcurrentAllocationDemo` APIs with RBAC and metatypes. |
| `src/services/ApplicationService.cpp` | Modified | Implemented `runConcurrentAllocationDemo` with permission gating and in-memory execution. |
| `ui/RunwaysView.h` | Modified | Added Tab 2 controls, dashboard card labels, results table, demo engine instance, and slots. |
| `ui/RunwaysView.cpp` | Modified | Implemented tabbed UI (`createLiveTab` and `createConcurrentTab`), queued signal bindings, and permission updates. |
| `tests/TestConcurrentRunwayAllocation.cpp` | Created | Comprehensive 30-point automated test suite covering lifecycle, contention, synchronization, and regressions. |
| `CMakeLists.txt` | Modified | Added `ConcurrentAllocationDemo` sources to targets and registered `TestConcurrentRunwayAllocation` test suite. |
| `docs/DAY20_CPP_THREADS_SYNCHRONIZATION_REPORT.md` | Created | Comprehensive Day 20 technical report. |
| `PROJECT_STATUS.md` | Modified | Updated milestone status to reflect completion of Day 20. |

---

## 9. Verification & Test Results

### 9.1 TestConcurrentRunwayAllocation (30 Test Points)
```text
********* Start testing of TestConcurrentRunwayAllocation *********
Config: Using QtTest library 6.11.2, Qt 6.11.2 (x86_64-little_endian-llp64 shared (dynamic) release build; by GCC 13.1.0), windows 11
PASS   : TestConcurrentRunwayAllocation::initTestCase()
PASS   : TestConcurrentRunwayAllocation::testWorkerThreadsStartSuccessfully()
PASS   : TestConcurrentRunwayAllocation::testAllStartedThreadsJoined()
PASS   : TestConcurrentRunwayAllocation::testNoWorkerThreadDetached()
PASS   : TestConcurrentRunwayAllocation::testSharedInputDataRemainsValid()
PASS   : TestConcurrentRunwayAllocation::testWorkerExceptionsCaughtAndReported()
PASS   : TestConcurrentRunwayAllocation::testThreadCreationFailureHandledSafely()
PASS   : TestConcurrentRunwayAllocation::testRepeatedRunsNoLeaks()
PASS   : TestConcurrentRunwayAllocation::testTwoThreadsOneRunwayExactlyOneSuccess()
PASS   : TestConcurrentRunwayAllocation::testThreeThreadsOneRunwayExactlyOneSuccess()
PASS   : TestConcurrentRunwayAllocation::testMultipleRequestsNeverDuplicateActiveAssignments()
PASS   : TestConcurrentRunwayAllocation::testSingleFlightCannotOwnMultipleRunwaysConcurrently()
PASS   : TestConcurrentRunwayAllocation::testMaintenanceRunwayRejectsAllConcurrentAttempts()
PASS   : TestConcurrentRunwayAllocation::testOccupiedRunwayRejectsAdditionalConcurrentRequests()
PASS   : TestConcurrentRunwayAllocation::testDifferentAvailableRunwaysAllocatedIndependently()
PASS   : TestConcurrentRunwayAllocation::testFinalResourceManagerStateMatchesResults()
PASS   : TestConcurrentRunwayAllocation::testAllResourceMutationsUseProtectedPath()
PASS   : TestConcurrentRunwayAllocation::testConcurrentOperationsPreserveInvariants()
PASS   : TestConcurrentRunwayAllocation::testNoDeadlockUnderContention()
PASS   : TestConcurrentRunwayAllocation::testNoDataRacesInResultCollection()
PASS   : TestConcurrentRunwayAllocation::testRepeatedContentionRunsRemainCorrect()
PASS   : TestConcurrentRunwayAllocation::testConditionVariableStartBarrierCorrect()
PASS   : TestConcurrentRunwayAllocation::testSynchronizationObjectsOutliveThreads()
PASS   : TestConcurrentRunwayAllocation::testRegressionDay19CriticalSectionIntact()
PASS   : TestConcurrentRunwayAllocation::testRegressionDay15AllocationIntact()
PASS   : TestConcurrentRunwayAllocation::testRegressionRbacAuthorizationRulesIntact()
PASS   : TestConcurrentRunwayAllocation::testRegressionDay16EmergencyPriorityIntact()
PASS   : TestConcurrentRunwayAllocation::testRegressionDay17PriorityAgingIntact()
PASS   : TestConcurrentRunwayAllocation::testRegressionDay18StarvationSimulationIntact()
PASS   : TestConcurrentRunwayAllocation::testGuiResultDeliveryThreadSafe()
PASS   : TestConcurrentRunwayAllocation::testRealDatabaseRecordsUnchangedByDemo()
PASS   : TestConcurrentRunwayAllocation::cleanupTestCase()
Totals: 32 passed, 0 failed, 0 skipped, 0 blacklisted, 27ms
********* Finished testing of TestConcurrentRunwayAllocation *********
```

### 9.2 Full CTest Suite (All 15 Test Suites)
```text
Test project D:/Projects/Airport Runway Manager &Resource Allocation/SmartATC/build
      Start  1: TestDatabaseConnectivity
 1/15 Test  #1: TestDatabaseConnectivity ................   Passed    0.20 sec
      Start  2: TestAuthAndRBAC
 2/15 Test  #2: TestAuthAndRBAC .........................   Passed    0.58 sec
      Start  3: TestFlightManagement
 3/15 Test  #3: TestFlightManagement ....................   Passed    0.62 sec
      Start  4: TestAircraftManagement
 4/15 Test  #4: TestAircraftManagement ..................   Passed    0.60 sec
      Start  5: TestProcessModel
 5/15 Test  #5: TestProcessModel ........................   Passed    0.59 sec
      Start  6: TestReadyQueueAndProcessStates
 6/15 Test  #6: TestReadyQueueAndProcessStates ..........   Passed    0.61 sec
      Start  7: TestPriorityScheduler
 7/15 Test  #7: TestPriorityScheduler ...................   Passed    0.60 sec
      Start  8: TestSchedulingMetrics
 8/15 Test  #8: TestSchedulingMetrics ...................   Passed    0.59 sec
      Start  9: TestRunwayManagement
 9/15 Test  #9: TestRunwayManagement ....................   Passed    0.60 sec
      Start 10: TestRunwayResourceAllocation
10/15 Test #10: TestRunwayResourceAllocation ............   Passed    0.60 sec
      Start 11: TestDynamicEmergencyPriority
11/15 Test #11: TestDynamicEmergencyPriority ............   Passed    0.62 sec
      Start 12: TestPriorityAging
12/15 Test #12: TestPriorityAging .......................   Passed    0.60 sec
      Start 13: TestStarvationSimulation
13/15 Test #13: TestStarvationSimulation ................   Passed    0.58 sec
      Start 14: TestCriticalSectionAndMutualExclusion
14/15 Test #14: TestCriticalSectionAndMutualExclusion ...   Passed    0.64 sec
      Start 15: TestConcurrentRunwayAllocation
15/15 Test #15: TestConcurrentRunwayAllocation ..........   Passed    0.05 sec

100% tests passed out of 15 (Total Test time = 8.09 sec)
```

### 9.3 Runtime Verification
- Executed `scripts/deploy_runtime.ps1`: verified 12 runtime DLLs (Qt6Core, Qt6Gui, Qt6Widgets, Qt6Sql, Qt6Test, libgcc_s_seh-1, libstdc++-6, libwinpthread-1, qwindows, qsqlmysql, qsqlite, libmysql).
- Tested standalone execution of `SmartATC.exe` outside Qt Creator.

---

## 10. Defects Found & Fixed

1. **`applyAging` Method Signature in Regression Test:**
   - *Issue:* In `testRegressionDay17PriorityAgingIntact()`, called `proc.applyAging(2)` which failed compilation.
   - *Fix:* Aligned call with Day 17 two-argument signature `proc.applyAging(now.addSecs(60), 30)`.
2. **Recursive Mutex Deadlock in Regression Test:**
   - *Issue:* In `testRegressionDay19CriticalSectionIntact()`, calling `criticalSectionStatus(1)` while holding `ScopedResourceLock` attempted to re-acquire `m_mutex`, causing a self-deadlock.
   - *Fix:* Replaced with unlocked helper `criticalSectionStatusUnlocked(1)` while lock is held, and `criticalSectionStatus(1)` after lock release.
3. **Multiple String Replacement in `RunwaysView.cpp`:**
   - *Issue:* Generic target `refreshRunways();` occurred multiple times in `RunwaysView.cpp`.
   - *Fix:* Reverted and applied with specific multiline context blocks.

---

## 11. Known Limitations & Day 21 Readiness

- **In-Process Concurrency Only:** The current `std::mutex` and `ConcurrentAllocationDemo` guarantee mutual exclusion across threads within the single desktop application process.
- **Database Multi-Statement ACID Transactions (Day 21–22 Scope):** Cross-process concurrency and database-level isolation (`START TRANSACTION`, `SELECT ... FOR UPDATE`, row-level locks, `COMMIT/ROLLBACK`) are deferred to Days 21 and 22.
- **ThreadSanitizer Limitation:** The Windows MinGW GCC 13.1 toolchain environment does not support `-fsanitize=thread` on Windows; concurrency safety is verified through deterministic barrier testing and automated invariant assertions.

---

## 12. Conclusion

Day 20 successfully implements and verifies **C++ Multithreading & Synchronization** in SmartATC. Using standard C++17 facilities (`std::thread`, `std::mutex`, `std::condition_variable`), the system demonstrates genuine simultaneous resource contention, verifies race condition prevention, manages thread lifecycles deterministically, and integrates safely with the Qt GUI. All 15 test suites pass 100%.
