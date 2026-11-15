# SmartATC — Operating System Concepts Demonstration Guide

## 1. Overview & Theoretical Mapping

The **Smart Air Traffic Control Management System (SmartATC)** serves as an applied educational simulation of fundamental Operating System (OS) concepts.

Rather than implementing synthetic toys, SmartATC maps aviation domain operations directly to operating system structures:

| Aviation Domain Entity | Operating System Concept | C++ Implementation Entity |
|---|---|---|
| **Flight Operation** | **Process** | `Models::Process` struct / Process Control Block |
| **Aircraft Waiting Queue** | **Ready Queue** | `SchedulerComponents::ReadyQueue` |
| **Flight Urgency / Tier** | **Process Priority** | `basePriority` & `effectivePriority` (1 = Highest, 5 = Lowest) |
| **In-Flight Emergency** | **High-Priority Process Escalation** | `isEmergency` flag $\to$ dynamic `effectivePriority = 1` override |
| **Long Waiting on Ground** | **Process Starvation** | Accumulated continuous waiting time in `READY` state |
| **Priority Escalation Over Time** | **Priority Aging** | `SchedulerComponents::AgingEngine` periodic promotion |
| **Physical Runway** | **Shared Hardware Resource** | `Models::Runway` protected by Critical Section |
| **Runway Acquisition / Clear** | **Critical Section & Mutual Exclusion**| `ResourceManager::ScopedResourceLock` (`std::mutex`) |

---

## 2. Process Control Block (PCB) & Lifecycle States

### 2.1 C++ Process Lifecycle States
In `SmartATC/src/models/Process.h`, the process lifecycle is defined strictly by the C++ `ProcessState` enum:

```cpp
enum class ProcessState {
    New,        // Initial creation state prior to queue entry
    Ready,      // Enqueued in ReadyQueue awaiting runway assignment
    Running,    // Currently dispatched / occupying runway
    Blocked,    // Waiting on external resource (e.g. runway occupied/maintenance)
    Completed   // Operation finished; turnaround/waiting metrics computed
};
```

> [!IMPORTANT]
> **State Model Correctness:**
> SmartATC uses strictly `New`, `Ready`, `Running`, `Blocked`, and `Completed`.
> Do not confuse these with legacy OS textbook terms like `Waiting` or `Terminated`. In SmartATC, a waiting process in the scheduling queue is in the `Ready` state, a blocked resource condition is in the `Blocked` state, and a finished process is in the `Completed` state.

### 2.2 Database Process State Mapping
In MySQL, the `scheduling_queue.process_state` column is defined as:
```sql
process_state ENUM('Ready', 'Running', 'Completed', 'Blocked') NOT NULL DEFAULT 'Ready'
```
- `ProcessState::New` maps to `'Ready'` upon persistent insertion into the database scheduling queue.
- `ProcessState::Ready` $\to$ `'Ready'`
- `ProcessState::Running` $\to$ `'Running'`
- `ProcessState::Blocked` $\to$ `'Blocked'`
- `ProcessState::Completed` $\to$ `'Completed'`

### 2.3 Process Control Block Attributes
Every flight process is encapsulated within a `Models::Process` struct:
- `flightId` / PID: Unique integer identifier.
- `basePriority`: Static priority assigned at flight scheduling (1 to 5).
- `effectivePriority`: Dynamically calculated priority used for queue ordering.
- `arrivalTime`: Wall-clock timestamp when the flight entered the ready queue.
- `readySince`: Wall-clock timestamp when the process entered the continuous `READY` state (used for aging).
- `agingLevel`: Number of completed aging intervals accrued while continuously in `READY`.
- `isEmergency`: Boolean flag indicating in-flight distress.
- `serviceTime`: Burst duration (simulated runway service time in seconds, default 60s).
- `waitingTime`: Cumulative time spent waiting in `READY` (Turnaround Time - Service Time).
- `turnaroundTime`: Elapsed duration from `arrivalTime` to `completionTime`.

---

## 3. Non-Preemptive Priority Scheduling & Tie-Breaking

### 3.1 Ordering Rules
SmartATC implements **Non-Preemptive Priority Scheduling**. 

In `ReadyQueue::sort()`, runnable processes are sorted using a strict three-tier deterministic comparator:
1. **Effective Priority ASC:** Lower numerical value indicates higher priority (Priority 1 is Highest; Priority 5 is Lowest).
2. **Arrival Time ASC:** If effective priorities are equal, the earlier `arrivalTime` is selected first (First-Come, First-Served / FCFS tie-breaker).
3. **Flight ID ASC:** If priority and arrival time are identical, the smaller `flightId` wins (deterministic stability tie-breaker).

$$\text{Ordering: } (\text{effectivePriority} \uparrow, \ \text{arrivalTime} \uparrow, \ \text{flightId} \uparrow)$$

### 3.2 Worked Demonstration Scenario (4-Process Queue)
Consider the baseline demonstration queue at Chhatrapati Shivaji Maharaj International Airport (BOM):

| Flight Number | Flight ID | Arrival Time | Base Priority | Effective Priority | State |
|---|---|---|---|---|---|
| **AI-101** | 1 | 10:00:00 | 2 | 2 | `Ready` |
| **6E-202** | 2 | 10:05:00 | 3 | 3 | `Ready` |
| **AI-303** | 3 | 10:08:00 | 3 | 3 | `Ready` |
| **SG-404** | 4 | 10:02:00 | 4 | 4 | `Ready` |

**Scheduler Queue Execution Order:**
1. **Index 0 $\to$ AI-101 (Priority 2):** Highest priority in queue ($2 < 3, 4$).
2. **Index 1 $\to$ 6E-202 (Priority 3):** Ties with AI-303 on priority ($3 == 3$), but arrived earlier (10:05:00 vs. 10:08:00).
3. **Index 2 $\to$ AI-303 (Priority 3):** Dispatched after 6E-202 due to arrival time tie-breaker.
4. **Index 3 $\to$ SG-404 (Priority 4):** Lowest priority in the queue.

---

## 4. Emergency Priority Escalation in Non-Preemptive Scheduling

### 4.1 Strict Non-Preemptive Behavior
> [!IMPORTANT]
> **No Preemption of Running Operations:**
> SmartATC is a **non-preemptive** scheduling system. 
> When an emergency is declared for a flight, the system elevates that flight's priority immediately. However, **it does not preempt, interrupt, or abort an already-running process**.
> In aviation, an aircraft in the critical stages of landing or rollout cannot be safely aborted mid-runway. The emergency flight jumps to the head of the `READY` queue and will be selected at the **next scheduling opportunity** once the currently running operation completes or releases its resource.

### 4.2 Dynamic Priority Elevation Invariants
When an emergency is declared via `ApplicationService::declareEmergency(flightId)`:
1. `isEmergency` flag is set to `true`.
2. `effectivePriority` is immediately overridden to **1** (highest possible priority).
3. `basePriority` remains **strictly preserved** (e.g., base priority 3 remains stored as 3).
4. The `ReadyQueue` re-sorts: the emergency process jumps to the head of the `ReadyQueue` ahead of all normal Priority 2–5 processes.
5. If the emergency is cleared: `effectivePriority` is restored to its aging-adjusted base priority:
   $$\text{effectivePriority} = \max(1, \text{basePriority} - \text{agingLevel})$$

---

## 5. Dynamic Priority Aging & Starvation Prevention

### 5.1 The Starvation Problem
In pure priority scheduling, continuous arrival of high-priority flights (Priorities 1 and 2) causes low-priority flights (e.g. `UK-505` with Priority 5) to wait indefinitely. This is **starvation** (indefinite blocking).

### 5.2 Mathematical Aging Model
SmartATC prevents starvation via `SchedulerComponents::AgingEngine`.

Aging is computed strictly from the continuous duration the process has spent in the `Ready` state:
$$\text{elapsedSeconds} = \text{readySince.secsTo}(now)$$

$$\text{agingLevels} = \left\lfloor \frac{\text{elapsedSeconds}}{\text{configuredIntervalSeconds}} \right\rfloor$$

$$\text{effectivePriority} = \begin{cases} 
1 & \text{if } \text{isEmergency} == \text{true} \\
\max(1, \ \text{basePriority} - \text{agingLevels}) & \text{otherwise}
\end{cases}$$

### 5.3 Interval Configuration
- **Default Configured Interval:** `AgingEngine::DEFAULT_INTERVAL_SECONDS = 30` seconds.
- **Configurable Range:** 1 second to 86,400 seconds (via `AgingEngine::setIntervalSeconds()`).
- **Demonstration Settings:** During interactive testing or GUI demonstration, the interval can be configured (e.g., 30s default or custom demo interval) to show aging cycles cleanly without arbitrary wait times.
- **Idempotency:** Because aging is calculated from `readySince.secsTo(now)`, evaluation is strictly idempotent: delayed timer triggers or timer restarts never cause skipped or duplicated aging levels.
- **Lower Bound:** The effective priority improves by decreasing numerically, but **never falls below 1** ($\max(1, \dots)$).

### 5.4 Worked Aging Example
Process `UK-505` enters the Ready Queue with `basePriority = 5` and `configuredIntervalSeconds = 30`:

| Elapsed Time in `Ready` | Completed Intervals (`agingLevels`) | Effective Priority Calculation | Resulting Effective Priority |
|---|---|---|---|
| $0\text{s} \le t < 30\text{s}$ | 0 | $\max(1, 5 - 0)$ | **5** |
| $30\text{s} \le t < 60\text{s}$ | 1 | $\max(1, 5 - 1)$ | **4** |
| $60\text{s} \le t < 90\text{s}$ | 2 | $\max(1, 5 - 2)$ | **3** |
| $90\text{s} \le t < 120\text{s}$ | 3 | $\max(1, 5 - 3)$ | **2** |
| $t \ge 120\text{s}$ | 4 | $\max(1, 5 - 4)$ | **1** (Upper bound reached) |

Once `UK-505` reaches Effective Priority 1, it competes equally with top-tier flights, breaking starvation.

---

## 6. Critical Section, Mutual Exclusion & Concurrency

### 6.1 Runway as a Shared Resource
A physical runway is a non-shareable resource. If two flights attempt to take off or land on Runway `09L` simultaneously, a fatal conflict occurs.

In SmartATC:
- Every runway resource is protected by a dedicated `std::mutex` in `ResourceManager`.
- Allocation requests must enter the **Critical Section** guarded by the mutex.
- We utilize an RAII wrapper: `ResourceManager::ScopedResourceLock`.

### 6.2 Mutual Exclusion Verification (`ConcurrentAllocationDemo`)
In `ui/RunwayManagementView.cpp` (and automated test suite `TestConcurrentRunwayAllocation.cpp`):
1. Two simultaneous worker threads (`Thread 1` and `Thread 2`) are launched using `std::thread`.
2. Both threads attempt to allocate **Runway 09L** to different flights (`AI-101` and `6E-202`).
3. Execution trace:
   - `Thread 1` acquires `ScopedResourceLock` on Runway 09L and enters the critical section.
   - `Thread 2` attempts to acquire the lock on Runway 09L, finds it locked, and blocks safely.
   - `Thread 1` executes the database transaction, updates runway status to `Occupied`, commits, and exits the critical section (releasing the mutex).
   - `Thread 2` unblocks, enters the critical section, reads the updated state, detects that Runway 09L is now `Occupied`, cleanly rejects the allocation, and exits.
4. **Result:** Exactly **one** flight acquires the runway. Mutual exclusion is guaranteed, with zero race conditions and zero deadlocks.
