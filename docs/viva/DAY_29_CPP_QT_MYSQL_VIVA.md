# SmartATC — C++, Qt & MySQL Integration Viva Preparation Guide

## 1. C++17 Core Engineering & OOP Principles

### 1.1 Why C++17?
SmartATC is authored in modern **C++17** (ISO/IEC 14882:2017). C++17 provides:
- **Resource Acquisition Is Initialization (RAII):** Deterministic object lifetime management where resources (mutex locks, file handles, database handles) are acquired in constructors and freed in destructors.
- **Concurrency Support:** Native standard threading library (`std::thread`, `std::mutex`, `std::unique_lock`, `std::condition_variable`).
- **Structured Binding & Nested Namespaces:** Clean, expressive code organization (`SchedulerComponents`, `Models`, `Database`).
- **Smart Pointers:** Automatic memory management via `std::unique_ptr` and `std::shared_ptr`, preventing memory leaks and dangling pointers.

### 1.2 Object-Oriented Programming (OOP) in SmartATC
1. **Encapsulation:**
   - Classes strictly guard private data members behind public accessors and mutators.
   - Example: In `AgingEngine`, interval seconds (`m_intervalSeconds`) and timer state (`m_isRunning`) are private; modification requires `setIntervalSeconds(int seconds, QString *outError)`, which validates interval bounds ($1 \le t \le 86400$) before modifying state.
2. **Inheritance:**
   - GUI views (`DashboardView`, `FlightsView`, `RunwaysView`, `SchedulingView`) inherit from `QWidget`.
   - Modals (`FlightDialog`, `RunwayAllocationDialog`, `ConcurrentAllocationDemo`) inherit from `QDialog`.
   - The main shell (`MainWindow`) inherits from `QMainWindow`.
   - Engine components (`AgingEngine`) inherit from `QObject` to utilize the Qt meta-object system and signals/slots.
3. **Polymorphism:**
   - Virtual destructor hierarchies in all Qt-derived classes (`virtual ~AgingEngine() override`).
   - Virtual event handlers (`closeEvent(QCloseEvent *event)`) overridden in views to handle clean teardown and timer stops.

---

## 2. Qt 6 Framework & GUI Architecture

### 2.1 The Signals and Slots Mechanism
Qt's **Signals and Slots** mechanism is a type-safe implementation of the Observer pattern for event-driven programming:
- **Signal:** Emitted when a specific event occurs (e.g. `QPushButton::clicked()`, `AgingEngine::agingEvaluated()`).
- **Slot:** A C++ member function invoked in response to a signal.
- **Loose Coupling:** The sender of a signal does not need to know which class or slot receives it; multiple slots can connect to a single signal, and multiple signals can connect to a single slot.
- **Thread Safety:** When connecting across different threads (e.g., worker threads in `ConcurrentAllocationDemo` sending logs to GUI), Qt uses `Qt::QueuedConnection`, marshalling arguments safely across thread boundaries into the GUI event loop.

### 2.2 Core Qt Components in SmartATC
- **`QMainWindow` (`ui/MainWindow.cpp`):** Top-level application shell. Manages sidebar navigation, top status bar, active user profile banner, and dynamically switches views via `QStackedWidget`.
- **`QWidget`:** Base class for all 5 embedded views:
  - `LoginView` (Authentication portal)
  - `DashboardView` (KPI cards, telemetry gauges, audit stream)
  - `FlightsView` (Flight table, status badges, action buttons)
  - `AircraftView` (Fleet records, wake turbulence categories)
  - `RunwaysView` (Runway cards, status toggle, allocator)
  - `SchedulingView` (ReadyQueue tab & Starvation simulation tab)
- **`QDialog`:** Modal popups for user input:
  - `FlightDialog` (Add/edit flight form with validation)
  - `RunwayDialog` (Runway configuration modal)
  - `RunwayAllocationDialog` (Flight-to-runway assignment modal)
  - `ConcurrentAllocationDemo` (Interactive multi-threaded simulation console)
- **`QTableWidget`:** Used across all views to render relational tabular data, format status badges with custom styling, and handle user selection.
- **`QTimer`:** Real-time event timer used in `AgingEngine` to fire periodic aging evaluations and in `DashboardView` to trigger live metric refreshes.

---

## 3. Qt SQL & Database Integration (`DatabaseManager`)

### 3.1 Architecture of the Database Layer
Database interaction in SmartATC is encapsulated in `DatabaseManager` (`src/database/DatabaseManager.cpp`):
- **Singleton Pattern:** `DatabaseManager::instance()` provides a single managed connection pool across the application.
- **Driver Plugin:** Uses the `QMYSQL` driver interfacing with 64-bit `libmysql.dll`.
- **External Configuration:** Connection parameters are loaded from `db.conf` at runtime, ensuring database credentials are never hardcoded.

### 3.2 Transaction Boundary APIs
`DatabaseManager` wraps Qt's transaction management:
```cpp
bool DatabaseManager::beginTransaction() {
    return QSqlDatabase::database(CONNECTION_NAME).transaction();
}

bool DatabaseManager::commitTransaction() {
    return QSqlDatabase::database(CONNECTION_NAME).commit();
}

bool DatabaseManager::rollbackTransaction() {
    return QSqlDatabase::database(CONNECTION_NAME).rollback();
}
```

---

## 4. End-to-End Execution Trace of a GUI Action

To demonstrate system integration in a viva, trace what happens when an Air Traffic Controller clicks **"Allocate Runway"**:

```
[User Action in GUI]
  │
  ▼
1. Controller clicks "Allocate Runway" button on RunwayManagementView (`ui/RunwaysView.cpp`).
  │
  ▼
2. Qt Signal `QPushButton::clicked()` fires, invoking slot `RunwaysView::onAllocateClicked()`.
  │
  ▼
3. Modal Dialog `RunwayAllocationDialog` opens, prompting selection of Runway 09L and Flight AI-101.
  │
  ▼
4. Controller clicks "Confirm Allocation" -> Dialog emits signal with selected IDs.
  │
  ▼
[Service Layer & RBAC Verification]
  │
  ▼
5. Slot invokes `ApplicationService::instance().allocateRunway(runwayId=1, flightId=1, &errorMsg)`.
  │
  ▼
6. `ApplicationService` verifies authentication: `m_session.isAuthenticated() == true`.
  │
  ▼
7. `ApplicationService` checks RBAC permission: `m_session.hasPermission("action:RunwayAllocation") == true`.
  │
  ▼
[Operating System Concurrency Protection]
  │
  ▼
8. `ApplicationService` acquires `ScopedResourceLock` (`std::mutex`) in `ResourceManager` for Runway 09L,
   preventing concurrent thread access in memory.
  │
  ▼
[Database Transaction Execution in MySQL]
  │
  ▼
9. `ApplicationService` initiates MySQL transaction: `db.transaction()`.
  │
  ▼
10. Issues row-level lock: `SELECT status FROM runways WHERE runway_id = 1 FOR UPDATE;`.
  │
  ▼
11. Verifies status == 'Available'; checks flight operational eligibility (`Scheduled` / `Taxiing`).
  │
  ▼
12. Multi-Table Mutations:
    a. `INSERT INTO runway_assignments (flight_id, runway_id, assigned_by, status, ...) VALUES (1, 1, 2, 'Active', NOW());`
    b. `UPDATE runways SET status = 'Occupied' WHERE runway_id = 1;`
    c. `UPDATE flights SET operational_status = 'Cleared' WHERE flight_id = 1;`
    d. `INSERT INTO operation_logs (operation_type, transaction_status, message, ...) VALUES ('RUNWAY_ASSIGN', 'Committed', ...);`
  │
  ▼
13. Commits transaction: `db.commit()`. MySQL flushes changes to disk and releases row locks.
  │
  ▼
[In-Memory Synchronization & UI Refresh]
  │
  ▼
14. `ApplicationService` updates in-memory `ResourceManager` and `Process` state to Running.
  │
  ▼
15. Service returns `AllocationResult::Success` to `RunwaysView`.
  │
  ▼
16. `RunwaysView` updates table: Runway 09L row badge switches to "Occupied" (Red),
    and button toggles to "Release".
  │
  ▼
17. Emits system notification; `DashboardView` utilization gauge increments automatically.
```

---

## 5. Architectural Invariant: No Invented Classes
> [!IMPORTANT]
> **Viva Accuracy Notice:**  
> When answering examiner questions, speak only of classes that actually exist in the SmartATC codebase:
> - **Service Façade:** `ApplicationService` (There are NO invented "FlightController", "RunwayDao", or "UserRepository" classes).
> - **Database Singleton:** `DatabaseManager` (Executes prepared `QSqlQuery` instances directly).
> - **Scheduler Layer:** `Scheduler`, `ReadyQueue`, `AgingEngine`.
> - **Resource Layer:** `ResourceManager`, `ScopedResourceLock`.
> - **Models:** `Flight`, `Aircraft`, `Runway`, `Process`, `Session`, `User`.
