# Installation and Setup Guide

This guide details the complete process for setting up the development environment, configuring MySQL 8.0, compiling the C++17 / Qt 6 codebase, executing automated tests, deploying runtime libraries, and launching the **Smart Air Traffic Control Management System (SmartATC)** on Windows.

---

## 1. Prerequisites and System Requirements

### 1.1 Hardware Specifications
- **Processor:** x86_64 Dual-Core CPU (Quad-core 2.5 GHz+ recommended)
- **RAM:** Minimum 4 GB (8 GB recommended for compilation)
- **Disk Space:** 5 GB free disk space (accounting for Qt toolchain and CMake build artifacts)

### 1.2 Required Software and Toolchains
Ensure the following tools are installed and accessible via system `PATH`:

1. **Operating System:** Windows 10 or 11 (64-bit).
2. **Compiler:** MinGW-w64 GCC/G++ 13.1.0 (64-bit) (installed with Qt or standalone).
3. **Build Tools:**
   - **CMake:** Version 3.19 or higher (`cmake --version`).
   - **Ninja:** High-performance build generator (`ninja --version`).
4. **Qt Framework:**
   - **Qt 6.11.2** (or compatible Qt 6.x MinGW 64-bit).
   - Core modules required: `Core`, `Widgets`, `Sql`, `Test`.
   - Qt installation path example: `C:\Qt\6.11.2\mingw_64`.
   - MinGW toolchain path example: `C:\Qt\Tools\mingw1310_64\bin`.
5. **Database Server:**
   - **MySQL Server 8.0.x** (or higher) with InnoDB engine.
   - MySQL Client C libraries (`libmysql.dll`) matching the 64-bit MinGW architecture.

---

## 2. MySQL Database Setup

### 2.1 Starting MySQL Service
Ensure the MySQL Server service is running:
```powershell
# In PowerShell (Run as Administrator)
Get-Service -Name MySQL80 | Start-Service
```

### 2.2 Database and User Creation
Open the MySQL command-line client or MySQL Workbench as `root`:
```sql
-- Create database
CREATE DATABASE IF NOT EXISTS smartatc CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

-- Create application user
CREATE USER IF NOT EXISTS 'smartatc_user'@'localhost' IDENTIFIED BY 'smartatc_pass';

-- Grant required privileges
GRANT ALL PRIVILEGES ON smartatc.* TO 'smartatc_user'@'localhost';
FLUSH PRIVILEGES;
```

### 2.3 Running Schema and Seed Scripts
Execute the SQL files in chronological order located in `database/`:
```powershell
cd "d:\Projects\Airport Runway Manager &Resource Allocation\SmartATC\database"

# 1. Base Relational Schema
mysql -u smartatc_user -psmartatc_pass smartatc < schema.sql

# 2. Base Seed Data (Roles, Initial Admin, Aircraft, Runways)
mysql -u smartatc_user -psmartatc_pass smartatc < seed_data.sql

# 3. Apply Schema Migrations
mysql -u smartatc_user -psmartatc_pass smartatc < migrations/migration_day14_runways.sql
mysql -u smartatc_user -psmartatc_pass smartatc < migrations/migration_day16_emergency.sql
mysql -u smartatc_user -psmartatc_pass smartatc < migrations/migration_day17_aging.sql
```

---

## 3. Database Configuration (`db.conf`)

SmartATC connects to MySQL using a local configuration file named `db.conf`. This file is excluded from Git tracking to protect credentials.

Create `db.conf` in the project root (`SmartATC/db.conf`) or copy it to the build output directory (`build/db.conf`):

```ini
[database]
host=localhost
port=3306
dbname=smartatc
user=smartatc_user
password=smartatc_pass
driver=QMYSQL
```

> [!NOTE]
> If `db.conf` is missing, the application automatically enters **Offline Simulation Mode**, allowing in-memory process scheduling, starvation demonstrations, and unit tests to operate without an active database connection.

---

## 4. Building the Project

### 4.1 Configuring Environment Variables
Open PowerShell and configure the Qt and MinGW environment paths:
```powershell
$env:PATH = "C:\Qt\6.11.2\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Program Files\MySQL\MySQL Server 8.0\bin;$env:PATH"
$env:CMAKE_PREFIX_PATH = "C:\Qt\6.11.2\mingw_64"
```

### 4.2 CMake Configuration
Generate the build system using Ninja:
```powershell
cd "d:\Projects\Airport Runway Manager &Resource Allocation\SmartATC"

cmake -B build -G "Ninja" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.2/mingw_64" `
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe" `
  -DCMAKE_C_COMPILER="C:/Qt/Tools/mingw1310_64/bin/gcc.exe"
```

### 4.3 Compiling Binaries
Execute compilation:
```powershell
cmake --build build --config Release
```
This builds:
- `build/bin/smartatc_gui.exe`: Main graphical application.
- `build/bin/manage_users.exe`: Interactive user administration CLI.
- 19 automated CTest binaries in `build/tests/`.

---

## 5. Running Automated Tests

SmartATC includes a 19-suite automated verification suite powered by Qt Test and CTest.

### 5.1 Offline Mode (Fast Execution)
Executes all in-memory OS scheduling, data model, critical section, and threading tests:
```powershell
cd "d:\Projects\Airport Runway Manager &Resource Allocation\SmartATC\build"
ctest --output-on-failure
```
*Expected: 19/19 tests pass (0 failures, ~1 second).*

### 5.2 Live Database Mode (ACID & Transaction Verification)
Ensure `db.conf` is present in `build/` or project root and run tests against the live MySQL instance:
```powershell
ctest --output-on-failure
```
*Expected: 19/19 tests pass (including `TestDatabaseTransactions`, `TestACIDProperties`, and `TestCompleteIntegration`).*

---

## 6. Runtime Deployment (DLL Packaging)

Qt and MySQL dynamic link libraries must be colocated with the executable to run outside of the developer command prompt.

### 6.1 Using the Automated Deployment Script
Execute the bundled PowerShell deployment script:
```powershell
cd "d:\Projects\Airport Runway Manager &Resource Allocation\SmartATC"
powershell -ExecutionPolicy Bypass -File .\scripts\deploy_runtime.ps1
```

### 6.2 Manual Deployment via `windeployqt`
Alternatively, deploy dependencies manually:
```powershell
C:\Qt\6.11.2\mingw_64\bin\windeployqt.exe build/bin/smartatc_gui.exe --dir build/bin/

# Ensure MySQL client library is in the bin directory
Copy-Item "C:\Program Files\MySQL\MySQL Server 8.0\lib\libmysql.dll" -Destination build/bin/
```

---

## 7. Launching the Application

### 7.1 Running the GUI
```powershell
cd "d:\Projects\Airport Runway Manager &Resource Allocation\SmartATC\build\bin"
.\smartatc_gui.exe
```

### 7.2 Initial Login Credentials
The seed database provides default credentials for demonstration:
- **Username:** `admin` | **Password:** `admin123` *(Role: Administrator)*
- **Username:** `atc_operator` | **Password:** `atc123` *(Role: Air Traffic Controller)*
- **Username:** `runway_ops` | **Password:** `ops123` *(Role: Airport Operator)*

---

## 8. Troubleshooting Common Issues

| Issue | Root Cause | Resolution |
|:---|:---|:---|
| `QSqlDatabase: QMYSQL driver not loaded` | Missing `qsqlmysql.dll` plugin or `libmysql.dll` | 1. Ensure `sqldrivers/qsqlmysql.dll` exists in `bin/sqldrivers/`.<br>2. Copy 64-bit `libmysql.dll` directly into `build/bin/`. |
| `Cannot connect to MySQL server on 'localhost'` | MySQL service stopped or credentials incorrect | Verify MySQL service status with `Get-Service MySQL80` and verify credentials in `db.conf`. |
| `Ninja: command not found` | Ninja not in `PATH` | Ensure CMake or Qt's Ninja directory is added to system `PATH` (e.g., `C:\Qt\Tools\Ninja`). |
| Missing MinGW runtime DLLs (`libgcc_s_seh-1.dll`) | MinGW bin directory not in `PATH` | Add `C:\Qt\Tools\mingw1310_64\bin` to `PATH` or run `scripts/deploy_runtime.ps1`. |
