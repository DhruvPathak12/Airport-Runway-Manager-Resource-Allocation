# Scheduler Design Specification — SmartATC

## 1. Overview of the Scheduling Subsystem

The SmartATC scheduling engine simulates an Operating System CPU scheduler tailored for exclusive resource allocation. It consists of four tightly integrated components:
1. **`Models::Process` (Process Control Block):** Holds process execution metadata and timing statistics.
2. **`SchedulerComponents::ReadyQueue`:** A thread-safe, in-memory queue maintaining runnable processes.
3. **`Scheduler`:** Executes Non-Preemptive Priority Scheduling with tie-breaking rules.
4. **`SchedulerComponents::AgingEngine`:** Evaluates elapsed waiting durations and adjusts dynamic priorities to prevent starvation.

---

## 2. Priority Scheduling Rules & Criteria

### 2.1 The Priority Scale
- Priority values range from **1 to 5**.
- **`1` = Highest Priority** (Distress, Fuel Low, Emergency).
- **`2` = High Priority** (Long-haul international arrivals, high passenger density).
- **`3` = Normal Priority** (Standard domestic scheduled flights; default).
- **`4` = Low Priority** (Regional commuter, non-urgent passenger operations).
- **`5` = Lowest Priority** (Cargo, positioning, general aviation).
- **Lower numerical value implies higher scheduling precedence:**
  $$\text{Precedence: } P_{\text{effective}} = 1 > 2 > 3 > 4 > 5$$

### 2.2 Candidate Selection Hierarchy
When `Scheduler::selectNextProcess()` executes, it evaluates the `ReadyQueue` using a strict three-tier deterministic comparator:
1. **Primary Criterion — Effective Priority:**
   The process with the lowest numerical `effectivePriority` is chosen.
2. **Secondary Criterion — Arrival Time (FCFS):**
   If two processes share the same `effectivePriority`, the process with the earlier `arrivalTime` wins.
3. **Tertiary Criterion — Simulated PID (Flight ID):**
   If arrival timestamps are identical, the process with the smaller `flightId` wins, guaranteeing 100% deterministic reproducibility.

### 2.3 Non-Preemptive Execution Rule
Once a process is dispatched (`ProcessState::Running`):
- It holds exclusive control of the execution channel for its full service duration (`serviceTime`).
- The scheduler **forbids preemption**. Even if a new Emergency flight (Priority 1) enters the ready queue while a Priority 5 flight is running, the running flight continues execution until completion. The emergency flight occupies the top of the ready queue and is dispatched immediately when the current flight finishes.

---

## 3. Aging Engine & Starvation Elimination

### 3.1 Starvation Vulnerability
Under high traffic volumes, low-priority flights (e.g. Priority 4 and 5) can experience indefinite waiting (starvation) if higher-priority flights (Priority 1, 2, and 3) continuously enter the ready queue.

### 3.2 The Aging Mechanism
The `AgingEngine` solves starvation through continuous wall-clock monitoring:
- **Eligible State:** Strictly applies to processes whose current state is `ProcessState::Ready`.
- **Elapsed Duration:** Each PCB stores `readySince` (timestamp of entry into continuous `Ready` state).
- **Interval Check:** Configurable interval $T_{\text{aging}}$ (default: 30 seconds).
- **Aging Level Calculation:**
  $$\text{agingLevel} = \min\left(255, \left\lfloor \frac{t_{\text{now}} - \text{readySince}}{T_{\text{aging}}} \right\rfloor\right)$$
- **Effective Priority Adjustment:**
  $$\text{effectivePriority} = \max(1, \text{basePriority} - \text{agingLevel})$$
- **Bounded Range:** Effective priority improves (decrements) by 1 per completed interval, stopping at **1**.
- **Queue Reordering:** When any process priority improves, `ReadyQueue::reorder()` re-sorts the queue.

---

## 4. Worked Scenario: 4-Flight Detailed Scheduling Walkthrough

To demonstrate the exact scheduling rules, emergency overrides, and aging dynamics, consider the following scenario with **four fictional flights**:

### Initial State at $t = 0\text{s}$ (Aging Interval $T_{\text{aging}} = 30\text{s}$)

| Flight Number | Flight ID | Airline | Operation | Arrival Time ($AT$) | Service Time ($BT$) | Base Priority | Initial Dynamic Priority | Status |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **AI-101** | 101 | Air India | Arrival | $t = 0\text{s}$ | 40s | 3 (Normal) | 3 | `Ready` |
| **6E-202** | 202 | IndiGo | Departure | $t = 0\text{s}$ | 30s | 4 (Low) | 4 | `Ready` |
| **SG-303** | 303 | SpiceJet | Arrival | $t = 5\text{s}$ | 35s | 2 (High) | 2 | Arrives at $t=5\text{s}$ |
| **UK-404** | 404 | Vistara | Arrival | $t = 25\text{s}$ | 25s | 5 (Lowest) | 5 | Arrives at $t=25\text{s}$ |

---

### Chronological Step-by-Step Execution

#### Event 1: Time $t = 0\text{s}$ — Initial Dispatch
- **Ready Queue:** `[AI-101 (P:3, AT:0s), 6E-202 (P:4, AT:0s)]`
- **Scheduler Decision:** `AI-101` has higher priority (P:3 vs P:4).
- **Dispatch:** `AI-101` is dispatched to `Running` on Runway 1.
- **Service Duration:** 40s (scheduled completion at $t = 40\text{s}$).
- **Ready Queue Remaining:** `[6E-202 (P:4, AT:0s, readySince:0s)]`

#### Event 2: Time $t = 5\text{s}$ — Arrival of SG-303
- `SG-303` enters ready queue with Base Priority 2.
- Ready Queue: `[SG-303 (P:2, AT:5s), 6E-202 (P:4, AT:0s)]`
- Runway 1 is busy servicing `AI-101`. Non-preemption rule applies; `AI-101` continues.

#### Event 3: Time $t = 25\text{s}$ — Arrival of UK-404
- `UK-404` enters ready queue with Base Priority 5.
- Ready Queue: `[SG-303 (P:2, AT:5s), 6E-202 (P:4, AT:0s), UK-404 (P:5, AT:25s)]`
- Runway 1 remains busy servicing `AI-101`.

#### Event 4: Time $t = 30\text{s}$ — First Aging Evaluation ($T_{\text{aging}} = 30\text{s}$)
- The `AgingEngine` checks all processes in `Ready` state:
  - `6E-202`: Elapsed wait $= 30\text{s} - 0\text{s} = 30\text{s}$. Accrues 1 aging level!
    $$\text{effectivePriority} = 4 - 1 = \mathbf{3}$$
  - `SG-303`: Elapsed wait $= 30\text{s} - 5\text{s} = 25\text{s} < 30\text{s}$. (Aging Level = 0, P remains 2).
  - `UK-404`: Elapsed wait $= 30\text{s} - 25\text{s} = 5\text{s} < 30\text{s}$. (Aging Level = 0, P remains 5).
- `ReadyQueue::reorder()` re-sorts:
  - Ready Queue: `[SG-303 (P:2, AT:5s), 6E-202 (P:3, AT:0s), UK-404 (P:5, AT:25s)]`

#### Event 5: Time $t = 35\text{s}$ — Emergency Declaration on UK-404!
- Flight `UK-404` reports hydraulic failure; ATC toggles `is_emergency = true`.
- **Dynamic Emergency Override:**
  $$\text{effectivePriority}(\text{UK-404}) = \mathbf{1}$$
- `ReadyQueue::reorder()` immediately elevates `UK-404` to the head of the queue:
  - Ready Queue: `[UK-404 (P:1 - EMERGENCY), SG-303 (P:2), 6E-202 (P:3)]`
- Non-preemption rule applies: `AI-101` continues until $t = 40\text{s}$.

#### Event 6: Time $t = 40\text{s}$ — AI-101 Completes; UK-404 Dispatched
- `AI-101` finishes execution on Runway 1:
  - $CT = 40\text{s}$
  - $TAT = 40 - 0 = \mathbf{40\text{s}}$
  - $WT = 40 - 40 = \mathbf{0\text{s}}$
  - State $\to$ `Completed`.
- **Scheduler Decision:** Head of ready queue is `UK-404` (Priority 1, Emergency).
- **Dispatch:** `UK-404` dispatched to `Running` (burst 25s, completion at $t = 65\text{s}$).
- Remaining in Ready Queue: `[SG-303 (P:2, AT:5s), 6E-202 (P:3, AT:0s)]`

#### Event 7: Time $t = 60\text{s}$ — Second Aging Evaluation
- `6E-202`: Elapsed wait $= 60\text{s} - 0\text{s} = 60\text{s} \implies 2$ aging levels!
  $$\text{effectivePriority} = 4 - 2 = \mathbf{2}$$
- `SG-303`: Elapsed wait $= 60\text{s} - 5\text{s} = 55\text{s} \implies 1$ aging level!
  $$\text{effectivePriority} = 2 - 1 = \mathbf{1}$$
- Ready Queue: `[SG-303 (P:1, AT:5s), 6E-202 (P:2, AT:0s)]`

#### Event 8: Time $t = 65\text{s}$ — UK-404 Completes; SG-303 Dispatched
- `UK-404` finishes execution:
  - $CT = 65\text{s}$
  - $TAT = 65 - 25 = \mathbf{40\text{s}}$
  - $WT = 40 - 25 = \mathbf{15\text{s}}$
  - State $\to$ `Completed`.
- **Scheduler Decision:** `SG-303` has Priority 1 (aged). Dispatched to `Running` (burst 35s, completion at $t = 100\text{s}$).
- Remaining in Ready Queue: `[6E-202 (P:2, AT:0s)]`

#### Event 9: Time $t = 90\text{s}$ — Third Aging Evaluation
- `6E-202`: Elapsed wait $= 90\text{s} - 0\text{s} = 90\text{s} \implies 3$ aging levels!
  $$\text{effectivePriority} = 4 - 3 = \mathbf{1}$$
- Ready Queue: `[6E-202 (P:1, AT:0s)]`

#### Event 10: Time $t = 100\text{s}$ — SG-303 Completes; 6E-202 Dispatched
- `SG-303` finishes execution:
  - $CT = 100\text{s}$
  - $TAT = 100 - 5 = \mathbf{95\text{s}}$
  - $WT = 95 - 35 = \mathbf{60\text{s}}$
  - State $\to$ `Completed`.
- `6E-202` (which originally had low Priority 4, but is now Priority 1 due to aging) is dispatched!
- **Starvation was completely prevented!**
- At $t = 130\text{s}$, `6E-202` completes ($CT = 130\text{s}$, $TAT = 130\text{s}$, $WT = 100\text{s}$).

---

### 4.3 Final Scenario Metrics Summary

| Flight | Arrival ($AT$) | Burst ($BT$) | Start Time | Completion ($CT$) | Turnaround ($TAT = CT - AT$) | Waiting ($WT = TAT - BT$) |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **AI-101** | 0s | 40s | 0s | 40s | **40s** | **0s** |
| **UK-404** (Emg) | 25s | 25s | 40s | 65s | **40s** | **15s** |
| **SG-303** | 5s | 35s | 65s | 100s | **95s** | **60s** |
| **6E-202** (Aged)| 0s | 30s | 100s | 130s | **130s** | **100s** |

- **Average Turnaround Time:** $\overline{TAT} = \frac{40 + 40 + 95 + 130}{4} = \frac{305}{4} = \mathbf{76.25\text{ seconds}}$
- **Average Waiting Time:** $\overline{WT} = \frac{0 + 15 + 60 + 100}{4} = \frac{175}{4} = \mathbf{43.75\text{ seconds}}$
