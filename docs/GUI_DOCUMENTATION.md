# Graphical User Interface (GUI) Documentation — SmartATC

## 1. Overview & UI Architecture

SmartATC provides an intuitive desktop interface built on **Qt 6.11 Widgets**. 

The UI follows a centralized model-view architecture where views interact with `ApplicationService` for business operations, RBAC permissions, and database updates.

### 1.1 UI Design Standards
- **Palette and Contrast:** Professional dark sidebar (`#1a1a2e`) combined with high-contrast light content canvases (`#f5f7fa`).
- **Standardized Status Indicators:**
  - `Green (#28a745)`: Available, Cleared, Normal, Connected.
  - `Blue (#007bff)`: Active, Taxiing, Occupied.
  - `Amber (#ffc107)`: Scheduled, Disconnected, Warning.
  - `Red (#dc3545)`: Emergency, Maintenance, Cancelled, Rolled Back.
- **Role-Based Dynamic Adaptability:** Sidebar navigation items and operational action buttons dynamically enable, disable, or hide depending on the authenticated session's role (`Admin`, `ATC`, or `Airport Operator`).

---

## 2. Comprehensive Screen Inventory

The SmartATC interface is organized into:
1. Root Authentication Window (`LoginView`)
2. Main Navigation Shell (`MainWindow`) with 5 Implemented Pages and 3 Placeholder Pages
3. Multi-Tab Subsystems (`SchedulingView`)
4. Modal Dialogs (4 Dialogs)
5. Embedded Operational Monitoring Panels

---

### 2.1 Root Authentication Window

#### `LoginView` (Entry Screen)
- **Container:** Page index 0 in `MainWindow::m_mainStack`.
- **Purpose:** Secure entry gate verifying credentials against `users` table via `ApplicationService::login()`.
- **Components:**
  - Username field (`QLineEdit`).
  - Password field (`QLineEdit` with `EchoMode::Password`).
  - "Log In" primary push button.
  - Status/Error banner displaying contextual authentication feedback (e.g., `"Invalid credentials"`, `"Account is Inactive"`).
- **Behavior:** On successful authentication, signals `loginSuccessful` to transition the main stack to the operational shell (`m_appShell`).

---

### 2.2 Main Navigation Shell (`MainWindow`)

The application shell consists of a top status title bar, a fixed-width left navigation sidebar (`QListWidget`), and a central stacked content area (`QStackedWidget`).

#### Top Status Bar Badges:
- Application Title: `"✈ Smart Air Traffic Control Management System"`
- Live Database Connection Badge (`● DB: Connected (smartatc@127.0.0.1:3306)` in green, or `○ DB: Disconnected` in amber).
- Authenticated User Pill (`👤 atc_operator (ATC)`).
- "Logout" action button (clears session and returns to `LoginView`).

#### Left Navigation Sidebar Pages:

| Nav Index | Navigation Label | Class / Implementation | Status | Permitted Roles |
|:---:|:---|:---|:---:|:---|
| **0** | **Dashboard** | `DashboardView` | **Implemented** | All Roles (Admin, ATC, Operator) |
| **1** | **Flights** | `FlightsView` | **Implemented** | All Roles (ATC has write actions) |
| **2** | **Aircraft** | `AircraftView` | **Implemented** | All Roles (Admin/Operator write) |
| **3** | **Runways** | `RunwaysView` | **Implemented** | All Roles (Operator/Admin write) |
| **4** | **Scheduling** | `SchedulingView` | **Implemented** | Admin, ATC |
| **5** | **Transactions** | `makePlaceholderPage()` | *Placeholder* | Admin |
| **6** | **Logs** | `makePlaceholderPage()` | *Placeholder* | All Roles |
| **7** | **Users** | `makePlaceholderPage()` | *Placeholder* | Admin |

---

### 2.3 Implemented Main View Pages

#### 1. Dashboard View (`DashboardView`)
- **Purpose:** Real-time data-driven operational monitoring center.
- **Embedded Panels:**
  - **12 Live KPI Cards:** Total Flights, Scheduled Flights, Active Flights, Emergency Flights, Total Runways, Available Runways, Occupied Runways, Maintenance Runways, Runway Utilization %, Ready Queue Depth, Running Count, and Completed Count.
  - **Active Emergency Alert Banner:** Prominently highlights active distress flights with quick-navigation links. Displays a normal status banner when 0 emergencies exist.
  - **Runway Capacity Gauge:** Responsive utilization bar color-coded by occupancy threshold (Green <60%, Amber 60–85%, Red >85%).
  - **OS Telemetry & Scheduling Statistics:** Displays current scheduling policy, aging interval, average waiting time ($\overline{WT}$), average turnaround time ($\overline{TAT}$), and next candidate in the ready queue.
  - **Recent Activity Tables:** 8 recent flights and 10 most recent persistent database audit logs.

#### 2. Flight Management View (`FlightsView`)
- **Purpose:** Manage commercial flight operations, schedule new flights, and declare emergencies.
- **Components:**
  - Filter Bar: Live search box, flight type filter (`All`, `Arrival`, `Departure`), and status filter.
  - Flights Table: Columns for Flight Number, Airline, Aircraft, Route (Source $\to$ Destination), Type, Scheduled Time, Priority, Emergency flag, and Operational Status.
  - Action Buttons:
    - **"Add Flight"**: Opens `FlightDialog` (ATC only).
    - **"Declare / Clear Emergency"**: Immediately toggles `is_emergency` flag and updates priority (ATC only).
    - **"Refresh"**: Re-queries flights from database.

#### 3. Aircraft Fleet Management View (`AircraftView`)
- **Purpose:** Manage fleet inventory and airframe operational readiness.
- **Components:**
  - Fleet Table: Columns for Tail Registration (`registration_number`), Aircraft Type (`aircraft_type`), Passenger Capacity, Operational Status (`Active`, `Maintenance`, `Retired`).
  - Action Buttons:
    - **"Register Aircraft"**: Opens `AircraftDialog` (Admin only).
    - **"Toggle Maintenance"**: Toggles airframe between `Active` and `Maintenance` (Operator/Admin).
    - **"Refresh"**: Re-queries fleet records.

#### 4. Runway Management View (`RunwaysView`)
- **Purpose:** Track runway infrastructure availability and demonstrate concurrency control.
- **Components:**
  - Runways Table: Runway Number (e.g., `09L`, `27R`), Runway Type (`Landing`, `Takeoff`, `Both`), and Operational Status (`Available`, `Occupied`, `Maintenance`).
  - Action Buttons:
    - **"Toggle Maintenance"**: Toggles runway between `Available` and `Maintenance` (Airport Operator only).
    - **"Allocate Runway"**: Opens `RunwayAllocationDialog` to manually assign an available runway to an eligible flight (ATC only).
    - **"Release Runway"**: Frees an occupied runway, committing a release transaction.
  - **Multi-threaded Contention Demo Panel:** Control panel allowing operators to select a target runway and launch concurrent worker threads (`ConcurrentAllocationDemo`) to verify race-free mutual exclusion.

#### 5. Scheduling & OS Simulation View (`SchedulingView`)
- **Purpose:** Manage the ready queue, priority aging engine, and starvation verification.
- **Tabs within `SchedulingView`:**
  - **Tab 1: "Live Scheduler" (`createLiveTab`):**
    - Ready Queue Table: Lists processes waiting for dispatch with columns for Flight ID, Arrival Time, Base Priority, Effective Priority, Service Time, Waiting Time, and Aging Levels.
    - Aging Engine Controls: Spinbox for configuring aging interval (seconds) and Start/Stop toggle button.
    - Dispatch Controls: **"Dispatch Next"** and **"Schedule & Allocate Next"** buttons.
    - Status Banner: Displays active running process, current aging engine status, and live average waiting time metrics.
  - **Tab 2: "Starvation Demonstration" (`createStarvationTab`, Day 18):**
    - Dual-workload execution panel comparing scheduling **Without Aging** vs. **With Aging**.
    - Comparison Table: Shows the side-by-side progression of waiting times and turnaround times.
    - Explanation Box: Summarizes how priority aging bounds waiting times and rescues low-priority processes.

---

### 2.4 Modal Dialogs

1. **`FlightDialog`:**
   - Input dialog for creating or editing flights.
   - Fields: Flight Number, Airline, Source, Destination, Flight Type (Arrival/Departure), Aircraft selection dropdown, Scheduled Time picker, and Base Priority spinbox (1 to 5).
2. **`AircraftDialog`:**
   - Input dialog for registering aircraft.
   - Fields: Registration Number (tail code), Aircraft Type, Capacity, and Initial Status.
3. **`RunwayDialog`:**
   - Input dialog for creating or configuring runway infrastructure.
   - Fields: Runway Number (e.g., `09L`), Runway Type (`Landing`, `Takeoff`, `Both`), and Status.
4. **`RunwayAllocationDialog`:**
   - Modal dialog for manually pairing an available runway with an eligible scheduled/taxiing flight.
   - Triggers the atomic `allocateRunway` transaction sequence upon submission.

---

### 2.5 Verification and Screen Capture Instructions

To launch the graphical interface and verify screen inventory:
```powershell
cd "d:\Projects\Airport Runway Manager &Resource Allocation\SmartATC\build\bin"
.\smartatc_gui.exe
```

1. **Login:** Enter username `admin` and password.
2. **Dashboard:** Verify all 12 KPI cards, runway progress bar, and recent audit logs render.
3. **Navigation:** Click each item in the left sidebar to verify smooth page switching across all 5 implemented pages and 3 placeholder pages.
4. **Scheduling Tabs:** Click "Scheduling" in the sidebar and verify both "Live Scheduler" and "Starvation Demonstration" tabs operate.
