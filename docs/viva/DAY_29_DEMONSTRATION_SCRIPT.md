# SmartATC — Final 18-Minute B.Tech Live Demonstration Script

## 1. Demonstration Setup & Guidelines
- **Target Presentation Time:** 18 Minutes (Strict 20-minute maximum ceiling).
- **Environment:** Standalone `SmartATC.exe` running on Windows 10/11 with local MySQL 8.0 on `127.0.0.1:3306`.
- **Pre-State:** Ensure database has baseline seeded data (`admin_demo`, `atc_demo`, `operator_demo`, aircraft `VT-AT1` to `VT-AT4`, Runways `09L`, `09R`, `27`).
- **Safety Policy:** Do NOT run destructive reset scripts (`DROP` or `TRUNCATE`) on the demonstration database.

---

## 2. Minute-by-Minute Action & Spoken Presentation Script

---

### Step 1: Application Launch & System Introduction (00:00 – 01:30)
- **Screen to Open:** Terminal / Desktop folder `SmartATC/build` $\to$ Launch `SmartATC.exe`.
- **Action to Perform:** Double click `SmartATC.exe`. Observe splash and `LoginDialog` appearance.
- **Expected Result:** Application window initializes cleanly without missing DLL errors; login modal appears centered.
- **Concept Demonstrated:** Standalone desktop deployment, Qt event loop initialization, secure session gate.
- **What Presenter Says:**
  > "Respected examiners, we present SmartATC: an educational desktop application modeling airport runway resource allocation to demonstrate core Operating System and Database Management System principles. We begin by launching the standalone compiled application, which interfaces directly with our local MySQL InnoDB relational backend."

---

### Step 2: Authentication & Role-Based Access Control (01:30 – 03:00)
- **Screen to Open:** `LoginDialog`.
- **Action to Perform:**
  1. Enter `operator_demo` / Password. Click **Login**.
  2. Point out that **Declare Emergency** and **Runway Allocation** are disabled/hidden, but **Runway Maintenance** is accessible.
  3. Log out.
  4. Enter `atc_demo` / Password. Click **Login**.
- **Expected Result:** `atc_demo` session unlocks air traffic controller commands (flight dispatching, runway allocation, emergency handling).
- **Concept Demonstrated:** Principle of least privilege, Role-Based Access Control (RBAC), secure SHA-256 password verification.
- **What Presenter Says:**
  > "SmartATC implements two-tier security. Passwords are never stored in plaintext—they are verified using SHA-256 hashes against the MySQL `users` table. Notice when logged in as an Airport Operator, critical flight controls are disabled. Logging in as `atc_demo` unlocks the operational command suite necessary for an Air Traffic Controller."

---

### Step 3: Flight & Aircraft Fleet Inspection (03:00 – 05:00)
- **Screen to Open:** `AircraftView` $\to$ then `FlightsView`.
- **Action to Perform:**
  1. Open `AircraftView`: inspect fleet `VT-AT1` through `VT-AT4` with wake turbulence classes.
  2. Switch to `FlightsView`: observe BOM arrival/departure flights (`AI-101`, `6E-202`, `AI-303`, `SG-404`, `UK-505`).
- **Expected Result:** Relational tables render live data from MySQL; operational statuses display as `Scheduled`.
- **Concept Demonstrated:** 3NF relational normalization (`aircraft` 1:N `flights`), domain entity mapping.
- **What Presenter Says:**
  > "In our Flight and Aircraft management modules, we see our normalized fleet and flight schedules. In Operating System terminology, every scheduled flight represents an incoming schedulable process, with scheduled arrival time representing arrival timestamp, and flight tier representing base process priority from 1 (highest) to 5 (lowest)."

---

### Step 4: OS Scheduling & Process Ready Queue (05:00 – 07:30)
- **Screen to Open:** `SchedulingView` $\to$ **Ready Queue Tab**.
- **Action to Perform:**
  1. Inspect the loaded Ready Queue:
     - Position 0: `AI-101` (Priority 2)
     - Position 1: `6E-202` (Priority 3, arrived 10:05)
     - Position 2: `AI-303` (Priority 3, arrived 10:08)
     - Position 3: `SG-404` (Priority 4)
     - Position 4: `UK-505` (Priority 5)
  2. Click **Dispatch Next Process**: observe `AI-101` transition from `Ready` to `Running`.
- **Expected Result:** Table re-sorts dynamically; PCB state updates visually.
- **Concept Demonstrated:** Non-Preemptive Priority Scheduling, PCB lifecycle (`New` $\to$ `Ready` $\to$ `Running`), FCFS tie-breaking between `6E-202` and `AI-303`.
- **What Presenter Says:**
  > "Here we observe our in-memory Operating System scheduler. Runnable processes in `ReadyQueue` are sorted strictly by effective priority ascending (smaller number is higher priority). Notice that 6E-202 and AI-303 both have Priority 3; our scheduler broke the tie deterministically using earliest arrival time. When we dispatch, AI-101 enters the Running state."

---

### Step 5: Dynamic Emergency Priority Escalation (07:30 – 09:30)
- **Screen to Open:** `SchedulingView` (Emergency Action Panel).
- **Action to Perform:**
  1. Select Flight **AI-303** (currently Priority 3).
  2. Click **Declare Emergency** (Reason: "Engine Oil Pressure Low").
  3. Observe Ready Queue updates: AI-303's effective priority elevates to **1**, moving it to Index 0.
  4. Highlight that **AI-101 is NOT interrupted**: it completes its running maneuver safely.
- **Expected Result:** AI-303 jumps to the head of the queue; base priority 3 remains preserved.
- **Concept Demonstrated:** Dynamic priority escalation, base priority preservation, non-preemptive execution guarantee.
- **What Presenter Says:**
  > "When an in-flight emergency occurs, AI-303 is dynamically escalated to Effective Priority 1, instantly jumping to the head of the queue. Crucially, notice that the currently running flight AI-101 is NOT preempted. In aviation, an aircraft landing cannot be aborted mid-touchdown. Non-preemptive scheduling guarantees safe completion before context-switching to the emergency."

---

### Step 6: Priority Aging & Starvation Demonstration (09:30 – 11:30)
- **Screen to Open:** `SchedulingView` $\to$ **Starvation Demo Tab**.
- **Action to Perform:**
  1. Focus on low-priority flight **UK-505** (Base Priority 5).
  2. Click **Simulate Inflow Flights**: observe UK-505 wait time increase while remaining unscheduled (Starvation).
  3. Check **Enable Priority Aging** (Default interval: 30 seconds).
  4. Observe UK-505's effective priority improve: $5 \to 4 \to 3 \to 2 \to 1$ over elapsed intervals.
- **Expected Result:** Starved process advances to the front of the queue and is dispatched.
- **Concept Demonstrated:** Process starvation, Priority Aging Engine, bounded waiting time ($O(1)$ timestamp evaluation).
- **What Presenter Says:**
  > "Under static priority scheduling, low-priority processes suffer from starvation. On this Starvation Demo panel, we prove both the vulnerability and the OS solution. Without aging, UK-505 waits indefinitely. By enabling our AgingEngine, the scheduler evaluates elapsed waiting time in Ready, decrementing the numerical priority every 30 seconds until UK-505 reaches Priority 1 and is dispatched."

---

### Step 7: Runway Resource Contention & Multithreading (11:30 – 13:30)
- **Screen to Open:** `RunwaysView` $\to$ Click **Concurrent Allocation Demo** button.
- **Action to Perform:**
  1. In `ConcurrentAllocationDemo` dialog, select target resource: **Runway 09L**.
  2. Set Thread 1 = `AI-101`, Thread 2 = `6E-202`.
  3. Click **Launch Concurrent Threads**.
  4. Inspect the thread synchronization console logs.
- **Expected Result:** Thread 1 acquires `ScopedResourceLock` (`std::mutex`), allocates runway; Thread 2 blocks, enters after release, detects Occupied status, and cleanly rejects. Exactly 1 flight wins.
- **Concept Demonstrated:** Critical section, mutual exclusion, `std::mutex`, RAII scoped locking (`std::unique_lock`), thread safety (`std::thread`).
- **What Presenter Says:**
  > "A runway is a shared physical resource. If two flights attempt simultaneous allocation, a fatal conflict occurs. In `ResourceManager`, each runway is guarded by a dedicated `std::mutex`. As demonstrated by these concurrent worker threads, Thread 1 enters the critical section and allocates 09L, while Thread 2 safely blocks. Zero race conditions occur."

---

### Step 8: MySQL Transactions, Row Locking & ACID Rollback (13:30 – 16:30)
- **Screen to Open:** `RunwaysView` (Runway Allocator & Embedded Transaction Log).
- **Action to Perform:**
  1. **Commit Demonstration:** Allocate Runway 09R to Flight SG-404. Show committed transaction log across `runway_assignments`, `runways`, `flights`, and `operation_logs`.
  2. **Rollback Demonstration:** Explain the difference between Type A validation rejection and Type B genuine rollback. Explain how failure injection (`AllocationFailureStage::DuringUpdateFlight`) in tests triggers `db.rollback()`, causing InnoDB undo logs to unroll all intermediate writes.
- **Expected Result:** Demonstrates multi-table atomicity, pessimistic row locks (`SELECT ... FOR UPDATE`), and zero partial writes.
- **Concept Demonstrated:** ACID transaction boundaries, InnoDB row-level locking, atomic COMMIT, undo log ROLLBACK.
- **What Presenter Says:**
  > "At the database layer, allocating a runway modifies four tables: `runway_assignments`, `runways`, `flights`, and `operation_logs`. In `ApplicationService`, these are wrapped in an atomic transaction using `SELECT ... FOR UPDATE` row locks under `REPEATABLE READ`. If any failure occurs midway, `db.rollback()` executes, and MySQL InnoDB undo logs revert all partial writes. We have verified all 4 ACID properties."

---

### Step 9: Centralized Dashboard & Conclusion (16:30 – 18:00)
- **Screen to Open:** `DashboardView`.
- **Action to Perform:**
  1. Review live KPI cards: Active Flights, Runway Utilization Gauge (showing dynamic percentage), Emergency Alert Banner, and live Audit Stream.
  2. Conclude presentation.
- **Expected Result:** Dashboard displays real-time telemetry matching underlying MySQL state.
- **Concept Demonstrated:** System-wide telemetry aggregation, MVC decoupling, educational capstone completion.
- **What Presenter Says:**
  > "To conclude, this is the SmartATC System Dashboard. Every metric is computed dynamically from real application and database state. We have demonstrated OS process management, non-preemptive priority scheduling, starvation prevention via aging, C++ multithreading, and ACID database transactions. Thank you, respected examiners. We are ready for your questions."
