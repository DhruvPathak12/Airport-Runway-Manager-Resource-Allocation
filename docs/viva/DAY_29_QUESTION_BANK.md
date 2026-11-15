# SmartATC — Comprehensive Viva Question Bank (170 Questions & Answers)

This question bank provides technical questions and answers designed for the B.Tech final-year viva examination.

---

## Part 1: Project-Specific Questions (30 Questions)

#### Q1: What is the Smart Air Traffic Control Management System (SmartATC)?
- **Short Answer:** An educational C++17 and Qt 6 desktop simulation demonstrating Operating System and Database Management System concepts through airport runway management.
- **Detailed Explanation:** SmartATC models commercial flights as OS processes, airport runways as shared hardware resources requiring mutual exclusion, air traffic prioritization as priority scheduling with aging, and runway allocations as multi-table ACID MySQL transactions.
- **SmartATC Example:** Demonstrates BOM (Mumbai Airport) arrival/departure sequencing across Runways 09L, 09R, and 27.

#### Q2: Is SmartATC a real air traffic control system?
- **Answer:** No. It is an educational desktop simulation designed to teach and demonstrate computer science principles. It runs strictly in user space on Windows and does not interface with physical radar, ADS-B transponders, or certified avionics hardware.

#### Q3: What is the primary problem SmartATC addresses?
- **Answer:** Safe, optimal sequencing of arriving and departing aircraft on scarce physical runways without race conditions, process starvation, or database corruption during sudden emergencies or failures.

#### Q4: What is the architectural pattern used in SmartATC?
- **Answer:** A 4-tier layered architecture: Presentation Layer (Qt Widgets), Application Service Layer (`ApplicationService`), Core Simulation Layer (OS Scheduler & Resource Manager), and Persistence Layer (`DatabaseManager` with MySQL InnoDB).

#### Q5: Why is a runway modeled as a shared OS resource rather than a CPU core?
- **Answer:** A CPU core executes instruction cycles and can be preempted via hardware timer interrupts. A runway is a physical, non-shareable resource (analogous to a tape drive or shared memory block) that an aircraft must occupy exclusively for its service duration without interruption.

#### Q6: What airport is modeled in the demonstration scenario?
- **Answer:** Chhatrapati Shivaji Maharaj International Airport (BOM / VABB), Mumbai, India.

#### Q7: What are the three application roles in SmartATC?
- **Answer:** `Admin` (System administrator), `ATC` (Air Traffic Controller), and `Airport Operator` (Ground and maintenance operations).

#### Q8: Where is Role-Based Access Control enforced?
- **Answer:** At two levels: presentation level (UI buttons are hidden/disabled) and service level (`ApplicationService` checks `m_session.hasPermission()` before executing any mutation).

#### Q9: How are passwords stored and verified?
- **Answer:** Passwords are never stored in plaintext. They are hashed using SHA-256 via `QCryptographicHash` and verified against the `password_hash` column in the `users` table.

#### Q10: What is the known cryptographic limitation of the current password implementation?
- **Answer:** It uses unsalted SHA-256. For production enterprise systems, it should be upgraded to an adaptive, salted password hashing scheme such as Argon2id or bcrypt.

#### Q11: How many database tables exist in SmartATC?
- **Answer:** Exactly eight tables: `roles`, `users`, `aircraft`, `flights`, `runways`, `scheduling_queue`, `runway_assignments`, and `operation_logs`.

#### Q12: Which database engine is used and why?
- **Answer:** MySQL 8.0 with the `InnoDB` storage engine for full ACID transactions, crash recovery, and pessimistic row locking (`SELECT ... FOR UPDATE`).

#### Q13: What scheduling policy does SmartATC use?
- **Answer:** Non-Preemptive Priority Scheduling with dynamic priority aging.

#### Q14: What is the priority scale in SmartATC?
- **Answer:** Priority 1 is the Highest Priority (Emergency/VIP); Priority 5 is the Lowest Priority (Routine domestic/cargo).

#### Q15: How are priority ties broken in the Ready Queue?
- **Answer:** First by earliest arrival time (`arrivalTime ASC`, FCFS tie-breaker), then by smaller flight ID (`flightId ASC`, deterministic stability tie-breaker).

#### Q16: What happens when an emergency is declared for a flight?
- **Answer:** The flight's `effectivePriority` is dynamically elevated to 1, moving it to index 0 of the Ready Queue. Its `basePriority` is strictly preserved.

#### Q17: Does declaring an emergency interrupt an aircraft currently landing on the runway?
- **Answer:** No. SmartATC uses non-preemptive scheduling. The currently running aircraft completes its landing safely, and the emergency flight is dispatched at the next scheduling opportunity.

#### Q18: What is process starvation in SmartATC?
- **Answer:** When low-priority flights (e.g. Priority 5) wait indefinitely in the Ready Queue because higher-priority flights continuously arrive.

#### Q19: How does SmartATC solve process starvation?
- **Answer:** Through `AgingEngine`, which periodically evaluates continuous waiting time in `Ready` (`readySince`) and decrements the numerical priority value toward 1 every 30 seconds.

#### Q20: What is the default aging interval?
- **Answer:** 30 seconds (configurable between 1 second and 86,400 seconds).

#### Q21: What formula is used to calculate aging?
- **Answer:** $\text{effectivePriority} = \max(1, \text{basePriority} - \left\lfloor\frac{\text{elapsedWaitingTimeSeconds}}{\text{configuredIntervalSeconds}}\right\rfloor)$.

#### Q22: Can effective priority become 0 or negative through aging?
- **Answer:** No. It is bounded by $\max(1, \dots)$; Priority 1 is the highest possible priority.

#### Q23: How does SmartATC prevent two flights from acquiring the same runway simultaneously?
- **Answer:** Dual-layer protection: in memory via `std::mutex` and RAII `ScopedResourceLock`, and in MySQL via pessimistic row locking (`SELECT ... FOR UPDATE`).

#### Q24: What is `ScopedResourceLock`?
- **Answer:** An RAII wrapper around `std::unique_lock<std::mutex>` that automatically acquires the lock on construction and releases it on destruction, preventing deadlocks.

#### Q25: What happens during a runway allocation transaction?
- **Answer:** It begins a transaction, locks the runway row `FOR UPDATE`, validates availability, inserts into `runway_assignments`, updates `runways.status` to `Occupied`, updates `flights.operational_status` to `Cleared`, inserts into `operation_logs`, and commits atomically.

#### Q26: What happens if a database update fails midway through an allocation?
- **Answer:** `db.rollback()` is invoked; MySQL's InnoDB undo logs revert all intermediate writes, leaving zero orphan records or partial updates.

#### Q27: How is in-memory state synchronized with the database?
- **Answer:** Two-phase execution: the MySQL transaction must commit successfully to disk before `ApplicationService` updates in-memory `ResourceManager` and `Process` objects. If the transaction rolls back, memory is not modified.

#### Q28: How many automated test suites exist in SmartATC?
- **Answer:** 19 CTest test suites, covering unit logic, integration, concurrency, and ACID verification (100% pass rate).

#### Q29: How does the dashboard retrieve operational metrics?
- **Answer:** `DashboardView` queries `ApplicationService`, which computes metrics directly from live database state and active PCB queues without hardcoded values.

#### Q30: What build tools are used to compile SmartATC?
- **Answer:** CMake 3.19+, Ninja build generator, and MinGW-w64 GCC 13.1.0 under Qt 6.11.2.

---

## Part 2: Operating System Questions (25 Questions)

#### Q31: What is a Process Control Block (PCB)?
- **Answer:** A kernel/system data structure that stores all execution and scheduling metadata for a specific process (PID, state, priorities, arrival time, burst duration, metrics).

#### Q32: What information does the `Models::Process` struct store in SmartATC?
- **Answer:** `flightId`, `queueId`, `arrivalTime`, `basePriority`, `effectivePriority`, `readySince`, `agingLevel`, `isEmergency`, `serviceTime`, `state`, `waitingTime`, and `turnaroundTime`.

#### Q33: What are the actual C++ process lifecycle states in SmartATC?
- **Answer:** `New`, `Ready`, `Running`, `Blocked`, and `Completed`.

#### Q34: What is the difference between `Ready` and `Blocked` states?
- **Answer:** A `Ready` process has all required resources and is runnable, waiting only for CPU/runway dispatch. A `Blocked` process cannot run because it is waiting on an external event or unavailable resource (e.g., runway in maintenance).

#### Q35: How does the database represent process states?
- **Answer:** Table `scheduling_queue` uses `ENUM('Ready', 'Running', 'Completed', 'Blocked')`. `ProcessState::New` maps to `'Ready'` upon insertion.

#### Q36: What is Turnaround Time (TAT)?
- **Answer:** The total elapsed time from when a process arrives in the system to when it completes: $TAT = t_{\text{completion}} - t_{\text{arrival}}$.

#### Q37: What is Waiting Time (WT)?
- **Answer:** The total time a process spends waiting in the ready queue without being serviced: $WT = TAT - t_{\text{service}}$.

#### Q38: Can Waiting Time ever be negative?
- **Answer:** No. Since $TAT \ge t_{\text{service}}$, Waiting Time is always $\ge 0$.

#### Q39: What is preemption in OS scheduling?
- **Answer:** The act of an operating system kernel interrupting an actively running process and moving it back to the ready queue to allocate the CPU to another process.

#### Q40: Why does SmartATC not use preemptive scheduling?
- **Answer:** In physical aviation, an aircraft in final touchdown rollout or takeoff cannot safely abort mid-runway. Non-preemptive scheduling accurately mirrors runway safety physics.

#### Q41: What is the First-Come, First-Served (FCFS) scheduling anomaly?
- **Answer:** The Convoy Effect, where short processes wait behind a very long process. SmartATC avoids this by using priority scheduling rather than pure FCFS.

#### Q42: What is priority inversion?
- **Answer:** When a high-priority process is indirectly blocked by a low-priority process holding a required resource.

#### Q43: What is the difference between static priority and dynamic priority?
- **Answer:** Static priority (`basePriority`) remains constant throughout execution. Dynamic priority (`effectivePriority`) is calculated and adjusted at runtime based on aging or emergency events.

#### Q44: What is an idempotent aging algorithm?
- **Answer:** An algorithm where calculating aging from absolute timestamps (`readySince.secsTo(now)`) produces the exact same correct result regardless of whether the evaluation timer ticks early, late, or restarts.

#### Q45: What is a Critical Section?
- **Answer:** A region of code accessing a shared resource that must not be accessed concurrently by more than one execution thread.

#### Q46: What are the three requirements for a valid critical section solution?
- **Answer:** Mutual Exclusion, Progress, and Bounded Waiting.

#### Q47: What is Mutual Exclusion?
- **Answer:** Ensuring that if process $P_i$ is executing in its critical section, no other process $P_j$ can execute in that critical section.

#### Q48: What is a Mutex?
- **Answer:** A locking mechanism used to synchronize access to a resource. Only the thread that acquired the mutex can release it.

#### Q49: What is the difference between a mutex and a binary semaphore?
- **Answer:** A mutex has ownership: only the thread that locked it can unlock it. A semaphore has no ownership; any thread can signal (post) an unlock.

#### Q50: What is a Race Condition?
- **Answer:** When multiple threads access and manipulate shared data concurrently, and the final outcome depends on the non-deterministic order of thread execution.

#### Q51: How does SmartATC prevent race conditions during runway allocation?
- **Answer:** By wrapping in-memory runway reservations in a critical section guarded by `std::mutex` via `ScopedResourceLock`.

#### Q52: What is a Deadlock?
- **Answer:** A state where two or more processes are permanently blocked, each waiting for a resource held by another process in the set.

#### Q53: What are the four Coffman conditions for deadlock?
- **Answer:** Mutual Exclusion, Hold and Wait, No Preemption, and Circular Wait.

#### Q54: How does SmartATC prevent deadlock in multi-resource scenarios?
- **Answer:** By enforcing a linear lock acquisition hierarchy: always acquire the airspace queue lock first, then the runway lock.

#### Q55: What is Context Switching?
- **Answer:** The procedure of saving the state of the currently executing process and loading the state of the next scheduled process.

---

## Part 3: DBMS & Relational Questions (25 Questions)

#### Q56: What is a Relational Database?
- **Answer:** A database structured according to the relational model, storing data in two-dimensional tables (relations) consisting of rows (tuples) and columns (attributes) linked by keys.

#### Q57: What is a Primary Key?
- **Answer:** A column or set of columns that uniquely identifies each row in a table. It cannot contain NULL values.

#### Q58: What is a Foreign Key?
- **Answer:** A column in one table that references the primary key of another table, establishing a relationship and enforcing referential integrity.

#### Q59: What is Referential Integrity?
- **Answer:** A relational database property ensuring that foreign key references always point to valid, existing primary key records in the parent table.

#### Q60: What does `ON DELETE RESTRICT` mean?
- **Answer:** Prevents deletion of a parent record if any child records reference its primary key.

#### Q61: What does `ON DELETE SET NULL` mean?
- **Answer:** If the referenced parent record is deleted, the foreign key column in the child table is automatically set to NULL.

#### Q62: What is Database Normalization?
- **Answer:** The systematic process of structuring a relational database schema to minimize data redundancy and eliminate insertion, update, and deletion anomalies.

#### Q63: What is First Normal Form (1NF)?
- **Answer:** A table where all attributes contain atomic (indivisible) scalar values with no repeating groups or nested arrays.

#### Q64: What is Second Normal Form (2NF)?
- **Answer:** The table is in 1NF, and every non-prime attribute is fully functionally dependent on the entire primary key (no partial key dependencies).

#### Q65: What is Third Normal Form (3NF)?
- **Answer:** The table is in 2NF, and no non-prime attribute is transitively dependent on the primary key ($X \to Y \to Z$).

#### Q66: Give an example of 3NF normalization in SmartATC.
- **Answer:** In `users`, we store `role_id` referencing `roles`. We do not duplicate `role_name` in `users`, avoiding the transitive dependency: $\text{user\_id} \to \text{role\_id} \to \text{role\_name}$.

#### Q67: What is a Unique Constraint?
- **Answer:** A rule ensuring that all values in a column or set of columns are distinct across the table (e.g. `aircraft.registration_number`).

#### Q68: What is a Database Index?
- **Answer:** A data structure (typically a B+Tree) that improves data retrieval speed on a table at the expense of additional storage and write overhead.

#### Q69: What is the primary clustering index in InnoDB?
- **Answer:** The primary key index, where actual table rows are stored physically within the leaf pages of the primary B+Tree.

#### Q70: What is a Secondary Index in InnoDB?
- **Answer:** An index on non-primary columns where leaf nodes store the index key value and the corresponding primary key pointer.

#### Q71: What is a Surrogate Key?
- **Answer:** An artificial, system-generated primary key (such as `AUTO_INCREMENT INT`) with no intrinsic business meaning, used for stable foreign key references.

#### Q72: What is a Natural Key?
- **Answer:** A primary key formed from existing real-world business attributes (such as aircraft registration `VT-AT1` or runway number `09L`).

#### Q73: Why does SmartATC use surrogate primary keys?
- **Answer:** Surrogate integer keys provide fast integer joins, compact foreign key indexes, and protect relational references if natural codes are renamed.

#### Q74: What is SQL Injection?
- **Answer:** A security vulnerability where malicious SQL commands are injected into data input fields, altering the structure of the executed query.

#### Q75: How do Prepared Statements prevent SQL Injection?
- **Answer:** The database compiles the SQL syntax structure first; user inputs are bound separately as literal data values and cannot modify the SQL execution tree.

#### Q76: What is a Database View?
- **Answer:** A virtual table defined by a stored SQL query, presenting filtered or joined data without duplicating underlying storage.

#### Q77: What is an INNER JOIN?
- **Answer:** A query that returns records that have matching values in both joined tables.

#### Q78: What is a LEFT OUTER JOIN?
- **Answer:** A query that returns all records from the left table, plus matched records from the right table (or NULL if no match exists).

#### Q79: What is the purpose of `CHECK` constraints in SmartATC?
- **Answer:** To enforce domain integrity directly at the database engine level (e.g. `CHECK (priority BETWEEN 1 AND 5)` and `CHECK (capacity > 0)`).

#### Q80: What is Database Cardinality?
- **Answer:** The numerical relationship between data entities (1:1, 1:N, M:N). SmartATC's `runway_assignments` resolves the M:N relationship between flights and runways.

---

## Part 4: MySQL Transactions & ACID Questions (15 Questions)

#### Q81: What does ACID stand for?
- **Answer:** Atomicity, Consistency, Isolation, and Durability.

#### Q82: What is Atomicity?
- **Answer:** The guarantee that all operations in a transaction either commit completely or roll back entirely ("all or nothing").

#### Q83: What is Consistency in ACID?
- **Answer:** The guarantee that a transaction transforms the database from one valid state to another, preserving all schemas, constraints, and cascades.

#### Q84: What is Isolation in ACID?
- **Answer:** The property ensuring that intermediate uncommitted transaction updates are invisible to concurrent transactions.

#### Q85: What is Durability in ACID?
- **Answer:** The guarantee that once a transaction commits, its writes persist permanently on non-volatile storage, even during crashes.

#### Q86: What are the four ANSI SQL Transaction Isolation Levels?
- **Answer:** Read Uncommitted, Read Committed, Repeatable Read, and Serializable.

#### Q87: What is the default transaction isolation level in MySQL InnoDB?
- **Answer:** `REPEATABLE READ`.

#### Q88: What is a Phantom Read?
- **Answer:** When Transaction A queries a range of rows twice, and Transaction B inserts a new row satisfying the filter between the two reads. InnoDB prevents this using Next-Key locking.

#### Q89: What is Pessimistic Locking?
- **Answer:** A concurrency control strategy that locks database rows immediately upon access (`SELECT ... FOR UPDATE`), preventing other transactions from modifying them until the lock is released.

#### Q90: What is Optimistic Locking?
- **Answer:** A concurrency control strategy where transactions execute without row locks, checking version counters at commit time to detect conflicts.

#### Q91: Why does SmartATC use Pessimistic Locking (`SELECT ... FOR UPDATE`)?
- **Answer:** Airport runway allocation is a high-contention, safety-critical resource. Pessimistic locking eliminates collision retries by serializing access at the database row level.

#### Q92: What does `COMMIT` do in MySQL?
- **Answer:** Permanently applies transaction updates to the database, flushes write-ahead redo logs to disk, and releases all acquired row locks.

#### Q93: What does `ROLLBACK` do in MySQL?
- **Answer:** Reverts all modifications made in the active transaction by consulting InnoDB undo logs, restoring rows to their pre-transaction values.

#### Q94: What is the InnoDB Redo Log?
- **Answer:** A write-ahead disk-based log used during crash recovery to re-apply committed transactions that were not yet flushed from the buffer pool to data files.

#### Q95: What is the InnoDB Undo Log?
- **Answer:** A disk-based log storing the previous versions of modified records, used to roll back transactions and provide MVCC snapshot reads.

---

## Part 5: C++ & Qt Framework Questions (20 Questions)

#### Q96: What is RAII in C++?
- **Answer:** Resource Acquisition Is Initialization: binding resource lifecycle (memory, mutex locks, database connections) to object lifetime, ensuring automatic cleanup when an object goes out of scope.

#### Q97: What is the difference between `std::unique_ptr` and `std::shared_ptr`?
- **Answer:** `std::unique_ptr` enforces exclusive ownership (non-copyable, movable). `std::shared_ptr` implements reference-counted shared ownership.

#### Q98: Why are Qt's Signals and Slots type-safe?
- **Answer:** Because the Qt Meta-Object Compiler (MOC) generates code that validates signature compatibility between signal parameters and slot arguments at compile time.

#### Q99: What is `Q_OBJECT` macro?
- **Answer:** A mandatory macro placed in the private header of any Qt class that uses signals, slots, dynamic properties, or meta-object reflection.

#### Q100: What is MOC in Qt?
- **Answer:** Meta-Object Compiler: a preprocessor that scans C++ headers for the `Q_OBJECT` macro and generates C++ source code implementing meta-information and signal dispatching.

#### Q101: What is a `Qt::QueuedConnection`?
- **Answer:** A signal-slot connection mode where the signal is posted to the receiver thread's event queue, executing the slot asynchronously in the receiver's thread. Essential for thread safety.

#### Q102: What is the difference between `QWidget`, `QDialog`, and `QMainWindow`?
- **Answer:** `QWidget` is the base UI element. `QDialog` is a top-level window designed for modal/modeless user prompts. `QMainWindow` provides a standard window structure with menu bars, status bars, and dock widgets.

#### Q103: What is `QSqlDatabase`?
- **Answer:** Qt's database connection manager representing a connection to a database server.

#### Q104: What is `QSqlQuery`?
- **Answer:** A Qt class that provides means to execute SQL DML/DDL statements, prepare parameterized queries, and navigate query result sets.

#### Q105: What is the Singleton Pattern and where is it used in SmartATC?
- **Answer:** A creational design pattern ensuring a class has only one instance with global access. Used in `DatabaseManager::instance()` and `ApplicationService::instance()`.

#### Q106: What is the Façade Pattern and where is it used?
- **Answer:** A structural design pattern providing a simplified interface to a complex subsystem. `ApplicationService` acts as a façade coordinating authentication, scheduling, resources, and database operations.

#### Q107: What is `std::unique_lock` and how does it differ from `std::lock_guard`?
- **Answer:** `std::lock_guard` is a strict RAII wrapper that locks on creation and unlocks on destruction. `std::unique_lock` offers flexible locking: manual unlocking, deferred locking, and compatibility with `std::condition_variable`.

#### Q108: What is a Data Race?
- **Answer:** When two or more concurrent threads access the same memory location, at least one access is a write, and no synchronization mechanisms are used.

#### Q109: What is `std::jthread` in C++20 vs. `std::thread` in C++17?
- **Answer:** `std::thread` requires explicit `.join()` or `.detach()`, else calling its destructor terminates the program. `std::jthread` joins automatically on destruction and supports cooperative cancellation.

#### Q110: How does SmartATC manage thread lifecycles safely in C++17?
- **Answer:** In `ConcurrentAllocationDemo`, spawned worker threads are explicitly tracked and joined via `join()` before destruction, preventing dangling threads.

#### Q111: What is `QTimer`?
- **Answer:** A Qt class providing repetitive and single-shot timers that emit a `timeout()` signal within the Qt event loop.

#### Q112: Why did SmartATC use CMake rather than qmake?
- **Answer:** CMake is the modern, official build system for Qt 6, offering modular targets, cross-platform build generation (Ninja, Visual Studio, Make), and declarative CTest test suite integration.

#### Q113: What does `CMAKE_AUTOMOC` do in CMake?
- **Answer:** Instructs CMake to automatically run the Qt Meta-Object Compiler on headers containing the `Q_OBJECT` macro before compilation.

#### Q114: What is an explicit constructor in C++ (`explicit`)?
- **Answer:** A constructor declared with `explicit` to prevent implicit type conversions and copy-initialization.

#### Q115: What is the virtual destructor requirement in polymorphic base classes?
- **Answer:** Base classes with virtual functions must declare a virtual destructor (`virtual ~Base() = default;`) so that deleting a derived object via a base pointer invokes the derived destructor, preventing resource leaks.

---

## Part 6: Integration & Testing Questions (15 Questions)

#### Q116: What testing framework was used for SmartATC?
- **Answer:** Qt Test (`Qt6::Test`) combined with CMake's `CTest` test runner.

#### Q117: How many CTest suites exist and what is their pass rate?
- **Answer:** Exactly 19 CTest test suites, with a 100% pass rate.

#### Q118: What is the difference between a Unit Test and an Integration Test?
- **Answer:** A Unit Test validates an isolated function or class in memory (e.g. `TestProcessModel`). An Integration Test verifies interactions across multiple modules and external systems (e.g. `TestCompleteIntegration` testing Service $\to$ Scheduler $\to$ Database).

#### Q119: What is Regression Testing?
- **Answer:** Re-running existing test suites to confirm that recent bug fixes or code changes have not broken existing functionality.

#### Q120: How was database transaction rollback tested?
- **Answer:** Via `TestACIDProperties`, using `ApplicationService::setAllocationFailureStage(DuringUpdateFlight)` to inject artificial failures and verify zero partial records in MySQL.

#### Q121: How was multi-threaded mutual exclusion verified in tests?
- **Answer:** In `TestConcurrentRunwayAllocation`, multiple worker threads concurrently requested the same runway; assertions verified that exactly one thread acquired the resource and the other was rejected.

#### Q122: How was priority aging verified in tests?
- **Answer:** In `TestPriorityAging`, synthetic evaluation timestamps were passed to `evaluateAging()`, asserting that effective priority decreased by 1 for each interval elapsed.

#### Q123: What is the purpose of `scripts/deploy_runtime.ps1`?
- **Answer:** A PowerShell script that copies required Qt DLLs, MinGW runtime libraries, and platform plugins into the build directory so `SmartATC.exe` runs standalone without modifying system PATH.

#### Q124: How was standalone application launch verified?
- **Answer:** By launching `SmartATC.exe` in a clean environment where developer Qt and MinGW directories were completely removed from PATH, confirming no missing DLL errors.

#### Q125: What is `db.conf` and why is it not tracked in Git?
- **Answer:** The local database configuration file. It is excluded by `.gitignore` to prevent leaking database passwords into public version control.

#### Q126: What is `db.conf.example`?
- **Answer:** A clean configuration template containing placeholder connection parameters committed to Git for reproducible setup.

#### Q127: What is Mocking vs. Live Database Testing?
- **Answer:** Mocking simulates database responses using synthetic in-memory stubs. Live database testing executes actual queries against a running MySQL instance.

#### Q128: Did SmartATC tests use live database connections?
- **Answer:** Yes. Test suites like `TestDatabaseConnectivity`, `TestDatabaseTransactions`, and `TestACIDProperties` executed live SQL transactions against local MySQL.

#### Q129: What happens if MySQL is offline when SmartATC launches?
- **Answer:** `DatabaseManager` logs an offline status; `ApplicationService` detects connection failure and gracefully falls back to in-memory simulation mode without crashing.

#### Q130: What is continuous integration in a capstone project?
- **Answer:** The practice of automating compilation and test execution on every Git commit to detect defects early.

---

## Part 7: Individual Contribution Questions (40 Questions — 10 Per Member)

### Member 1: User, Role & Security Management

#### Q131: What role did you play in database schema design?
- **Answer:** I authored the SQL DDL for the `roles`, `users`, and `operation_logs` tables in `sql/schema.sql`, establishing foreign key constraints and audit columns.

#### Q132: What classes did you implement in C++?
- **Answer:** `Models::User`, `Models::Session`, `Utils::PasswordUtils`, and `ui/LoginView`.

#### Q133: How does `Session::hasPermission()` work?
- **Answer:** It evaluates the current user's role against a static permission mapping table in `Session.cpp`, returning true if the requested permission key is authorized.

#### Q134: How did you implement password hashing?
- **Answer:** In `PasswordUtils::hash()`, using Qt's `QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()`.

#### Q135: How does your UI prevent invalid credentials?
- **Answer:** `LoginView` validates that fields are non-empty, calls `ApplicationService::login()`, and displays red status labels on authentication failure without exposing sensitive details.

#### Q136: What is recorded in `operation_logs`?
- **Answer:** `log_id`, `user_id`, `operation_type`, `flight_id`, `runway_id`, `transaction_status` ('Committed'/'Rolled Back'), `message`, and timestamp.

#### Q137: What test suite did you author?
- **Answer:** `TestAuthAndRBAC`, validating valid logins, invalid passwords, inactive users, and permission checks.

#### Q138: What challenge did you face with SQL parameter binding?
- **Answer:** Ensuring that `user_id` in `operation_logs` could accept NULL when system-automated actions occur, handled via `QVariant(QMetaType(QMetaType::Int))`.

#### Q139: Why is `operation_logs` foreign key configured as `ON DELETE SET NULL`?
- **Answer:** To preserve the historical audit trail even if a user account is deleted in the future.

#### Q140: What would you improve in Member 1's subsystem for production?
- **Answer:** Upgrade from unsalted SHA-256 to salted Argon2id with adaptive work factors, and implement multi-factor authentication (MFA).

---

### Member 2: Flight & Aircraft Domain Management

#### Q141: What classes did you author in the C++ layer?
- **Answer:** `Models::Flight`, `Models::Aircraft`, `ui/FlightsView`, `ui/FlightDialog`, `ui/AircraftView`, and `ui/AircraftDialog`.

#### Q142: What tables did you design in MySQL?
- **Answer:** The `aircraft` and `flights` tables in `sql/schema.sql`.

#### Q143: What operational statuses does a flight transition through?
- **Answer:** `Scheduled` $\to$ `Taxiing` $\to$ `Cleared` $\to$ `Landed` (for arrivals) or `Departed` (for departures), plus `Cancelled`.

#### Q144: How does `FlightDialog` validate user input?
- **Answer:** It validates non-empty flight codes, route fields, arrival/departure selection, positive scheduled times, and priority integer bounds (1–5).

#### Q145: What wake turbulence categories are supported?
- **Answer:** Heavy, Medium, and Light, based on aircraft gross takeoff weight specifications.

#### Q146: What tests did you write to verify your modules?
- **Answer:** `TestFlightManagement` and `TestAircraftManagement`.

#### Q147: How does a flight become a process in the OS scheduler?
- **Answer:** `ApplicationService` extracts `flight_id`, `priority`, and scheduled arrival time from `Flight`, passing them to create a `Process` PCB in `scheduling_queue`.

#### Q148: Why is `aircraft_id` in `flights` nullable?
- **Answer:** Because flight schedules are frequently published months in advance before a physical tail number is assigned to the flight.

#### Q149: What challenge did you encounter with flight table updates?
- **Answer:** Handling terminal flight states (`Landed`, `Departed`); ensuring that terminal flights cannot be re-allocated to runways.

#### Q150: What would you add to Member 2's subsystem in the future?
- **Answer:** Integration with live ADS-B flight telemetry streams and gate assignment tracking.

---

### Member 3: OS Concepts & Process Scheduling Simulation

#### Q151: What classes did you implement for OS simulation?
- **Answer:** `Models::Process`, `SchedulerComponents::ReadyQueue`, `SchedulerComponents::Scheduler`, `SchedulerComponents::AgingEngine`, `SchedulerComponents::StarvationSimulation`, and `ui/SchedulingView`.

#### Q152: What is the structure of the Process Control Block?
- **Answer:** A C++ struct `Process` encapsulating PID (`flightId`), queue ID, base priority, effective priority, arrival time, burst time, state, and metrics.

#### Q153: What are the C++ process states in your model?
- **Answer:** `New`, `Ready`, `Running`, `Blocked`, and `Completed`.

#### Q154: How does `ReadyQueue` sort processes?
- **Answer:** Using a strict comparator: `effectivePriority ASC`, then `arrivalTime ASC`, then `flightId ASC`.

#### Q155: How does Dynamic Emergency Priority work?
- **Answer:** It overrides `effectivePriority = 1`, moving the flight to index 0 of the Ready Queue without preempting the currently running aircraft. Base priority is preserved.

#### Q156: How does your `AgingEngine` calculate accrued aging?
- **Answer:** $\left\lfloor\text{readySince.secsTo}(now) / \text{intervalSeconds}\right\rfloor$, decremented from `basePriority` down to a minimum of 1.

#### Q157: What is the default aging interval and how is it configured?
- **Answer:** 30 seconds default, configurable via `AgingEngine::setIntervalSeconds(int seconds)`.

#### Q158: What tests did you author?
- **Answer:** 7 CTest suites: `TestProcessModel`, `TestReadyQueueAndProcessStates`, `TestPriorityScheduler`, `TestSchedulingMetrics`, `TestDynamicEmergencyPriority`, `TestPriorityAging`, and `TestStarvationSimulation`.

#### Q159: What was the main challenge with priority aging?
- **Answer:** Preventing timer drift and duplicate aging increments; solved by computing aging strictly from absolute elapsed timestamps (`readySince`).

#### Q160: What is the Starvation Demo tab in `SchedulingView`?
- **Answer:** An interactive comparison tool showing low-priority flights starved under high inflow in Mode A (without aging) vs. reaching dispatch in Mode B (with aging).

---

### Member 4: Concurrency, Resource Allocation & DBMS Transactions

#### Q161: What classes did you author?
- **Answer:** `ResourceManager`, `ConcurrentAllocationDemo`, `DatabaseManager`, `Models::Runway`, `Models::RunwayAssignment`, `ui/RunwaysView`, `ui/RunwayAllocationDialog`, and `ui/DashboardView`.

#### Q162: What tables did you author in MySQL?
- **Answer:** `runways` and `runway_assignments` in `sql/schema.sql`.

#### Q163: How is mutual exclusion implemented in C++?
- **Answer:** Using `std::mutex` and an RAII `ScopedResourceLock` (`std::unique_lock`) inside `ResourceManager`.

#### Q164: What is the exact sequence of your allocation transaction?
- **Answer:** `START TRANSACTION` $\to$ `SELECT ... FOR UPDATE` row locks $\to$ validate availability $\to$ `INSERT runway_assignments` $\to$ `UPDATE runways (Occupied)` $\to$ `UPDATE flights (Cleared)` $\to$ `INSERT operation_logs` $\to$ `COMMIT`.

#### Q165: How did you test transaction rollback?
- **Answer:** By creating failure-injection hooks (`AllocationFailureStage::DuringUpdateFlight`) that simulate crashes midway through execution and verify that all partial writes roll back.

#### Q166: What is the purpose of `ConcurrentAllocationDemo`?
- **Answer:** A multi-threaded simulation dialog that spawns simultaneous `std::thread` worker threads competing for the same runway to demonstrate race condition prevention.

#### Q167: How does `DashboardView` calculate runway utilization?
- **Answer:** It executes a live SQL query calculating $\frac{\text{Occupied Runways}}{\text{Total Available + Occupied Runways}} \times 100\%$, omitting runways in Maintenance.

#### Q168: What tests did you author?
- **Answer:** 7 CTest suites: `TestRunwayManagement`, `TestRunwayResourceAllocation`, `TestCriticalSectionAndMutualExclusion`, `TestConcurrentRunwayAllocation`, `TestDatabaseTransactions`, `TestACIDProperties`, and `TestDashboardMonitoring`.

#### Q169: What challenge did you face with row-level locking?
- **Answer:** Preventing deadlock during concurrent allocations; solved by enforcing a strict linear resource hierarchy (locking runways first, flights second).

#### Q170: Why did you synchronize in-memory state only after MySQL commits?
- **Answer:** To prevent memory/database drift: if the database transaction fails and rolls back, in-memory state is left untouched.
