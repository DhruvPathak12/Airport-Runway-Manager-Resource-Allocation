# SmartATC — Top 20 Examiner Traps & Defense Strategies

In technical project vivas, external examiners often ask subtle, trap-oriented questions to test whether students truly understand the boundary between theoretical concepts, simulation abstractions, and actual implementation details.

---

### Trap 1: "Is this a real air traffic control system?"
- **Trap:** The examiner tests whether you overclaim your project's scope.
- **Accurate Viva Response:**
  > "No, sir/ma'am. SmartATC is strictly an educational desktop simulation designed to demonstrate Operating System and Database Management System concepts. It runs entirely in user space on Windows and does not interface with physical radar, aircraft transponders, or certified aviation safety infrastructure. It uses the intuitive domain of an airport to model computer science principles."

---

### Trap 2: "Does your project create real operating system processes using `fork()` or `CreateProcess()`?"
- **Trap:** Confusing real OS kernel processes with in-memory simulated processes.
- **Accurate Viva Response:**
  > "No. Our project does not spawn kernel-level OS processes for each flight. In SmartATC, a flight is represented by a C++ structure `Models::Process`, which acts as an in-memory Process Control Block (PCB). Spawning 50 real OS processes would introduce unnecessary OS context-switching overhead without pedagogical benefit. We simulate process scheduling, state transitions, and metrics within the application layer."

---

### Trap 3: "Why is a runway treated as a shared OS resource rather than a CPU core?"
- **Trap:** Confusing the computational execution engine (CPU) with a non-shareable physical resource.
- **Accurate Viva Response:**
  > "A CPU core executes instructions cycle-by-cycle and can be preempted via hardware timer interrupts. A runway, by contrast, is a non-shareable physical resource (analogous to a magnetic tape drive or shared memory block). Aircraft cannot share a runway simultaneously without collision, and once an aircraft touches down, it holds the runway non-preemptively for its service burst duration until rollout completes."

---

### Trap 4: "Why did you choose non-preemptive scheduling instead of preemptive scheduling?"
- **Trap:** Preemptive scheduling is often considered 'more advanced', so examiners ask why you chose non-preemptive.
- **Accurate Viva Response:**
  > "In computing theory, preemptive scheduling is common for general-purpose OS timesharing. However, in aviation safety, an aircraft in final touchdown flare or high-speed takeoff roll cannot be aborted mid-runway without severe catastrophic risk. Non-preemptive scheduling accurately models runway physics: the running flight finishes its critical maneuver safely, and higher-priority processes are scheduled at the next available opportunity."

---

### Trap 5: "Does an emergency flight interrupt a flight currently landing on the runway?"
- **Trap:** Testing if you understand your own non-preemptive implementation.
- **Accurate Viva Response:**
  > "No. Because SmartATC is non-preemptive, an emergency declaration elevates the distressed flight's effective priority to 1 and places it at index 0 of the Ready Queue. However, it does not abort or interrupt the currently running flight. The emergency flight is dispatched at the very next scheduling opportunity when the runway is freed."

---

### Trap 6: "Why is a lower priority number considered higher priority in your system?"
- **Trap:** Questioning the priority numerical scale convention.
- **Accurate Viva Response:**
  > "In SmartATC, Priority 1 is highest and Priority 5 is lowest. This directly mirrors established operating system standards such as UNIX nice levels and POSIX realtime priorities, where smaller numerical rank corresponds to higher dispatch precedence. In our comparator, sorting by `effectivePriority ASC` naturally places Priority 1 at the head of the queue."

---

### Trap 7: "Does priority aging modify the flight's base priority in the database?"
- **Trap:** Testing whether dynamic aging corrupts permanent static metadata.
- **Accurate Viva Response:**
  > "No. In SmartATC, we maintain a strict architectural separation between `basePriority` (static flight tier, 1–5) and `effectivePriority` (dynamic scheduling priority). Aging only decrements `effectivePriority`. `basePriority` is strictly preserved in `flights.priority` and `scheduling_queue.base_priority` so the original flight classification is never lost."

---

### Trap 8: "How does priority aging prevent starvation?"
- **Trap:** Expecting you to explain the exact mathematical mechanism.
- **Accurate Viva Response:**
  > "In static priority scheduling, low-priority processes (Priority 4 or 5) can wait indefinitely if high-priority processes continuously arrive. Our `AgingEngine` calculates elapsed continuous waiting time in the Ready state from `readySince.secsTo(now)`. Every 30 seconds, it increases the accrued aging level and decrements the effective priority by 1 ($\max(1, \text{base} - \text{agingLevels})$). Eventually, the starving flight reaches Priority 1 and is dispatched via arrival-time tie-breaking."

---

### Trap 9: "Is a C++ mutex sufficient to protect runways if two controllers run SmartATC on different computers?"
- **Trap:** Testing whether you understand the boundary between in-memory synchronization and distributed persistence.
- **Accurate Viva Response:**
  > "No. A C++ `std::mutex` only protects memory within a single operating system process address space. If two controllers run separate instances of SmartATC connected to the same MySQL server, their C++ mutexes know nothing about each other. That is why we also implement pessimistic row locking (`SELECT ... FOR UPDATE`) in MySQL InnoDB. The database engine serializes concurrent requests across different machines."

---

### Trap 10: "Why do you need database transactions if you already checked runway status in C++?"
- **Trap:** Testing race condition awareness between application memory and the database.
- **Accurate Viva Response:**
  > "Between checking runway status in C++ and persisting the allocation, another thread or process could modify the database. Furthermore, runway allocation requires modifying four tables simultaneously (`runway_assignments`, `runways`, `flights`, and `operation_logs`). Without an atomic database transaction, any midway failure or crash would leave the database corrupted with partial writes."

---

### Trap 11: "What is the difference between a validation rejection and a transaction rollback?"
- **Trap:** A critical distinction examiners look for when evaluating DBMS transactions.
- **Accurate Viva Response:**
  > "A validation rejection occurs before any database write statements execute (e.g. detecting that a runway is already Occupied or in Maintenance), returning an error immediately. A transaction rollback occurs after one or more mutations have already executed within an open transaction; when a subsequent statement fails, `db.rollback()` triggers MySQL InnoDB undo logs to physically revert earlier writes and restore the pre-transaction baseline."

---

### Trap 12: "How is Durability implemented and how did you verify it?"
- **Trap:** Confusing true power-loss server crash recovery with application restart persistence.
- **Accurate Viva Response:**
  > "In MySQL InnoDB, Durability is guaranteed by write-ahead redo logs that flush committed transactions to non-volatile disk. In our project, we verified durability by committing runway allocations, terminating the `SmartATC.exe` application process, and relaunching it to confirm that all committed statuses and assignments persisted identically upon reconnection. We did not perform hardware power-cut crash tests."

---

### Trap 13: "What transaction isolation level does SmartATC use?"
- **Trap:** Testing whether you know your database engine defaults.
- **Accurate Viva Response:**
  > "SmartATC runs on MySQL InnoDB's default `REPEATABLE READ` transaction isolation level. To prevent dirty reads, non-repeatable reads, and lost updates during high-contention allocations, we explicitly issue `SELECT ... FOR UPDATE` row locks, which acquire exclusive X-locks and serialize concurrent transactions at the database level."

---

### Trap 14: "Why did you use the InnoDB engine instead of MyISAM?"
- **Trap:** Testing core DBMS storage engine knowledge.
- **Accurate Viva Response:**
  > "MyISAM does not support transactions (`COMMIT` or `ROLLBACK`), lacks foreign key referential integrity enforcement, lacks crash recovery undo/redo logs, and only supports table-level locking. InnoDB supports full ACID transactions, row-level locking, foreign key constraints, and MVCC, making it the only suitable engine for our application."

---

### Trap 15: "Why do you use prepared statements instead of dynamic SQL strings?"
- **Trap:** Security and query compilation knowledge.
- **Accurate Viva Response:**
  > "Prepared statements eliminate SQL injection vulnerabilities (OWASP A03). The database engine compiles the SQL syntax structure before parameter values are bound. User inputs are treated strictly as literal data constants, making it impossible for input characters to alter the SQL syntax tree. Additionally, prepared statements allow query plan reuse."

---

### Trap 16: "What are the limitations of your SHA-256 password hashing implementation?"
- **Trap:** Seeing if you acknowledge known security limitations honestly.
- **Accurate Viva Response:**
  > "Our current implementation uses unsalted SHA-256 via Qt's `QCryptographicHash`. While significantly superior to plaintext, unsalted SHA-256 is vulnerable to precomputed rainbow table attacks and high-speed GPU brute force. For an enterprise production deployment, the hashing algorithm must be upgraded to an adaptive, salted algorithm such as Argon2id or bcrypt with an individual salt per user."

---

### Trap 17: "What happens if MySQL goes offline while SmartATC is running?"
- **Trap:** Testing application fault tolerance and error handling.
- **Accurate Viva Response:**
  > "In `DatabaseManager`, connection checks detect when the database handle becomes invalid. `ApplicationService` handles database offline states gracefully: it catches connection errors, sets informative error strings, falls back to in-memory simulation mode, and notifies the user without crashing the application."

---

### Trap 18: "What is the difference between a Process and a Thread in your project?"
- **Trap:** Fundamental OS conceptual knowledge.
- **Accurate Viva Response:**
  > "A Process is an independent execution unit with its own virtual memory address space. In our simulation, flights are modeled as processes. A Thread is a lightweight execution stream within a process that shares the process's heap and global memory. In SmartATC, we spawn C++ `std::thread` worker instances in `ConcurrentAllocationDemo` to simulate concurrent allocation requests within our application process."

---

### Trap 19: "Can two flights with the same priority enter the Ready Queue, and how does the scheduler decide between them?"
- **Trap:** Testing your tie-breaking comparator understanding.
- **Accurate Viva Response:**
  > "Yes. Multiple flights frequently share the same priority tier (e.g. domestic flights at Priority 3). Our `ReadyQueue` comparator breaks ties deterministically: first by earliest arrival timestamp (`arrivalTime ASC`, FCFS tie-breaker), and if arrival times are identical, by smaller unique flight ID (`flightId ASC`). The dispatch order is completely deterministic."

---

### Trap 20: "What are the actual process lifecycle states in your C++ code?"
- **Trap:** Checking whether you quote generic textbook states or know your actual C++ code.
- **Accurate Viva Response:**
  > "Our C++ `ProcessState` enum defines exactly five states: `New`, `Ready`, `Running`, `Blocked`, and `Completed`. In the MySQL `scheduling_queue` table, these map to the `process_state` ENUM (`Ready`, `Running`, `Completed`, `Blocked`), with `New` mapping to `Ready` upon queue entry. We do not use invented states like `Waiting` or `Terminated`."
