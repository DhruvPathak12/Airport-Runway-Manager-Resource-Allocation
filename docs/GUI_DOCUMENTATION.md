# Graphical User Interface (GUI) Documentation — SmartATC

## 1. Overview & UI Design Standards

SmartATC provides an intuitive desktop user interface developed using **Qt 6.11 Widgets**. 

### UI Principles:
- **Clean Industrial Aesthetics:** High-contrast color palette, modern typography, and structured layout margins.
- **Visual Status Coding:**
  - `Green (#28a745)`: Available, Cleared, Normal, Connected.
  - `Blue (#007bff)`: Active, Taxiing, Occupied.
  - `Amber (#ffc107)`: Scheduled, Offline, Warning.
  - `Red (#dc3545)`: Emergency, Maintenance, Cancelled, Rolled Back.
- **Dynamic Role-Based Filtering:** Navigation items and modification controls adapt automatically based on the user's role permissions.

---

## 2. Implemented Screen Catalog

### 2.1 Login View (`LoginView`)
- **Purpose:** Entry gate verifying credentials before admitting users into the operational shell.
- **Components:**
  - Username text entry.
  - Password entry with masked characters (`EchoMode::Password`).
  - "Log In" primary action button.
  - Contextual error label displaying authentication failures (e.g., `"Invalid credentials"` or `"Account is Inactive"`).
- **Security:** Passwords cannot be copy-pasted or inspected; fields are cleared upon logout.

---

### 2.2 Main Window & Sidebar Navigation (`MainWindow`)
- **Shell Structure:** Top status bar and a two-column body (left sidebar + stacked content area).
- **Top Bar Badges:**
  - Live digital clock (`yyyy-MM-dd hh:mm:ss`).
  - Database status badge (`● DB: Connected (smartatc@127.0.0.1:3306)` in green, or `○ DB: Disconnected` in amber).
  - Authenticated user pill (`👤 atc_controller (ATC)`).
  - Logout action button.
- **Sidebar Navigation:**
  - `Dashboard`: Accessible to all roles.
  - `Flights`: Accessible to all roles (edit actions restricted to ATC).
  - `Aircraft`: Accessible to all roles (modification restricted to Admin/Operator).
  - `Runways`: Accessible to all roles (maintenance restricted to Operator/Admin).
  - `Scheduling`: Accessible to `Admin` and `ATC` (hidden from Airport Operator).

---

### 2.3 Dashboard View (`DashboardView`)
- **Purpose:** Real-time data-driven operational monitoring center.
- **Components:**
  1. **Live Header Bar:** Digital clock, live connection badge, user badge, and manual **"Refresh Dashboard"** button.
  2. **12 KPI Cards:** Total Flights, Scheduled Flights, Active Flights, Emergency Flights, Total Runways, Available Runways, Occupied Runways, Maintenance Runways, Runway Utilization %, Ready Queue Depth, Running Count, and Completed Count.
  3. **Active Emergency Panel:** Surfaces active distress flights (airline, priority, origin, destination, assigned runway) with instant **"Open Flights →"** navigation links. Shows a green reassuring normal operations banner when 0 emergencies exist.
  4. **Runway Capacity Bar & Table:** Responsive progress bar color-coded by capacity threshold (Green <60%, Amber 60–85%, Red >85%) and runway asset overview table.
  5. **OS Telemetry Panel:** Highlights scheduling policy, aging interval, live average waiting time ($\bar{W}$), average turnaround time ($\overline{\text{TAT}}$), and next queue candidate card.
  6. **Recent Flights & Audit History:** 8 recent flights and the 10 most recent persistent database operation logs with user attribution.

---

### 2.4 Flight Management View (`FlightsView` & `FlightDialog`)
- **Purpose:** Manage commercial flight schedules, flight types, and emergency states.
- **Components:**
  - Filter bar: Real-time search box, flight type dropdown (`All`, `Arrival`, `Departure`), and status dropdown.
  - Flights Table: Columns for Flight Number, Airline, Aircraft, Route, Type, Scheduled Time, Priority, Emergency, and Status.
  - Action Toolbar:
    - **"Add Flight"**: Opens `FlightDialog` (ATC only).
    - **"Edit Flight"**: Updates route, scheduled time, or priority.
    - **"Declare / Clear Emergency"**: ATC action immediately toggling `is_emergency`.
    - **"Cancel Flight"**: Marks flight cancelled and removes any ready-queue process.

---

### 2.5 Aircraft Fleet Management View (`AircraftView` & `AircraftDialog`)
- **Purpose:** Manage registered airframes, maintenance status, and capacity.
- **Components:**
  - Fleet Table: Columns for Tail Registration, Model, Capacity, Status (`Active`, `Maintenance`, `Retired`), and Registration Date.
  - Action Buttons:
    - **"Register Aircraft"**: Admin-only dialog adding new airframes.
    - **"Toggle Maintenance"**: Operator/Admin action updating airframe readiness.
    - **"Delete Aircraft"**: Admin action protected by referential integrity check.

---

### 2.6 Runway Management View (`RunwaysView` & `RunwayDialog`)
- **Purpose:** Track runway availability, configuration, and multi-threaded contention.
- **Components:**
  - Runways Table: Runway Designation, Type (`Landing`, `Takeoff`, `Both`), Status (`Available`, `Occupied`, `Maintenance`), Length, Heading, and Surface.
  - **"Toggle Maintenance"**: Operator/Admin button closing/opening runway.
  - **"Concurrent Contention Demo" Panel:** Control panel allowing the operator to select a runway, configure competing worker threads (default 3), and execute `ConcurrentAllocationDemo`. Displays real-time logs verifying mutual exclusion.

---

### 2.7 Scheduling & Starvation View (`SchedulingView`)
- **Purpose:** Manage the ready queue, priority aging engine, and starvation verification.
- **Tab 1: Live Ready Queue & Dispatch:**
  - **Ready Queue Table:** Displays enqueued processes sorted by effective priority, showing Flight ID, Arrival Time, Base Priority, Effective Priority, Burst Time, Waiting Time, and Aging Levels.
  - **Aging Engine Controls:** Spinbox for configuring aging interval (1–86400s, default 30s) and Start/Stop toggle button.
  - **"Schedule & Allocate Next"**: Executes unified Day 23 scheduling-to-runway allocation.
  - **Performance Metrics Banner:** Shows live Average Waiting Time and Average Turnaround Time.
- **Tab 2: Starvation Demonstration:**
  - Dual-track simulation module executing controlled workloads.
  - Compares priority scheduling with aging versus without aging over configurable horizons (e.g., 180 seconds).

---

## 3. How to Launch and Capture Screenshots

To inspect the live running application and capture screenshots on Windows:

1. **Deploy Runtime Binaries:**
   ```powershell
   powershell -ExecutionPolicy Bypass -File "d:\Projects\Airport Runway Manager &Resource Allocation\SmartATC\scripts\deploy_runtime.ps1"
   ```
2. **Launch the Executable:**
   ```powershell
   & "d:\Projects\Airport Runway Manager &Resource Allocation\SmartATC\build\SmartATC.exe"
   ```
3. **Log in with Role Credentials:**
   - **ATC Controller:** Username: `atc_controller` / Password: as configured in seed data.
   - **System Admin:** Username: `admin` / Password: as configured in seed data.
   - **Airport Operator:** Username: `ops_user` / Password: as configured in seed data.
4. **Capture Window Screenshots:**
   - Use Windows shortcut `Alt + PrtScn` or `Win + Shift + S` to capture individual views.
   - Save screenshots into `docs/screenshots/` for presentation slide decks.
