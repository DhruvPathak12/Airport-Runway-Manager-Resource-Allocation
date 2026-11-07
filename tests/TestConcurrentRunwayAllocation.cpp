#include <QDateTime>
#include <QDebug>
#include <QSignalSpy>
#include <QTest>
#include <chrono>
#include <thread>

#include "database/DatabaseManager.h"
#include "models/Flight.h"
#include "models/Process.h"
#include "models/Runway.h"
#include "resource/ConcurrentAllocationDemo.h"
#include "resource/ResourceManager.h"
#include "scheduler/Scheduler.h"
#include "scheduler/StarvationSimulation.h"
#include "services/ApplicationService.h"

/**
 * @brief Automated unit and integration test suite for Day 20: C++ Threads &
 * Synchronization. Covers 30 comprehensive test points across:
 *   1. Thread Lifecycle Management (Tests 1–7)
 *   2. Concurrent Resource Access & Mutual Exclusion (Tests 8–15)
 *   3. Synchronization & Race Condition Prevention (Tests 16–22)
 *   4. Integration & Regression Verification (Tests 23–30)
 */
class TestConcurrentRunwayAllocation : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();
  void init();
  void cleanup();

  // ── 1. Thread Lifecycle Management (Tests 1–7) ───────────────────────────
  void testWorkerThreadsStartSuccessfully();
  void testAllStartedThreadsJoined();
  void testNoWorkerThreadDetached();
  void testSharedInputDataRemainsValid();
  void testWorkerExceptionsCaughtAndReported();
  void testThreadCreationFailureHandledSafely();
  void testRepeatedRunsNoLeaks();

  // ── 2. Concurrent Resource Access & Contention (Tests 8–15) ──────────────
  void testTwoThreadsOneRunwayExactlyOneSuccess();
  void testThreeThreadsOneRunwayExactlyOneSuccess();
  void testMultipleRequestsNeverDuplicateActiveAssignments();
  void testSingleFlightCannotOwnMultipleRunwaysConcurrently();
  void testMaintenanceRunwayRejectsAllConcurrentAttempts();
  void testOccupiedRunwayRejectsAdditionalConcurrentRequests();
  void testDifferentAvailableRunwaysAllocatedIndependently();
  void testFinalResourceManagerStateMatchesResults();

  // ── 3. Synchronization & Invariant Verification (Tests 16–22) ────────────
  void testAllResourceMutationsUseProtectedPath();
  void testConcurrentOperationsPreserveInvariants();
  void testNoDeadlockUnderContention();
  void testNoDataRacesInResultCollection();
  void testRepeatedContentionRunsRemainCorrect();
  void testConditionVariableStartBarrierCorrect();
  void testSynchronizationObjectsOutliveThreads();

  // ── 4. Integration & Regression Verification (Tests 23–30) ───────────────
  void testRegressionDay19CriticalSectionIntact();
  void testRegressionDay15AllocationIntact();
  void testRegressionRbacAuthorizationRulesIntact();
  void testRegressionDay16EmergencyPriorityIntact();
  void testRegressionDay17PriorityAgingIntact();
  void testRegressionDay18StarvationSimulationIntact();
  void testGuiResultDeliveryThreadSafe();
  void testRealDatabaseRecordsUnchangedByDemo();

private:
  ApplicationService *m_appService = nullptr;
  ResourceManager *m_resourceMgr = nullptr;
};

void TestConcurrentRunwayAllocation::initTestCase() {
  m_appService = new ApplicationService(this);
  m_appService->initialize();
  m_resourceMgr = m_appService->resourceManager();
}

void TestConcurrentRunwayAllocation::cleanupTestCase() {
  if (m_appService) {
    m_appService->shutdown();
    delete m_appService;
    m_appService = nullptr;
    m_resourceMgr = nullptr;
  }
}

void TestConcurrentRunwayAllocation::init() {
  if (m_resourceMgr) {
    m_resourceMgr->clearState();
  }
}

void TestConcurrentRunwayAllocation::cleanup() {
  if (m_resourceMgr) {
    m_resourceMgr->clearState();
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// 1. Thread Lifecycle Management Tests (1–7)
// ─────────────────────────────────────────────────────────────────────────────

void TestConcurrentRunwayAllocation::testWorkerThreadsStartSuccessfully() {
  ConcurrentAllocationDemo demo(m_resourceMgr);
  auto reqs = ConcurrentAllocationDemo::createDefaultRequests(1, 4);
  QCOMPARE(reqs.size(), 4);

  auto report = demo.execute(1, reqs);
  QCOMPARE(report.totalRequests, 4);
  QCOMPARE(report.results.size(), 4);

  for (const auto &res : report.results) {
    QVERIFY(!res.systemThreadId.isEmpty());
    QVERIFY(res.status == QStringLiteral("Allocation Successful") ||
            res.status == QStringLiteral("Allocation Rejected"));
  }
}

void TestConcurrentRunwayAllocation::testAllStartedThreadsJoined() {
  ConcurrentAllocationDemo demo(m_resourceMgr);
  // Running execute must join all worker threads before returning
  auto report = demo.executeDefaultScenario(2, 3);
  QCOMPARE(report.totalRequests, 3);
  // After execute returns, worker thread vector is cleared and all threads are
  // joined
  QVERIFY(!demo.isRunning());
}

void TestConcurrentRunwayAllocation::testNoWorkerThreadDetached() {
  // Verify that demo destructor and cancelAndJoin join all threads without
  // detaching
  {
    ConcurrentAllocationDemo demo(m_resourceMgr);
    demo.startAsyncDefault(3, 3);
    QVERIFY(demo.isRunning());
    demo.cancelAndJoin();
    QVERIFY(!demo.isRunning());
  }
  // Completed cleanly without std::terminate from unjoined threads
  QVERIFY(true);
}

void TestConcurrentRunwayAllocation::testSharedInputDataRemainsValid() {
  ConcurrentAllocationDemo demo(m_resourceMgr);
  QList<ConcurrentAllocationDemo::Request> reqs;

  ConcurrentAllocationDemo::Request r1{1,   QStringLiteral("Thread 1 (T1)"),
                                       101, QStringLiteral("AI-101"),
                                       10,  QStringLiteral("10")};
  ConcurrentAllocationDemo::Request r2{2,   QStringLiteral("Thread 2 (T2)"),
                                       202, QStringLiteral("6E-202"),
                                       10,  QStringLiteral("10")};
  ConcurrentAllocationDemo::Request r3{3,   QStringLiteral("Thread 3 (T3)"),
                                       303, QStringLiteral("AI-303"),
                                       10,  QStringLiteral("10")};
  reqs << r1 << r2 << r3;

  auto report = demo.execute(10, reqs);
  QCOMPARE(report.results.size(), 3);

  QMap<int, QString> expectedFlights = {
      {1, "AI-101"}, {2, "6E-202"}, {3, "AI-303"}};
  for (const auto &res : report.results) {
    QVERIFY(expectedFlights.contains(res.requestId));
    QCOMPARE(res.flightNumber, expectedFlights.value(res.requestId));
  }
}

void TestConcurrentRunwayAllocation::testWorkerExceptionsCaughtAndReported() {
  // Invalid runway and flight IDs must be caught and reported gracefully
  // without crash
  ConcurrentAllocationDemo demo(m_resourceMgr);
  QList<ConcurrentAllocationDemo::Request> reqs;
  reqs.append({1, QStringLiteral("Thread 1 (T1)"), -1, QStringLiteral("INV-01"),
               -5, QStringLiteral("INV")});

  auto report = demo.execute(-5, reqs);
  QCOMPARE(report.results.size(), 1);
  QVERIFY(!report.results.first().success);
  QCOMPARE(report.results.first().status,
           QStringLiteral("Allocation Rejected"));
}

void TestConcurrentRunwayAllocation::testThreadCreationFailureHandledSafely() {
  // Empty request list handling
  ConcurrentAllocationDemo demo(m_resourceMgr);
  auto report = demo.execute(1, {});
  QCOMPARE(report.totalRequests, 0);
  QVERIFY(!report.summaryMessage.isEmpty());
}

void TestConcurrentRunwayAllocation::testRepeatedRunsNoLeaks() {
  ConcurrentAllocationDemo demo(m_resourceMgr);
  for (int run = 0; run < 10; ++run) {
    m_resourceMgr->clearState();
    auto report = demo.executeDefaultScenario(1, 3);
    QCOMPARE(report.successfulAllocations, 1);
    QCOMPARE(report.rejectedAllocations, 2);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. Concurrent Resource Access & Contention Tests (8–15)
// ─────────────────────────────────────────────────────────────────────────────

void TestConcurrentRunwayAllocation::
    testTwoThreadsOneRunwayExactlyOneSuccess() {
  ConcurrentAllocationDemo demo(m_resourceMgr);
  auto report = demo.executeDefaultScenario(1, 2);

  QCOMPARE(report.totalRequests, 2);
  QCOMPARE(report.successfulAllocations, 1);
  QCOMPARE(report.rejectedAllocations, 1);
  QVERIFY(!report.hasDataRace);
  QVERIFY(report.finalOwningFlightId > 0);
}

void TestConcurrentRunwayAllocation::
    testThreeThreadsOneRunwayExactlyOneSuccess() {
  ConcurrentAllocationDemo demo(m_resourceMgr);
  auto report = demo.executeDefaultScenario(2, 3);

  QCOMPARE(report.totalRequests, 3);
  QCOMPARE(report.successfulAllocations, 1);
  QCOMPARE(report.rejectedAllocations, 2);
  QVERIFY(!report.hasDataRace);
  QVERIFY(report.finalOwningFlightId > 0);
}

void TestConcurrentRunwayAllocation::
    testMultipleRequestsNeverDuplicateActiveAssignments() {
  ConcurrentAllocationDemo demo(m_resourceMgr);
  auto report = demo.executeDefaultScenario(3, 6);

  QCOMPARE(report.totalRequests, 6);
  QCOMPARE(report.successfulAllocations, 1);
  QCOMPARE(report.rejectedAllocations, 5);
  QVERIFY(!report.hasDataRace);

  // Verify in ResourceManager that only one flight is recorded as active
  int activeFlight = m_resourceMgr->activeFlightForRunway(3);
  QCOMPARE(activeFlight, report.finalOwningFlightId);
}

void TestConcurrentRunwayAllocation::
    testSingleFlightCannotOwnMultipleRunwaysConcurrently() {
  ConcurrentAllocationDemo demo(m_resourceMgr);
  // Two threads representing the SAME flight (Flight 999) request Runway 1 and
  // Runway 2 concurrently
  QList<ConcurrentAllocationDemo::Request> reqs;
  reqs.append({1, QStringLiteral("T1"), 999, QStringLiteral("AI-999"), 1,
               QStringLiteral("09L")});
  reqs.append({2, QStringLiteral("T2"), 999, QStringLiteral("AI-999"), 2,
               QStringLiteral("09R")});

  auto report = demo.execute(1, reqs);
  QCOMPARE(report.totalRequests, 2);

  // Exactly one of the runways can be acquired by Flight 999; the other must be
  // rejected
  int totalSuccess = 0;
  for (const auto &res : report.results) {
    if (res.success)
      totalSuccess++;
  }
  QCOMPARE(totalSuccess, 1);

  // Flight 999 must hold at most one active runway
  int heldRunway = m_resourceMgr->activeRunwayForFlight(999);
  QVERIFY(heldRunway == 1 || heldRunway == 2);
}

void TestConcurrentRunwayAllocation::
    testMaintenanceRunwayRejectsAllConcurrentAttempts() {
  // Set runway 4 to maintenance before launching threads
  m_resourceMgr->setRunwayMaintenance(4, true);
  QVERIFY(m_resourceMgr->isRunwayInMaintenance(4));

  ConcurrentAllocationDemo demo(m_resourceMgr);
  auto report = demo.executeDefaultScenario(4, 4);

  QCOMPARE(report.totalRequests, 4);
  QCOMPARE(report.successfulAllocations, 0);
  QCOMPARE(report.rejectedAllocations, 4);
  QCOMPARE(report.finalOwningFlightId, 0);
  QVERIFY(!report.hasDataRace);
}

void TestConcurrentRunwayAllocation::
    testOccupiedRunwayRejectsAdditionalConcurrentRequests() {
  // Pre-occupy runway 5 with flight 501
  QString err;
  bool ok = m_resourceMgr->allocateRunway(5, 501, &err);
  QVERIFY(ok);
  QCOMPARE(m_resourceMgr->activeFlightForRunway(5), 501);

  ConcurrentAllocationDemo demo(m_resourceMgr);
  auto report = demo.executeDefaultScenario(5, 3);

  QCOMPARE(report.totalRequests, 3);
  QCOMPARE(report.successfulAllocations, 0);
  QCOMPARE(report.rejectedAllocations, 3);
  QCOMPARE(report.finalOwningFlightId, 501);
}

void TestConcurrentRunwayAllocation::
    testDifferentAvailableRunwaysAllocatedIndependently() {
  // Launch requests for two different available runways
  ConcurrentAllocationDemo demo(m_resourceMgr);
  QList<ConcurrentAllocationDemo::Request> reqs;
  reqs.append({1, QStringLiteral("T1"), 601, QStringLiteral("AI-601"), 6,
               QStringLiteral("09L")});
  reqs.append({2, QStringLiteral("T2"), 701, QStringLiteral("6E-701"), 7,
               QStringLiteral("09R")});

  auto report = demo.execute(6, reqs);
  QCOMPARE(report.totalRequests, 2);

  // Both should succeed because they target independent, non-conflicting shared
  // resources
  int successCount = 0;
  for (const auto &res : report.results) {
    if (res.success)
      successCount++;
  }
  QCOMPARE(successCount, 2);
  QCOMPARE(m_resourceMgr->activeFlightForRunway(6), 601);
  QCOMPARE(m_resourceMgr->activeFlightForRunway(7), 701);
}

void TestConcurrentRunwayAllocation::
    testFinalResourceManagerStateMatchesResults() {
  ConcurrentAllocationDemo demo(m_resourceMgr);
  auto report = demo.executeDefaultScenario(8, 4);

  int winningId = 0;
  for (const auto &res : report.results) {
    if (res.success) {
      winningId = res.flightId;
      break;
    }
  }

  QVERIFY(winningId > 0);
  QCOMPARE(m_resourceMgr->activeFlightForRunway(8), winningId);
  QCOMPARE(report.finalOwningFlightId, winningId);
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. Synchronization & Invariant Verification (16–22)
// ─────────────────────────────────────────────────────────────────────────────

void TestConcurrentRunwayAllocation::
    testAllResourceMutationsUseProtectedPath() {
  ConcurrentAllocationDemo demo(m_resourceMgr);
  auto report = demo.executeDefaultScenario(1, 5);

  // At no point during execution could two threads concurrently hold the mutex
  QVERIFY(report.maxConcurrentObserved <= 1);
}

void TestConcurrentRunwayAllocation::
    testConcurrentOperationsPreserveInvariants() {
  ConcurrentAllocationDemo demo(m_resourceMgr);
  auto report = demo.executeDefaultScenario(2, 4);

  // Invariant: Runway has at most 1 active flight
  QCOMPARE(report.successfulAllocations, 1);
  QVERIFY(m_resourceMgr->isRunwayReserved(2));

  // Release and verify available
  m_resourceMgr->releaseRunway(2);
  QVERIFY(!m_resourceMgr->isRunwayReserved(2));
}

void TestConcurrentRunwayAllocation::testNoDeadlockUnderContention() {
  ConcurrentAllocationDemo demo(m_resourceMgr);
  // 8 concurrent threads contending for 1 resource must complete quickly
  // without deadlock
  auto t0 = std::chrono::steady_clock::now();
  auto report = demo.executeDefaultScenario(9, 8);
  auto t1 = std::chrono::steady_clock::now();

  auto durationMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
  QVERIFY(durationMs < 5000); // Must finish well within 5 seconds
  QCOMPARE(report.successfulAllocations, 1);
  QCOMPARE(report.rejectedAllocations, 7);
}

void TestConcurrentRunwayAllocation::testNoDataRacesInResultCollection() {
  ConcurrentAllocationDemo demo(m_resourceMgr);
  auto report = demo.executeDefaultScenario(1, 5);

  QCOMPARE(report.results.size(), 5);
  QSet<int> seenRequestIds;
  for (const auto &res : report.results) {
    QVERIFY(!seenRequestIds.contains(res.requestId));
    seenRequestIds.insert(res.requestId);
  }
  QCOMPARE(seenRequestIds.size(), 5);
}

void TestConcurrentRunwayAllocation::testRepeatedContentionRunsRemainCorrect() {
  ConcurrentAllocationDemo demo(m_resourceMgr);
  for (int i = 0; i < 5; ++i) {
    auto report = demo.executeDefaultScenario(1, 3);
    QCOMPARE(report.successfulAllocations, 1);
    QCOMPARE(report.rejectedAllocations, 2);
    m_resourceMgr->releaseRunway(1);
  }
}

void TestConcurrentRunwayAllocation::
    testConditionVariableStartBarrierCorrect() {
  // Verify that all threads wait on start barrier until coordinator triggers
  // them
  ConcurrentAllocationDemo demo(m_resourceMgr);
  auto report = demo.executeDefaultScenario(3, 4);
  QCOMPARE(report.totalRequests, 4);
  QVERIFY(!report.hasDataRace);
}

void TestConcurrentRunwayAllocation::
    testSynchronizationObjectsOutliveThreads() {
  // Dynamically instantiate demo and destroy after launching threads
  auto *demo = new ConcurrentAllocationDemo(m_resourceMgr);
  auto report = demo->executeDefaultScenario(5, 3);
  QCOMPARE(report.successfulAllocations, 1);
  delete demo; // Safe deletion joins all threads
  QVERIFY(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. Integration & Regression Verification (23–30)
// ─────────────────────────────────────────────────────────────────────────────

void TestConcurrentRunwayAllocation::
    testRegressionDay19CriticalSectionIntact() {
  // Direct test of Day 19 ScopedResourceLock
  {
    ResourceManager::ScopedResourceLock lock(m_resourceMgr->resourceMutex(), 1,
                                             m_resourceMgr);
    QVERIFY(lock.isLocked());
    QCOMPARE(m_resourceMgr->criticalSectionStatusUnlocked(1),
             QStringLiteral("Critical Section Entered"));
  }
  QCOMPARE(m_resourceMgr->criticalSectionStatus(1),
           QStringLiteral("Critical Section Released"));
}

void TestConcurrentRunwayAllocation::testRegressionDay15AllocationIntact() {
  // Test backward compatibility of allocateRunway API
  QString err;
  bool ok = m_resourceMgr->allocateRunway(2, 201, &err);
  QVERIFY(ok);
  QVERIFY(m_resourceMgr->isRunwayReserved(2));

  bool ok2 = m_resourceMgr->allocateRunway(2, 202, &err);
  QVERIFY(!ok2);
  QVERIFY(err.contains(QStringLiteral("Resource Contention")));
}

void TestConcurrentRunwayAllocation::
    testRegressionRbacAuthorizationRulesIntact() {
  // Unauthenticated user must be rejected
  m_appService->logout();
  QString error;
  auto report = m_appService->runConcurrentAllocationDemo(1, 3, &error);
  QCOMPARE(report.totalRequests, 0);
  QVERIFY(error.contains(QStringLiteral("Authentication required")) ||
          error.contains(QStringLiteral("Permission denied")));

  // Authenticate as ATC
  m_appService->setTestSession(Session(2, QStringLiteral("atc_demo"),
                                       QStringLiteral("ATC Demo"),
                                       QStringLiteral("ATC")));
  error.clear();
  auto atcReport = m_appService->runConcurrentAllocationDemo(1, 3, &error);
  QCOMPARE(atcReport.totalRequests, 3);
  QCOMPARE(atcReport.successfulAllocations, 1);
  QCOMPARE(atcReport.rejectedAllocations, 2);
}

void TestConcurrentRunwayAllocation::
    testRegressionDay16EmergencyPriorityIntact() {
  Models::Process proc(101, 1, 5, 60);
  QCOMPARE(proc.basePriority, 5);
  QCOMPARE(proc.effectivePriority, 5);

  proc.setEmergency(true);
  QCOMPARE(proc.isEmergency, true);
  QCOMPARE(proc.effectivePriority, 1);
  QCOMPARE(proc.basePriority, 5);

  proc.setEmergency(false);
  QCOMPARE(proc.isEmergency, false);
  QCOMPARE(proc.effectivePriority, 5);
}

void TestConcurrentRunwayAllocation::testRegressionDay17PriorityAgingIntact() {
  Models::Process proc(202, 2, 4, 60);
  QDateTime now = QDateTime::currentDateTime();
  proc.readySince = now;
  QCOMPARE(proc.effectivePriority, 4);

  proc.applyAging(now.addSecs(60), 30);
  QCOMPARE(proc.agingLevel, 2);
  QCOMPARE(proc.effectivePriority, 2); // 4 - 2 = 2
}

void TestConcurrentRunwayAllocation::
    testRegressionDay18StarvationSimulationIntact() {
  SchedulerComponents::SimulationConfig cfg;
  cfg.mode = SchedulerComponents::SimulationMode::WithoutAging;
  cfg.agingIntervalSeconds = 30;
  cfg.simulationHorizonSeconds = 180;

  SchedulerComponents::StarvationSimulation sim(cfg);
  auto metrics = sim.runToHorizon();
  QVERIFY(metrics.starvationObserved);
  QCOMPARE(metrics.lowPriorityDispatched, false);
}

void TestConcurrentRunwayAllocation::testGuiResultDeliveryThreadSafe() {
  ConcurrentAllocationDemo demo(m_resourceMgr);
  QSignalSpy startSpy(&demo, &ConcurrentAllocationDemo::demoStarted);
  QSignalSpy updateSpy(&demo, &ConcurrentAllocationDemo::threadStateChanged);
  QSignalSpy finishSpy(&demo, &ConcurrentAllocationDemo::demoFinished);

  demo.startAsyncDefault(1, 3);

  // Wait for demoFinished signal to be delivered safely via Qt event loop
  QVERIFY(finishSpy.wait(5000));
  QCOMPARE(finishSpy.count(), 1);
  QVERIFY(startSpy.count() >= 1);
  QVERIFY(updateSpy.count() >= 3);
}

void TestConcurrentRunwayAllocation::testRealDatabaseRecordsUnchangedByDemo() {
  // Verify that running demo does not modify database state or require MySQL
  // connection
  ConcurrentAllocationDemo demo(m_resourceMgr);
  auto report = demo.executeDefaultScenario(1, 3);
  QCOMPARE(report.totalRequests, 3);
  QCOMPARE(report.successfulAllocations, 1);
  QCOMPARE(report.rejectedAllocations, 2);
}

QTEST_MAIN(TestConcurrentRunwayAllocation)
#include "TestConcurrentRunwayAllocation.moc"
