# DAY 29 — FINAL VIVA PREPARATION & INDIVIDUAL CONTRIBUTION REPORT

## 1. Executive Summary
- **Project:** Smart Air Traffic Control Management System (SmartATC)
- **Repository:** `https://github.com/DhruvPathak12/Airport-Runway-Manager-Resource-Allocation`
- **Milestone:** Phase 2 — Day 29 of 29 (Final Viva Preparation & Individual Contribution Verification)
- **Status:** **COMPLETE** — All preparation deliverables created and verified against actual source code and MySQL schema.
- **Date:** 2026-11-15

Day 29 marks the final academic preparation milestone for the SmartATC B.Tech capstone project. A comprehensive 10-document viva preparation package was created in `docs/viva/`, covering complete project architecture, deep-dive OS concepts, 8-table relational DBMS theory, C++17/Qt/MySQL integration traces, ACID transaction proofs, a 170-question viva question bank, member-wise contribution profiles, an 18-minute live demonstration script, top 20 examiner traps, and a high-yield quick revision sheet.

> [!NOTE]
> **Nature of Deliverables:**  
> These documents constitute comprehensive academic preparation, revision, and defense materials for the student engineering group. They represent technical study material, not a certification that students have completed oral memorization or defense delivery.

---

## 2. Source-Code & Schema Inspection Summary
- **Source Inspection:** Verified against actual C++ classes (`ApplicationService`, `DatabaseManager`, `AgingEngine`, `ReadyQueue`, `Scheduler`, `ResourceManager`, `Process`, `Flight`, `Aircraft`, `Runway`, `Session`, `User`). No invented controller or repository classes were introduced.
- **Schema Inspection:** Verified against `sql/schema.sql` and live MySQL 8.0 instance. Exactly eight normalized tables confirmed: `roles`, `users`, `aircraft`, `flights`, `runways`, `scheduling_queue`, `runway_assignments`, and `operation_logs`.
- **Git History Inspection:** Confirmed that all repository commits were pushed via a single maintainer account (`Dhruv Pathak` / `DhruvPathak12`) committing on behalf of the project team. The four-member contribution breakdown was established through module ownership and automated test authoring rather than per-commit Git author tags.

---

## 3. Review of Deliverables Created

| Deliverable Document | Primary Scope & Contents |
|---|---|
| [`docs/viva/DAY_29_PROJECT_OVERVIEW.md`](file:///d:/Projects/Airport%20Runway%20Manager%20&Resource%20Allocation/SmartATC/docs/viva/DAY_29_PROJECT_OVERVIEW.md) | Full 17-section project overview: problem statement, motivation, scope, 4-tier system architecture, technology justification, educational disclaimer, and future enhancements. |
| [`docs/viva/DAY_29_OS_VIVA.md`](file:///d:/Projects/Airport%20Runway%20Manager%20&Resource%20Allocation/SmartATC/docs/viva/DAY_29_OS_VIVA.md) | Deep-dive OS viva guide: PCB attributes, C++ states (`New`, `Ready`, `Running`, `Blocked`, `Completed`), Non-Preemptive Priority Scheduling, tie-breakers, dynamic emergency escalation, 30s aging formula, scheduling metrics (TAT, WT), and runway mutex synchronization. |
| [`docs/viva/DAY_29_DBMS_VIVA.md`](file:///d:/Projects/Airport%20Runway%20Manager%20&Resource%20Allocation/SmartATC/docs/viva/DAY_29_DBMS_VIVA.md) | Complete 8-table relational dictionary: PKs, FKs, constraints, 1NF/2NF/3NF normalization analysis, prepared statements, and SQL injection prevention. |
| [`docs/viva/DAY_29_CPP_QT_MYSQL_VIVA.md`](file:///d:/Projects/Airport%20Runway%20Manager%20&Resource%20Allocation/SmartATC/docs/viva/DAY_29_CPP_QT_MYSQL_VIVA.md) | Technical guide to C++17 (RAII, smart pointers), Qt 6 Widgets, Signals and Slots, `DatabaseManager` singleton, and the 17-step end-to-end execution trace of a GUI action. |
| [`docs/viva/DAY_29_TRANSACTION_ACID_VIVA.md`](file:///d:/Projects/Airport%20Runway%20Manager%20&Resource%20Allocation/SmartATC/docs/viva/DAY_29_TRANSACTION_ACID_VIVA.md) | Deep-dive on transactions: exact 6-step multi-table allocation sequence, 4 ACID properties, pessimistic row locking (`SELECT ... FOR UPDATE`), dirty reads/lost updates/deadlocks, and Type A vs. Type B rollback distinction. |
| [`docs/viva/DAY_29_TEAM_CONTRIBUTIONS.md`](file:///d:/Projects/Airport%20Runway%20Manager%20&Resource%20Allocation/SmartATC/docs/viva/DAY_29_TEAM_CONTRIBUTIONS.md) | Comprehensive 4-member contribution profiles: assigned duties, actual implemented files/classes, tables, authored tests, integration work, challenges solved, 10 individual viva questions, and 60-second / 2-minute oral defense scripts per member. |
| [`docs/viva/DAY_29_QUESTION_BANK.md`](file:///d:/Projects/Airport%20Runway%20Manager%20&Resource%20Allocation/SmartATC/docs/viva/DAY_29_QUESTION_BANK.md) | Exhaustive 170-question viva question bank with short answers, detailed explanations, and SmartATC implementation examples across 7 core categories. |
| [`docs/viva/DAY_29_DEMONSTRATION_SCRIPT.md`](file:///d:/Projects/Airport%20Runway%20Manager%20&Resource%20Allocation/SmartATC/docs/viva/DAY_29_DEMONSTRATION_SCRIPT.md) | Realistic 18-minute live demonstration script with exact screens, actions, expected results, concepts demonstrated, and presenter lines for each of the 9 demonstration steps. |
| [`docs/viva/DAY_29_EXAMINER_TRAPS.md`](file:///d:/Projects/Airport%20Runway%20Manager%20&Resource%20Allocation/SmartATC/docs/viva/DAY_29_EXAMINER_TRAPS.md) | Strategic guide to top 20 examiner trap questions (e.g. real ATC system, kernel processes, preemption, mutex vs. row locks, durability verification scope). |
| [`docs/viva/DAY_29_FINAL_REVISION_SHEET.md`](file:///d:/Projects/Airport%20Runway%20Manager%20&Resource%20Allocation/SmartATC/docs/viva/DAY_29_FINAL_REVISION_SHEET.md) | High-yield 1-page quick revision sheet: key numbers, formulas (TAT, WT, Aging, Utilization), 8-table cheat sheet, real class names, and top 5 viva answers. |

---

## 4. Missing Evidence and Technical Limitations Honestly Disclosed
1. **Git Authorship Evidence Gap:** In `git log`, all commits carry a single author identity (`Dhruv Pathak`). There is no per-member commit history in Git. Member attributions are based on documented module ownership and functional subsystem responsibilities.
2. **Password Cryptography:** Passwords use unsalted SHA-256 (educational prototype design). Production hardening requires Argon2id or bcrypt.
3. **Durability Verification:** Verified across application shutdown and restarts via MySQL write-ahead redo logs; raw hardware-level power-cut failure was not tested.
4. **Single Active Scheduling Record per Flight:** The `uq_sq_flight_id` constraint enforces a strict 1:0..1 relationship.

---

## 5. Git Status and Commit Information
- **Pre-Day 29 HEAD Commit:** `d99cbaf129f12239f6916fb2f57b6f849635eeb7`
- **Files Staged for Day 29:** 10 new documents in `docs/viva/`, updated `PROJECT_STATUS.md`, and this report `docs/DAY_29_FINAL_VIVA_PREPARATION_REPORT.md`.
- **Remote Push:** Ready to commit and push to `origin/main`.
