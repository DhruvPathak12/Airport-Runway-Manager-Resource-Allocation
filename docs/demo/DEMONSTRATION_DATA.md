# SmartATC — Final Demonstration Data & Environment Specification

## 1. Airport Simulation Context
- **ICAO / IATA Identifier:** VABB / BOM
- **Airport Name:** Chhatrapati Shivaji Maharaj International Airport, Mumbai, India
- **Operating Authority:** Mumbai International Airport Limited (MIAL) / Directorate General of Civil Aviation (DGCA)
- **Simulation Scope:** Commercial runway management, arrival/departure sequencing, dynamic emergency handling, and priority aging.

---

## 2. Demonstration Accounts & Role Verification

SmartATC implements Role-Based Access Control (RBAC) across three application roles: `Admin`, `ATC` (Air Traffic Controller), and `Airport Operator`.

The status of standard demonstration accounts in the live MySQL database and `seed.sql` has been verified:

| Username | Application Role | Live DB Status | Permissions Scope | Notes |
|---|---|---|---|---|
| **`admin_demo`** | `Admin` | **VERIFIED** | Full administrative rights, user oversight, scheduling, transactions | Present in live database and `seed.sql` |
| **`atc_demo`** | `ATC` | **VERIFIED** | Flight management, emergency declaration, runway allocation, scheduling | Present in live database and `seed.sql` |
| **`operator_demo`** | `Airport Operator` | **VERIFIED** | Runway maintenance toggling, aircraft registry view | Present in live database and `seed.sql` |
| **`admin`** | `Admin` | **NOT VERIFIED** | N/A | Not present in default `seed.sql` or database |
| **`atc_operator`** | `ATC` | **NOT VERIFIED** | N/A | Not present in default `seed.sql` or database |
| **`runway_ops`** | `Airport Operator` | **NOT VERIFIED** | N/A | Not present in default `seed.sql` or database |

> [!NOTE]
> **Account Provisioning Guide (For Non-Verified Accounts):**
> If an evaluation panel requires accounts named `admin`, `atc_operator`, or `runway_ops`, provision them safely in MySQL without modifying source code:
> ```sql
> INSERT INTO users (username, password_hash, role_id, full_name, status)
> VALUES 
>   ('admin', SHA2('Admin@1234', 256), (SELECT role_id FROM roles WHERE role_name = 'Admin'), 'System Administrator', 'Active'),
>   ('atc_operator', SHA2('ATC@1234', 256), (SELECT role_id FROM roles WHERE role_name = 'ATC'), 'Air Traffic Controller', 'Active'),
>   ('runway_ops', SHA2('Operator@1234', 256), (SELECT role_id FROM roles WHERE role_name = 'Airport Operator'), 'Runway Operations', 'Active')
> ON DUPLICATE KEY UPDATE status = 'Active';
> ```

---

## 3. Runway Infrastructure Specification

The demonstration models the primary runway infrastructure at Mumbai (BOM):

| Runway ID | Runway Number | Physical Length | Operational Category | Demonstration Status | Assigned Flights |
|---|---|---|---|---|---|
| **1** | `09L` | 3,660 m | `Both` (Landing & Takeoff) | **Available** | Competing `AI-101` & `6E-202` |
| **2** | `09R` | 2,990 m | `Takeoff` (Takeoff only) | **Available** | Reserved for `SG-404` |
| **3** | `27`  | 3,445 m | `Landing` (Landing only) | **Maintenance** | Simulates unavailable resource |

---

## 4. Aircraft Fleet Inventory

Four representative commercial aircraft models registered under the Indian Directorate General of Civil Aviation (`VT-*` series):

| Aircraft ID | Tail Registration | Model | Wake Turbulence Category | Capacity | Status |
|---|---|---|---|---|---|
| **1** | `VT-AT1` | Airbus A320neo | Medium | 180 | Active |
| **2** | `VT-AT2` | Airbus A321neo | Medium | 222 | Active |
| **3** | `VT-AT3` | Boeing 777-300ER | Heavy | 396 | Active |
| **4** | `VT-AT4` | Boeing 737-800 | Medium | 189 | Active |

---

## 5. Demonstration Flights Catalog

Five diverse commercial flights scheduled at Mumbai (BOM) representing distinct scheduling tiers, priorities, and airlines:

| Flight ID | Flight Number | Airline | Aircraft | Route | Flight Type | Base Priority | Initial Operational Status | Initial Process State |
|---|---|---|---|---|---|---|---|---|
| **1** | `AI-101` | Air India | VT-AT1 (A320neo) | DEL $\to$ BOM | Arrival | **2** | Scheduled | `Ready` |
| **2** | `6E-202` | IndiGo | VT-AT2 (A321neo) | BLR $\to$ BOM | Arrival | **3** | Scheduled | `Ready` |
| **3** | `AI-303` | Air India | VT-AT3 (B777-300ER)| LHR $\to$ BOM | Arrival | **3** | Scheduled | `Ready` (Declared Emergency) |
| **4** | `SG-404` | SpiceJet | VT-AT4 (B737-800) | BOM $\to$ DXB | Departure | **4** | Scheduled | `Ready` |
| **5** | `UK-505` | Vistara | VT-AT1 (A320neo) | GOI $\to$ BOM | Arrival | **5** | Scheduled | `Ready` (Starvation Candidate) |

---

## 6. Baseline Ready Queue State (PCB Scheduling)

Prior to running scheduler steps, the in-memory `ReadyQueue` is initialized as follows:

| Queue Position | Flight Number | Flight ID | Arrival Time | Base Priority | Effective Priority | State | Dispatch Precedence Reason |
|---|---|---|---|---|---|---|---|
| **#0** | `AI-101` | 1 | 10:00:00 | 2 | 2 | `Ready` | Highest priority in initial queue ($2 < 3, 4, 5$) |
| **#1** | `6E-202` | 2 | 10:05:00 | 3 | 3 | `Ready` | Ties with AI-303; earlier arrival time (FCFS) |
| **#2** | `AI-303` | 3 | 10:08:00 | 3 | 3 | `Ready` | Ties with 6E-202; later arrival time |
| **#3** | `SG-404` | 4 | 10:02:00 | 4 | 4 | `Ready` | Lower priority ($4 > 3$) |
| **#4** | `UK-505` | 5 | 10:01:00 | 5 | 5 | `Ready` | Lowest priority ($5 > 4$) — Starvation candidate |

---

## 7. Operational Invariants & Data Safety
1. **Pessimistic Concurrency:** Runway allocation requires atomic locking via `SELECT ... FOR UPDATE` on `runways`, `runway_assignments`, and `flights`.
2. **Mutual Exclusion:** In-memory allocation is guarded by `ResourceManager` mutexes (`std::mutex`) preventing race conditions.
3. **Non-Preemptive Execution:** Emergency priority overrides effective priority to 1, but does not interrupt an active running operation.
4. **Data Preservation:** Zero `DROP` or `TRUNCATE` operations are permitted during demonstration or reset runs.
