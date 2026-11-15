# SmartATC — Four-Member Technical Contribution & Viva Defense Guide

## 1. Team Structure & Git Authorship Transparency

SmartATC was developed as a collaborative 4-member B.Tech capstone project. 

> [!IMPORTANT]
> **Git Authorship Evidence Notice:**  
> In the Git repository history (`git log`), commits were committed and pushed through a single developer identity (`Dhruv Pathak` / `DhruvPathak12`), who acted as the project's **Repository Maintainer and Lead Integrator** on behalf of the group. 
> 
> Therefore, Git commit authorship alone does not provide an isolated per-member commit breakdown. The technical division of responsibilities documented below reflects the agreed module ownership, architectural boundaries, test suites authored, and domain deliverables established across the team lifecycle.

---

## 2. Member-Wise Responsibility Matrix & Evidence

| Team Member | Functional Domain | Primary Source Files | Database Tables Handled | Automated Tests Authored |
|:---|:---|:---|:---|:---|
| **Member 1** | **User & Role Management, Security & Audit** | `src/models/User.h`, `src/models/Session.*`, `src/utils/PasswordUtils.*`, `ui/LoginView.*` | `roles`, `users`, `operation_logs` | `TestAuthAndRBAC` |
| **Member 2** | **Flight & Aircraft Fleet Administration** | `src/models/Flight.h`, `src/models/Aircraft.h`, `ui/FlightsView.*`, `ui/FlightDialog.*`, `ui/AircraftView.*`, `ui/AircraftDialog.*` | `aircraft`, `flights` | `TestFlightManagement`, `TestAircraftManagement` |
| **Member 3** | **OS Process Modeling & Priority Scheduling** | `src/models/Process.*`, `src/scheduler/ReadyQueue.*`, `src/scheduler/Scheduler.*`, `src/scheduler/AgingEngine.*`, `src/scheduler/StarvationSimulation.*`, `ui/SchedulingView.*` | `scheduling_queue` | `TestProcessModel`, `TestReadyQueueAndProcessStates`, `TestPriorityScheduler`, `TestSchedulingMetrics`, `TestDynamicEmergencyPriority`, `TestPriorityAging`, `TestStarvationSimulation` |
| **Member 4** | **Runway Concurrency & MySQL Transactions** | `src/resource/ResourceManager.*`, `src/resource/ConcurrentAllocationDemo.*`, `src/database/DatabaseManager.*`, `src/models/Runway.h`, `src/models/RunwayAssignment.h`, `ui/RunwaysView.*`, `ui/RunwayAllocationDialog.*`, `ui/DashboardView.*` | `runways`, `runway_assignments` | `TestRunwayManagement`, `TestRunwayResourceAllocation`, `TestCriticalSectionAndMutualExclusion`, `TestConcurrentRunwayAllocation`, `TestDatabaseTransactions`, `TestACIDProperties`, `TestDashboardMonitoring` |

---

## 3. Individual Member Contribution Profiles

---

### Member 1: User & Role Management, Security & Audit

#### 1. Assigned Responsibility
Authentication, Authorization, Role-Based Access Control (RBAC), and Audit Trail logging.

#### 2. Actual Implemented Features
- Secure login verification against MySQL `users` table using SHA-256 password hashing.
- Session management (`Models::Session`) maintaining current user ID, role, and permission keys.
- Least-privilege permission matrix gating actions (`action:RunwayAllocation`, `action:ManageScheduling`, `action:DeclareEmergency`, `action:RunwayMaintenance`).
- Operational audit logging in `operation_logs` tracking user timestamps, actions, and transaction statuses (`Committed`, `Rolled Back`).

#### 3. Important Source Files
- `src/models/User.h`
- `src/models/Session.h` & `src/models/Session.cpp`
- `src/utils/PasswordUtils.h` & `src/utils/PasswordUtils.cpp`
- `ui/LoginView.h` & `ui/LoginView.cpp`

#### 4. Important Classes and Methods
- `Models::Session::hasPermission(const QString &key) const`
- `Utils::PasswordUtils::hash(const QString &plaintext)`
- `Utils::PasswordUtils::verify(const QString &plaintext, const QString &storedHash)`
- `ApplicationService::login(const QString &username, const QString &password, QString *outError)`

#### 5. Database Tables Handled
- `roles` (`role_id`, `role_name`)
- `users` (`user_id`, `username`, `password_hash`, `role_id`, `status`)
- `operation_logs` (`log_id`, `user_id`, `operation_type`, `transaction_status`, `message`)

#### 6. Tests Related to Their Work
- `TestAuthAndRBAC`: Validates valid credentials, invalid password rejection, inactive account blocking, and permission key checks.

#### 7. Integration Work
- Bound the `LoginView` modal to `MainWindow`, ensuring session state is broadcast via signals to unlock specific navigation items.
- Connected service methods to record operational audit logs on every flight or runway mutation.

#### 8. Challenges Encountered & Solutions
- *Challenge:* Preventing SQL injection while checking user credentials.  
  *Solution:* Used Qt's parameterized `QSqlQuery::prepare()` and `bindValue()` to ensure inputs are strictly parsed as data literals.
- *Challenge:* Unsalted SHA-256 vulnerability to dictionary attacks.  
  *Solution:* Evaluated and documented the limitation transparently in `SECURITY_CONSIDERATIONS.md`, identifying Argon2id/bcrypt as the recommended production upgrade.

#### 9. Likely Individual Viva Questions
1. *How does your application prevent unauthorized operators from allocating runways?*
2. *Where are passwords stored, and why did you choose SHA-256?*
3. *What is the difference between authentication and authorization in SmartATC?*

#### 10. Spoken Contribution Summaries
- **60-Second Explanation:**
  > "I took ownership of User Management, Security, and Role-Based Access Control in SmartATC. I implemented the authentication layer using SHA-256 password hashing and developed the `Session` model that enforces a strict least-privilege permission matrix across Admin, Air Traffic Controller, and Airport Operator roles. I authored `LoginView`, created the DDL for the `roles`, `users`, and `operation_logs` tables in MySQL, and ensured that every state-modifying action in the service layer requires verified permission keys. I also authored the automated test suite `TestAuthAndRBAC`."
- **2-Minute Explanation:**
  > "In SmartATC, my primary domain was security, identity, and audit compliance. At a commercial airport, access must follow the principle of least privilege: an airport operator inspecting runways must not be able to declare emergencies or clear flights. 
  > 
  > I engineered the `Session` and `User` models, creating a permission matrix where specific keys like `action:RunwayAllocation` are granted exclusively to ATC and Admin. I developed `LoginView` using Qt Widgets, incorporating password masking and error feedback. In `PasswordUtils`, I implemented SHA-256 hashing to ensure plaintext credentials never exist in memory or in the database.
  > 
  > At the database layer, I designed the schema for `roles`, `users`, and the `operation_logs` table. Every significant system event—from login to runway release—records an immutable log entry with user ID, target entity, timestamp, and transaction outcome (`Committed` or `Rolled Back`). Finally, I authored `TestAuthAndRBAC`, verifying credential validation, role permissions, and session invalidation."

---

### Member 2: Flight & Aircraft Domain Engineering

#### 1. Assigned Responsibility
Aviation domain data modeling, fleet management, and schedule administration.

#### 2. Actual Implemented Features
- Aircraft fleet registry (`aircraft` table) managing registration numbers, airworthiness status, and passenger capacities.
- Flight lifecycle tracking (`flights` table) from `Scheduled` to `Taxiing`, `Cleared`, `Landed`, `Departed`, and `Cancelled`.
- Interactive Qt GUI views (`FlightsView`, `AircraftView`) with tabular display, status badges, and input validation modals.
- Wake turbulence categorization (Heavy, Medium, Light) used for runway compatibility checks.

#### 3. Important Source Files
- `src/models/Flight.h`
- `src/models/Aircraft.h`
- `ui/FlightsView.h` & `ui/FlightsView.cpp`
- `ui/FlightDialog.h` & `ui/FlightDialog.cpp`
- `ui/AircraftView.h` & `ui/AircraftView.cpp`
- `ui/AircraftDialog.h` & `ui/AircraftDialog.cpp`

#### 4. Important Classes and Methods
- `Models::Flight` struct (Properties: `flightId`, `flightNumber`, `airline`, `aircraftId`, `priority`, `isEmergency`, `operationalStatus`)
- `Models::Aircraft` struct (Properties: `aircraftId`, `registrationNumber`, `aircraftType`, `capacity`, `status`)
- `FlightsView::refreshData()` & `FlightsView::onAddFlightClicked()`
- `ApplicationService::createFlight()` & `ApplicationService::updateFlightStatus()`

#### 5. Database Tables Handled
- `aircraft` (`aircraft_id`, `registration_number`, `aircraft_type`, `capacity`, `status`)
- `flights` (`flight_id`, `flight_number`, `airline`, `aircraft_id`, `flight_type`, `scheduled_time`, `priority`, `is_emergency`, `operational_status`)

#### 6. Tests Related to Their Work
- `TestFlightManagement`: Verifies flight CRUD operations, flight number format checks, and foreign key constraints.
- `TestAircraftManagement`: Verifies aircraft registration uniqueness, positive capacity checks, and status updates.

#### 7. Integration Work
- Connected the Flight domain model to the OS simulation layer, ensuring that newly scheduled flights can be enqueued as Process Control Blocks (`Process`) in `ReadyQueue`.
- Bound `FlightsView` to `ApplicationService` to ensure UI tables refresh dynamically upon database updates.

#### 8. Challenges Encountered & Solutions
- *Challenge:* Managing aircraft assignment without requiring all flights to have assigned airplanes immediately.  
  *Solution:* Configured foreign key `fk_flights_aircraft_id` with `ON DELETE SET NULL`, allowing flights to be scheduled before physical aircraft allocation.
- *Challenge:* Enforcing priority bounds (1–5).  
  *Solution:* Added SQL `CHECK (priority BETWEEN 1 AND 5)` constraint in MySQL schema and UI range validation in `FlightDialog`.

#### 9. Likely Individual Viva Questions
1. *What is the relationship between the `aircraft` table and the `flights` table?*
2. *Why is `priority` stored in the `flights` table if there is also an `effective_priority` in `scheduling_queue`?*
3. *What operational flight states does your system support?*

#### 10. Spoken Contribution Summaries
- **60-Second Explanation:**
  > "I was responsible for the Flight and Aircraft Management subsystem. I designed the domain models `Flight` and `Aircraft` and their corresponding normalized tables in MySQL. I implemented the graphical interfaces `FlightsView` and `AircraftView` along with modal dialogs for adding and editing flight records with input validation. I established the operational lifecycle states from Scheduled to Landed or Departed and ensured that static flight attributes cleanly feed into the operating system scheduling queue. I also authored the automated test suites `TestFlightManagement` and `TestAircraftManagement`."
- **2-Minute Explanation:**
  > "In SmartATC, my role focused on modeling the physical and operational aviation entities. An airport cannot manage air traffic without a clean representation of both the physical fleet and the commercial schedule.
  > 
  > I engineered the `aircraft` table, which catalogs tail registrations like `VT-AT1` and wake turbulence classes. I then designed the `flights` table, which links an aircraft to a specific route, arrival/departure type, scheduled time, and base priority. In the UI layer, I developed `FlightsView` and `AircraftView` using Qt's Model-View pattern. I wrote custom dialogs (`FlightDialog`, `AircraftDialog`) that validate flight codes and enforce priority ranges between 1 and 5.
  > 
  > Crucially, my subsystem serves as the entry pipeline for the OS scheduler: when an incoming flight is marked for sequencing, its flight ID, base priority, and arrival time are passed to the scheduling subsystem to generate a Process Control Block. I validated this through `TestFlightManagement` and `TestAircraftManagement`, ensuring full foreign key referential integrity."

---

### Member 3: OS Concepts & Process Scheduling Simulation

#### 1. Assigned Responsibility
Operating System concepts simulation: Process Control Block (PCB), Ready Queue, Non-Preemptive Priority Scheduling, Dynamic Emergency Priority, and Priority Aging.

#### 2. Actual Implemented Features
- Process Control Block (`Models::Process`) tracking arrival timestamps, service burst durations, and process states (`New`, `Ready`, `Running`, `Blocked`, `Completed`).
- Thread-safe `ReadyQueue` priority queue sorted by effective priority (1 = highest), arrival time, and PID.
- Non-Preemptive Priority Scheduler computing metrics (TAT, WT, CT).
- Dynamic Emergency Priority escalation overriding effective priority to 1 while preserving base priority.
- Wall-clock `AgingEngine` with configurable intervals (default: 30s) preventing low-priority starvation.
- Comparative starvation simulation tab in `SchedulingView`.

#### 3. Important Source Files
- `src/models/Process.h` & `src/models/Process.cpp`
- `src/scheduler/ReadyQueue.h` & `src/scheduler/ReadyQueue.cpp`
- `src/scheduler/Scheduler.h` & `src/scheduler/Scheduler.cpp`
- `src/scheduler/AgingEngine.h` & `src/scheduler/AgingEngine.cpp`
- `src/scheduler/StarvationSimulation.h` & `src/scheduler/StarvationSimulation.cpp`
- `ui/SchedulingView.h` & `ui/SchedulingView.cpp`

#### 4. Important Classes and Methods
- `Models::Process::calculateAgingLevels(const QDateTime &now, int intervalSeconds) const`
- `Models::Process::calculateMetrics(const QDateTime &completedAt, QString *outError)`
- `SchedulerComponents::ReadyQueue::push(std::shared_ptr<Models::Process> process)`
- `SchedulerComponents::ReadyQueue::reorder()`
- `SchedulerComponents::AgingEngine::evaluateAging(ReadyQueue &queue, ...)`
- `SchedulerComponents::Scheduler::dispatchNext()`

#### 5. Database Tables Handled
- `scheduling_queue` (`queue_id`, `flight_id`, `arrival_time`, `ready_since`, `base_priority`, `effective_priority`, `aging_level`, `service_time`, `process_state`, `waiting_time`, `turnaround_time`, `completion_time`)

#### 6. Tests Related to Their Work
- `TestProcessModel`, `TestReadyQueueAndProcessStates`, `TestPriorityScheduler`, `TestSchedulingMetrics`, `TestDynamicEmergencyPriority`, `TestPriorityAging`, `TestStarvationSimulation` (7 CTest suites).

#### 7. Integration Work
- Integrated `AgingEngine` with Qt's `QTimer` to run in real time.
- Connected scheduler dispatching with `ApplicationService` to transition flights into runway allocations.
- Created dual-tab `SchedulingView` (Ready Queue inspection and Starvation Demo comparison).

#### 8. Challenges Encountered & Solutions
- *Challenge:* Timer drift causing non-deterministic aging during tests.  
  *Solution:* Formulated aging strictly from continuous elapsed time `readySince.secsTo(now)`, ensuring evaluation is idempotent.
- *Challenge:* Starvation of low-priority flights under heavy traffic.  
  *Solution:* Implemented formula $\max(1, \text{basePriority} - \left\lfloor\Delta t / T_{\text{interval}}\right\rfloor)$ to systematically improve priority without mutating base priority.

#### 9. Likely Individual Viva Questions
1. *Why did you implement non-preemptive scheduling instead of preemptive scheduling?*
2. *What is the formula used by your AgingEngine, and what is the default interval?*
3. *What are the exact process states supported in your C++ model?*

#### 10. Spoken Contribution Summaries
- **60-Second Explanation:**
  > "I was responsible for the Operating System Process and Scheduling simulation in SmartATC. I implemented the Process Control Block structure in C++, tracking process states across New, Ready, Running, Blocked, and Completed. I engineered the `ReadyQueue` and Non-Preemptive Priority Scheduler where Priority 1 is highest, with deterministic tie-breaking on arrival time and PID. I implemented Dynamic Emergency Priority escalation and the real-time `AgingEngine`, which uses a 30-second default interval to calculate elapsed waiting time and elevate starved processes. I authored 7 automated test suites validating all OS scheduling behaviors."
- **2-Minute Explanation:**
  > "My technical focus was translating classical Operating System scheduling theory into our airport simulation. In SmartATC, a flight is modeled as an OS process with arrival time, simulated CPU burst duration, and a Process Control Block.
  > 
  > I engineered the `ReadyQueue`, implementing non-preemptive priority scheduling. We chose non-preemptive scheduling because an aircraft in the final stages of landing cannot be safely aborted mid-runway without severe hazard. To sort processes, I implemented a three-tier comparator: lower numerical priority first, then earliest arrival time, then flight ID.
  > 
  > A major contribution was solving process starvation. Under pure priority scheduling, Priority 5 flights wait indefinitely if higher priority traffic continuously arrives. I developed `AgingEngine`, which monitors `readySince` timestamps and periodically computes `floor(elapsedSeconds / intervalSeconds)` (defaulting to 30s) to decrement the effective priority down to 1. Additionally, I implemented dynamic emergency handling, which elevates effective priority to 1 without mutating the preserved base priority. I validated these algorithms across 7 automated test suites."

---

### Member 4: Concurrency, Resource Allocation & DBMS Transactions

#### 1. Assigned Responsibility
Physical runway resource management, critical section protection, C++ multithreading, and MySQL ACID database transactions.

#### 2. Actual Implemented Features
- Runway infrastructure modeling (`runways` table) and operational status lifecycle (`Available`, `Occupied`, `Maintenance`).
- Critical Section protection using `std::mutex` and custom RAII `ScopedResourceLock` preventing in-memory race conditions.
- Multi-threaded concurrency simulation (`ConcurrentAllocationDemo`) demonstrating simultaneous thread contention using `std::thread`.
- Atomic multi-statement MySQL transactions (`START TRANSACTION`, `COMMIT`, `ROLLBACK`) across `runways`, `runway_assignments`, `flights`, and `operation_logs`.
- Pessimistic row locking via `SELECT ... FOR UPDATE` in MySQL InnoDB.
- System Dashboard and KPI monitoring (`DashboardView`).

#### 3. Important Source Files
- `src/resource/ResourceManager.h` & `src/resource/ResourceManager.cpp`
- `src/resource/ConcurrentAllocationDemo.h` & `src/resource/ConcurrentAllocationDemo.cpp`
- `src/database/DatabaseManager.h` & `src/database/DatabaseManager.cpp`
- `src/models/Runway.h` & `src/models/RunwayAssignment.h`
- `ui/RunwaysView.h` & `ui/RunwaysView.cpp`
- `ui/RunwayAllocationDialog.h` & `ui/RunwayAllocationDialog.cpp`
- `ui/DashboardView.h` & `ui/DashboardView.cpp`

#### 4. Important Classes and Methods
- `ResourceManager::ScopedResourceLock` (RAII wrapper around `std::unique_lock<std::mutex>`)
- `ApplicationService::allocateRunway(int runwayId, int flightId, QString *outError)`
- `ApplicationService::releaseRunway(int runwayId, QString *outError)`
- `DatabaseManager::beginTransaction()`, `commitTransaction()`, `rollbackTransaction()`
- `ApplicationService::setAllocationFailureStage(AllocationFailureStage stage)`

#### 5. Database Tables Handled
- `runways` (`runway_id`, `runway_number`, `runway_type`, `status`)
- `runway_assignments` (`assignment_id`, `flight_id`, `runway_id`, `assigned_by`, `assigned_at`, `released_at`, `status`)

#### 6. Tests Related to Their Work
- `TestRunwayManagement`, `TestRunwayResourceAllocation`, `TestCriticalSectionAndMutualExclusion`, `TestConcurrentRunwayAllocation`, `TestDatabaseTransactions`, `TestACIDProperties`, `TestDashboardMonitoring` (7 CTest suites).

#### 7. Integration Work
- Designed the two-phase synchronization mechanism: executing the MySQL transaction first, committing to disk, and only then updating in-memory `ResourceManager` state.
- Developed failure injection hooks to verify atomic rollback behavior.
- Built `DashboardView` binding real-time KPI metrics and runway utilization gauges to live database queries.

#### 8. Challenges Encountered & Solutions
- *Challenge:* Preventing double allocation when two concurrent threads request the same runway simultaneously.  
  *Solution:* Implemented dual-layer protection: in-memory `std::mutex` scoped lock plus MySQL InnoDB pessimistic row locking (`SELECT ... FOR UPDATE`).
- *Challenge:* Verifying true Atomicity rollback rather than pre-validation rejection.  
  *Solution:* Developed a failure-injection hook (`AllocationFailureStage::DuringUpdateFlight`) that triggers failure midway through mutations, proving that InnoDB undo logs roll back earlier writes.

#### 9. Likely Individual Viva Questions
1. *Why is a runway modeled as a shared OS resource rather than a CPU core?*
2. *What is the exact sequence of SQL queries inside your runway allocation transaction?*
3. *What is the difference between your C++ mutex and MySQL's row-level locking?*

#### 10. Spoken Contribution Summaries
- **60-Second Explanation:**
  > "I took ownership of Concurrency, Runway Resource Allocation, and MySQL Database Transactions in SmartATC. I modeled airport runways as shared hardware resources requiring mutual exclusion. In C++, I protected in-memory allocation using `std::mutex` and an RAII `ScopedResourceLock`, demonstrating race-condition prevention in `ConcurrentAllocationDemo`. At the database layer, I implemented atomic multi-statement MySQL transactions using InnoDB pessimistic row locks (`SELECT ... FOR UPDATE`) across four tables. I also authored the failure injection harness proving atomic rollback, developed the System Dashboard, and authored 7 automated test suites."
- **2-Minute Explanation:**
  > "My primary responsibility was ensuring data integrity and thread safety across the shared physical resources of the airport. A runway cannot be simultaneously used by multiple flights. 
  > 
  > I implemented dual-layer concurrency protection. First, in memory, each runway is guarded by a dedicated `std::mutex`. I implemented an RAII `ScopedResourceLock` using `std::unique_lock`, guaranteeing that locks release automatically even if an exception occurs. To prove this, I created `ConcurrentAllocationDemo`, which spawns competing C++ worker threads and demonstrates that exactly one flight acquires the resource while the second safely blocks.
  > 
  > Second, at the DBMS layer, I engineered the transactional backbone in `ApplicationService`. Runway allocation updates `runway_assignments`, `runways`, `flights`, and `operation_logs`. I wrapped these in an atomic MySQL transaction using `SELECT ... FOR UPDATE` row locks under `REPEATABLE READ`. To verify the ACID properties, I built a failure-injection framework that triggers synthetic failures midway through execution, proving that InnoDB undo logs roll back partial writes. I also developed the real-time `DashboardView` and verified my implementation across 7 CTest suites."
