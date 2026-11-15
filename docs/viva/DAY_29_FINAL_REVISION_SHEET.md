# SmartATC — Final High-Yield Viva Revision Sheet (Quick Reference)

*Keep this sheet handy for last-minute review immediately prior to the viva examination.*

---

## 1. Quick Facts & Numbers to Remember

- **Project:** Smart Air Traffic Control Management System (SmartATC)
- **Setting:** Chhatrapati Shivaji Maharaj International Airport (BOM / VABB), Mumbai
- **Technology Stack:** C++17, Qt 6.11.2 Widgets, MySQL 8.0.46 (InnoDB), CMake 3.19+, Ninja
- **Automated Tests:** **19 CTest Suites** (100% Pass Rate)
- **Database Tables:** Exactly **8 normalized tables** in 3NF
- **Roles:** `Admin`, `ATC` (Air Traffic Controller), `Airport Operator`
- **Scheduler:** Non-Preemptive Priority Scheduling with dynamic aging
- **Priority Scale:** **1 = Highest Priority** (Emergency/VIP), **5 = Lowest Priority** (Routine domestic/cargo)
- **Default Aging Interval:** **30 Seconds** (Configurable from 1s to 86,400s)
- **Process States in C++:** `New`, `Ready`, `Running`, `Blocked`, `Completed`
- **Database Process States:** `Ready`, `Running`, `Completed`, `Blocked`
- **Runways in Demo:** `09L` (Available/Both), `09R` (Available/Takeoff), `27` (Maintenance/Landing)

---

## 2. Core Mathematical Formulas

### 1. Turnaround Time ($TAT$)
$$TAT = t_{\text{completion}} - t_{\text{arrival}} \quad (\text{seconds})$$

### 2. Waiting Time ($WT$)
$$WT = TAT - t_{\text{service}} \quad (\text{seconds, where } WT \ge 0)$$

### 3. Continuous Waiting Duration ($\Delta t_{\text{ready}}$)
$$\Delta t_{\text{ready}} = \text{readySince.secsTo}(t_{\text{now}}) \quad (\text{seconds})$$

### 4. Accrued Aging Levels ($L_{\text{aging}}$)
$$L_{\text{aging}} = \left\lfloor \frac{\Delta t_{\text{ready}}}{T_{\text{interval}}} \right\rfloor \quad (\text{where } T_{\text{interval}} = 30\text{s default})$$

### 5. Effective Priority Formula ($P_{\text{eff}}$)
$$P_{\text{eff}} = \begin{cases} 
1 & \text{if } \text{isEmergency} == \text{true} \\
\max(1, \ P_{\text{base}} - L_{\text{aging}}) & \text{otherwise}
\end{cases}$$

### 6. Runway Operational Utilization Percentage
$$\text{Utilization} = \frac{\text{Count}(\text{Occupied Runways})}{\text{Count}(\text{Available Runways}) + \text{Count}(\text{Occupied Runways})} \times 100\%$$
*(Runways in Maintenance are excluded from the denominator)*

---

## 3. The 8 Database Tables Cheat Sheet

| # | Table Name | Primary Key | Key Foreign Keys & Constraints |
|---|---|---|---|
| 1 | `roles` | `role_id` | `uq_roles_role_name` (`VARCHAR(50) UNIQUE`) |
| 2 | `users` | `user_id` | `fk_users_role_id` $\to$ `roles`, `uq_users_username` |
| 3 | `aircraft` | `aircraft_id` | `uq_aircraft_registration`, `chk_aircraft_capacity` ($> 0$) |
| 4 | `flights` | `flight_id` | `fk_flights_aircraft_id` $\to$ `aircraft`, `chk_flights_priority` (1–5) |
| 5 | `runways` | `runway_id` | `uq_runways_runway_number` (`09L`, `09R`, `27`) |
| 6 | `scheduling_queue` | `queue_id` | `fk_sq_flight_id` $\to$ `flights`, `uq_sq_flight_id` (1:0..1), `chk_sq_base_priority`, `chk_sq_effective_priority` |
| 7 | `runway_assignments` | `assignment_id` | `fk_ra_flight_id` $\to$ `flights`, `fk_ra_runway_id` $\to$ `runways`, `fk_ra_assigned_by` $\to$ `users` |
| 8 | `operation_logs` | `log_id` | `fk_ol_flight_id`, `fk_ol_runway_id`, `fk_ol_user_id` (all `SET NULL`) |

---

## 4. Key Real C++ Classes & Methods Reference

- **Service Façade:** `ApplicationService` (`src/services/ApplicationService.cpp`)
  - `allocateRunway(int runwayId, int flightId, QString *outError)`
  - `releaseRunway(int runwayId, QString *outError)`
  - `declareEmergency(int flightId, const QString &reason)`
  - `setAllocationFailureStage(AllocationFailureStage stage)`
- **Database Singleton:** `DatabaseManager` (`src/database/DatabaseManager.cpp`)
  - `beginTransaction()`, `commitTransaction()`, `rollbackTransaction()`
- **Scheduler Subsystem:** (`src/scheduler/`)
  - `ReadyQueue::push()`, `ReadyQueue::pop()`, `ReadyQueue::reorder()`
  - `AgingEngine::evaluateAging()`, `AgingEngine::setIntervalSeconds()`
  - `Scheduler::dispatchNext()`
- **Resource Subsystem:** (`src/resource/`)
  - `ResourceManager::allocateRunway()`, `ResourceManager::releaseRunway()`
  - `ResourceManager::ScopedResourceLock` (RAII `std::unique_lock<std::mutex>`)
  - `ConcurrentAllocationDemo` (`std::thread` worker simulations)

---

## 5. Top 5 Killer Viva Answers

1. **Why non-preemptive scheduling?**
   > "An aircraft in touchdown rollout or takeoff cannot abort mid-runway without severe crash hazard. Once allocated, a flight holds the runway non-preemptively until completion; higher-priority flights wait for the next scheduling opportunity."
2. **Why is a runway not a CPU?**
   > "A CPU executes instruction cycles and can be time-shared and preempted. A runway is a shared physical hardware resource requiring mutual exclusion for its operational burst duration."
3. **Difference between base priority and effective priority?**
   > "`basePriority` is the immutable static tier of the flight. `effectivePriority` is the dynamic runtime priority used by the Ready Queue, which is adjusted by emergency escalation (set to 1) and by the aging formula."
4. **Difference between validation rejection and transaction rollback?**
   > "Validation rejection happens before write statements execute when a resource is unavailable. Transaction rollback happens after mutations have already executed in MySQL buffer; `db.rollback()` triggers InnoDB undo logs to unroll partial writes and restore the baseline."
5. **Why use both C++ mutex and MySQL row locks?**
   > "A C++ mutex only synchronizes memory within a single application process. MySQL row locks (`SELECT ... FOR UPDATE`) serialize database transactions across multiple concurrent application instances or network clients."
