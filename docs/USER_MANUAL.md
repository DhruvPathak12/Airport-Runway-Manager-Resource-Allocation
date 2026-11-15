# SmartATC User Manual & Operator Guide

The **Smart Air Traffic Control Management System (SmartATC)** is an educational desktop simulation designed to demonstrate Operating System scheduling, mutual exclusion, multithreading, and DBMS ACID transactions through an airport operations control center.

This user manual guides operators through system navigation, role capabilities, operational workflows, and simulation modules.

---

## 1. Getting Started and Authentication

### 1.1 Launching the Application
Launch the graphical executable from your build directory:
```powershell
.\build\bin\smartatc_gui.exe
```
Upon launch, the application displays the **Login View**.

### 1.2 User Roles & Operational Permissions
Sign in using configured user credentials matching your desired access level:

| Role | Default Username | Permitted Actions |
|:---|:---|:---|
| **Administrator** | `admin` | Full system access: User management, scheduling management, fleet administration, transaction monitoring. |
| **Air Traffic Controller (ATC)** | `atc_operator` | Operational control: Flight management, emergency declarations, process dispatching, runway allocation. |
| **Airport Operator** | `runway_ops` | Infrastructure maintenance: Aircraft fleet registry, runway status toggle (`Available` / `Maintenance`), contention demonstration. |

---

## 2. Navigation Overview

Once logged in, the **Main Window** opens. Navigation between operational modules is handled via the navigation sidebar on the left:

- 📊 **Dashboard:** Real-time operational overview, queue sizes, runway utilization, and recent audit logs.
- ✈️ **Flights:** Live flight table, arrival/departure records, flight creation, and emergency status toggles.
- 🛩️ **Aircraft:** Fleet registry, aircraft type specifications, passenger capacity, and maintenance status.
- 🛫 **Runways:** Infrastructure runway status, operational type (`Landing`, `Takeoff`, `Both`), maintenance toggle, and concurrency demo.
- ⏱️ **Scheduling & OS Simulation:** Process Ready Queue, priority scheduling dispatch, Priority Aging engine, and Starvation demonstration.

---

## 3. Operational Modules & Workflows

### 3.1 System Dashboard
The Dashboard provides an aggregated, real-time snapshot of the airport and simulation environment:
- **Flight Statistics:** Total registered flights, scheduled counts, active flights, and active emergency alert cards.
- **Runway Status Bar:** Visual indicators of Available (Green), Occupied (Blue), and Maintenance (Red) runways with a dynamic utilization progress bar.
- **OS Scheduling Metrics:** Current Ready Queue depth, active running process, average Turnaround Time ($\overline{TAT}$), and average Waiting Time ($\overline{WT}$).
- **Audit History:** Real-time log table showing the 10 most recent persistent database transactions with user attribution and status.

---

### 3.2 Flight Management
The Flights tab allows operators to track and schedule aircraft movements:
1. **Filter Flights:** Use the dropdowns to filter by `All Flights`, `Arrivals Only`, `Departures Only`, or operational status (`Scheduled`, `Taxiing`, `Cleared`, `Landed`, `Departed`, `Cancelled`).
2. **Add a Flight (ATC Only):**
   - Click **"Add Flight"** to open `FlightDialog`.
   - Input Flight Number (e.g., `AI-302`), Select Aircraft Registration, Origin/Destination, Scheduled Time, and Base Priority (1 = Highest to 5 = Lowest).
   - Click **"Submit"** to persist to MySQL.
3. **Declare In-Flight Emergency (ATC Only):**
   - Select a flight from the table.
   - Click **"Declare Emergency"**.
   - The flight's effective priority immediately escalates to **Priority 1 (Emergency Highest Priority)**, moving it to the head of the Ready Queue for the next runway allocation.

---

### 3.3 Runway Infrastructure & Contention Control
The Runways tab manages runway strip availability and demonstrates hardware mutual exclusion:
1. **Inspect Runways:** View Runway Number (e.g., `09L`, `27R`), Runway Type (`Landing`, `Takeoff`, `Both`), and current status (`Available`, `Occupied`, `Maintenance`).
2. **Toggle Maintenance (Airport Operator Only):**
   - Select a runway.
   - Click **"Toggle Maintenance"** to transition the runway into `Maintenance` state. The scheduler and allocation engine will reject any flight assignments to this runway until restored to `Available`.
3. **Run Multi-threaded Contention Demonstration:**
   - Locate the **Concurrent Contention Demo** panel.
   - Select a target runway and configure competing worker threads (default 3).
   - Click **"Run Thread Contention Demo"**.
   - Spawns concurrent C++ threads representing separate aircraft attempting to acquire the same runway simultaneously.
   - The log console proves that mutual exclusion (`std::mutex`) allows exactly **one** aircraft to acquire the runway at any instant while other threads wait safely.

---

### 3.4 Process Scheduling & Priority Aging Simulation
The Scheduling tab demonstrates Operating System process scheduling concepts mapped to airport operations.

#### A. Viewing the Ready Queue
- The **Ready Queue Table** shows all simulated processes waiting for runway dispatch.
- Columns display Flight ID (PID), Arrival Time, Base Priority, Effective Priority (1=Highest...5=Lowest), Service Time, Waiting Time, and Aging Level.

#### B. Stepping Process Dispatch
- Click **"Dispatch Next Process"** (or **"Schedule & Allocate Next"**).
- The non-preemptive scheduler evaluates the Ready Queue and dispatches the highest-priority process (lowest numerical effective priority, broken by earlier arrival time).
- State transitions from `Ready` $\to$ `Running` $\to$ `Completed`.

#### C. Configuring Priority Aging
Prevent starvation of low-priority commercial flights during heavy traffic:
1. Locate the **Priority Aging Configuration** panel.
2. Set the **Aging Interval** spinbox (e.g., `30 seconds`).
3. Click **"Start Aging"**.
4. The background `AgingEngine` continuously monitors waiting processes. For every completed interval in `Ready` state, effective priority improves by decrementing its numerical value:
   $$\text{effectivePriority} = \max(1, \text{basePriority} - \text{agingLevel})$$
5. The queue dynamically re-sorts to reflect improved priorities.

#### D. Starvation Demonstration (Day 18 Tab)
1. Switch to the **"Starvation Demonstration"** tab.
2. Click **"Run Without Aging"**:
   - Simulates a continuous stream of high-priority flights (Priority 2) bypassing an initial low-priority flight (Priority 4).
   - The low-priority flight's waiting time grows indefinitely, visually illustrating process starvation.
3. Click **"Run With Aging"**:
   - The same arrival pattern executes, but the aging engine gradually improves the low-priority flight's priority to 3, 2, and 1.
   - The low-priority flight is successfully dispatched, bounding waiting time and proving starvation elimination.
4. The comparison table displays side-by-side turnaround and waiting time metrics.

---

### 3.5 Database Transactions & ACID Verification
SmartATC guarantees complete atomicity during runway assignments:
1. When a runway is assigned via `allocateRunway`:
   - A MySQL transaction begins (`db.transaction()`).
   - Pessimistic row locks (`SELECT ... FOR UPDATE`) secure the runway, flight, and active assignment records.
   - Updates insert `runway_assignments`, set runway status to `Occupied`, advance flight status to `Cleared`, and insert a committed audit record into `operation_logs`.
   - `db.commit()` commits all mutations atomically.
2. If any step fails (e.g., conflict or simulated failure), `db.rollback()` reverts all database changes, and `ResourceManager` restores in-memory state.

---

## 4. Logging Out
To switch roles or close session:
1. Click the **"Logout"** button in the top-right header bar.
2. The active session is terminated, and the application returns to the **Login View**.
