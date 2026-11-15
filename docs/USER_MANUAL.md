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

### 1.2 User Roles & Credentials
Sign in using pre-configured user credentials matching your desired access level:

| Role | Username | Password | Permitted Actions |
|:---|:---|:---|:---|
| **Administrator** | `admin` | `admin123` | Full access: User management, flight scheduling, runway maintenance, starvation simulation, database transactions. |
| **Air Traffic Controller (ATC)** | `atc_operator` | `atc123` | Operational access: Flight management, emergency declarations, process dispatch, scheduling queue control. |
| **Airport Operator** | `runway_ops` | `ops123` | Infrastructure access: Aircraft fleet registry, runway status toggle (Free/Maintenance), resource monitoring. |

---

## 2. Navigation Overview

Once logged in, the **Main Window** opens. Navigation between operational modules is handled via the navigation bar on the left:

- 📊 **Dashboard:** High-level operational metrics, active queue sizes, runway utilization, and transaction counters.
- ✈️ **Flights:** Live flight table, arrival/departure records, flight creation, and emergency status toggles.
- 🛩️ **Aircraft:** Fleet registry, aircraft type specifications, passenger capacity, and wake turbulence classifications.
- 🛫 **Runways:** Infrastructure runway status, length/surface specifications, maintenance toggle, and concurrency demo.
- ⏱️ **Scheduling & OS Simulation:** Process Ready Queue, priority scheduling dispatch, Priority Aging engine, and Starvation demonstration.

---

## 3. Operational Modules & Workflows

### 3.1 System Dashboard
The Dashboard provides an aggregated, real-time snapshot of the airport and simulation environment:
- **Flight Statistics:** Total registered flights, scheduled vs. landed/departed counts, active emergency count.
- **Runway Status Bar:** Visual indicators of Free (Green), Occupied (Red), and Maintenance (Yellow) runways.
- **OS Scheduling Metrics:** Current Ready Queue depth, active running process, average Turnaround Time (TAT), and average Waiting Time (WT).
- **DBMS Transaction Counter:** Successful commits, aborts/rollbacks, and transaction throughput.

---

### 3.2 Flight Management
The Flights tab allows operators to track and schedule aircraft movements.
1. **Filter Flights:** Use the dropdown to filter by `All Flights`, `Arrivals Only`, or `Departures Only`.
2. **Add a Flight:**
   - Click **"Register Flight"**.
   - Input Flight Number (e.g., `AI-302`), Select Aircraft Registration, Origin/Destination, Scheduled Time, and Base Priority (1 = Low to 5 = Critical).
   - Click **"Submit"** to persist to MySQL.
3. **Declare In-Flight Emergency:**
   - Select an in-flight arrival from the table.
   - Click **"Declare Emergency"**.
   - The flight's priority immediately escalates to **Level 5 (Emergency)**, moving it to the head of the CPU scheduling queue.

---

### 3.3 Runway Infrastructure & Contention Control
The Runways tab manages airport strip availability and demonstrates hardware mutual exclusion.
1. **Inspect Runways:** View Runway IDs (e.g., `RWY-09L`, `RWY-27R`), length in meters, surface type, and current status.
2. **Toggle Maintenance:**
   - Select a runway.
   - Click **"Set Maintenance"** to transition the runway into `MAINTENANCE` state. The scheduler will reject any flight assignments to this runway until restored to `FREE`.
3. **Run Multi-threaded Contention Demonstration:**
   - Click **"Run Thread Contention Demo"**.
   - Spawns 4 concurrent C++ threads representing separate aircraft attempting to acquire the same runway simultaneously.
   - The log console proves that mutual exclusion (`std::mutex`) allows exactly **one** aircraft to acquire the runway at any instant while other threads wait safely.

---

### 3.4 Process Scheduling & Priority Aging Simulation
The Scheduling tab demonstrates CPU scheduling concepts mapped to airport operations.

#### A. Viewing the Ready Queue
- The **Ready Queue Table** shows all simulated processes waiting for runway dispatch.
- Columns display Process ID, Flight ID, Effective Priority, Burst Time, Waiting Time, and Aging Level.

#### B. Stepping Process Dispatch
- Click **"Dispatch Next Process"**.
- The non-preemptive scheduler dispatches the highest-priority process from the Ready Queue to the runway.
- State transitions from `READY` -> `RUNNING` -> `COMPLETED`.

#### C. Configuring Priority Aging
Prevent starvation of low-priority commercial flights during heavy traffic:
1. Locate the **Priority Aging Configuration** panel.
2. Set **Wait Time Threshold** (e.g., `30 seconds` or `3 dispatch ticks`).
3. Set **Aging Increment** (`+1 Priority level`).
4. Set **Maximum Boost Cap** (e.g., `Priority 4`).
5. Click **"Apply Aging Settings"**.
6. When aging is active, processes that wait longer than the threshold automatically receive priority upgrades until dispatched.

#### D. Starvation Demonstration
1. Click **"Run Starvation Simulation"**.
2. The simulation creates a low-priority cargo flight (Priority 1) followed by a continuous stream of incoming high-priority flights (Priority 4).
3. **Phase 1 (Without Aging):** The low-priority flight is continuously bypassed, illustrating CPU starvation (waiting time indefinitely increases).
4. **Phase 2 (With Aging):** With aging enabled, the low-priority flight's priority steadily rises to Priority 2, 3, and 4, ensuring bounded waiting time and successful execution.
5. The window displays a comparison chart and summary report.

---

### 3.5 Database Transactions & ACID Verification
SmartATC guarantees complete atomicity during runway assignments:
1. When a runway is assigned, the system opens a MySQL transaction:
   - Sets flight state to `LANDED` or `DEPARTED`.
   - Inserts record into `runway_assignments`.
   - Updates runway status to `OCCUPIED`.
   - Updates process queue state to `COMPLETED`.
2. If any step fails (e.g., simulated network fault or runway already reserved), the entire transaction is rolled back, preventing orphaned runway assignments.

---

## 4. Logging Out
To switch roles or close session:
1. Click the **"Log Out"** button in the lower-left corner of the window.
2. The active session is terminated, and the application returns to the **Login View**.
