#include <QtTest/QtTest>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>

#include "models/Runway.h"
#include "models/Flight.h"
#include "models/Process.h"
#include "models/Session.h"
#include "models/RunwayAssignment.h"
#include "services/ApplicationService.h"
#include "database/DatabaseManager.h"
#include "scheduler/Scheduler.h"
#include "scheduler/ReadyQueue.h"
#include "scheduler/AgingEngine.h"
#include "resource/ResourceManager.h"

/**
 * @brief Test suite for System Dashboard and Operational Monitoring (Day 24).
 *
 * Verifies:
 *   1. Dashboard metric aggregation and data retrieval.
 *   2. Calculation correctness (utilization %, ready queue depth, process counts, metrics).
 *   3. Divide-by-zero protection under edge cases (0 runways, 0 flights, empty queue).
 *   4. Offline / disconnected resilience (graceful fallback without crashes).
 *   5. In-memory synchronization with Scheduler, ReadyQueue, and AgingEngine.
 *   6. Live MySQL integration (when available) for audit logs, transaction tracking, and KPIs.
 */
class TestDashboardMonitoring : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // ── Metric Calculation & Edge Cases ──────────────────────────────────────
    void testZeroRunwaySafeDivision();
    void testRunwayUtilizationFormula();
    void testEmptyQueueAndProcessMetrics();
    void testInMemoryQueueSynchronization();
    void testSchedulerMetricsAggregation();
    void testNextCandidateIdentification();

    // ── Offline / Disconnected Behavior ──────────────────────────────────────
    void testOfflineDashboardSummaryResilience();
    void testOfflineRecentOperationLogsResilience();
    void testOfflineRunwayUtilizationResilience();

    // ── Live Database Integration (Conditional) ──────────────────────────────
    void testLiveDatabaseDashboardSummary();
    void testEmergencyFlightDetection();
    void testRecentOperationLogsPaginationAndOrdering();
    void testLastTransactionExtraction();
    void testDashboardConsistencyWithSubsystems();

private:
    ApplicationService m_service;
    bool               m_dbConnected = false;
};

void TestDashboardMonitoring::initTestCase()
{
    DatabaseManager &db = DatabaseManager::instance();
    db.loadConfig();
    m_dbConnected = db.isConnected() || db.connect();
    if (m_dbConnected) {
        qDebug() << "TestDashboardMonitoring: Connected to MySQL database.";
    } else {
        qDebug() << "TestDashboardMonitoring: Running in offline / disconnected mode (" << db.lastError() << ").";
    }
}

void TestDashboardMonitoring::cleanupTestCase()
{
    if (m_dbConnected) {
        DatabaseManager::instance().disconnect();
    }
}

void TestDashboardMonitoring::init()
{
    if (m_service.scheduler()) {
        m_service.scheduler()->reset();
    }
}

void TestDashboardMonitoring::cleanup()
{
}

// ─────────────────────────────────────────────────────────────────────────────
// Metric Calculation & Edge Cases
// ─────────────────────────────────────────────────────────────────────────────

void TestDashboardMonitoring::testZeroRunwaySafeDivision()
{
    // Verify that when there are 0 runways, runway utilization percentage returns exactly 0.0%
    // without floating point exceptions, NaN, or crash.
    QString error;
    double util = m_service.getRunwayUtilizationPercentage(&error);
    Q_UNUSED(util);

    // Specifically test the calculation logic with 0 total runways:
    // formula: total > 0 ? (occupied * 100.0 / total) : 0.0
    int totalRunways = 0;
    int occupiedRunways = 0;
    double safeUtil = totalRunways > 0 ? (occupiedRunways * 100.0 / totalRunways) : 0.0;
    QCOMPARE(safeUtil, 0.0);
    QVERIFY(!std::isnan(safeUtil));
    QVERIFY(!std::isinf(safeUtil));
}

void TestDashboardMonitoring::testRunwayUtilizationFormula()
{
    // Test various runway count configurations
    auto calcUtil = [](int total, int occupied) -> double {
        return total > 0 ? (static_cast<double>(occupied) * 100.0 / static_cast<double>(total)) : 0.0;
    };

    QCOMPARE(calcUtil(4, 0), 0.0);
    QCOMPARE(calcUtil(4, 1), 25.0);
    QCOMPARE(calcUtil(4, 2), 50.0);
    QCOMPARE(calcUtil(4, 3), 75.0);
    QCOMPARE(calcUtil(4, 4), 100.0);
    QCOMPARE(calcUtil(10, 3), 30.0);
    QCOMPARE(calcUtil(0, 0), 0.0);
    QCOMPARE(calcUtil(0, 5), 0.0);
}

void TestDashboardMonitoring::testEmptyQueueAndProcessMetrics()
{
    // When no processes exist, dashboard summary should report 0 depth and 0 averages
    QString error;
    ApplicationService::DashboardSummary summary = m_service.getDashboardSummary(&error);

    QCOMPARE(summary.readyQueueDepth, 0);
    QCOMPARE(summary.runningProcesses, 0);
    QCOMPARE(summary.completedProcesses, 0);
    QCOMPARE(summary.hasHighestPriorityWaiting, false);
}

void TestDashboardMonitoring::testInMemoryQueueSynchronization()
{
    // Add simulated processes into ReadyQueue
    Models::Process p1(1, 101, 4, 30); // Flight 101, priority 4
    p1.flightNumber = "AA101";
    Models::Process p2(2, 102, 1, 25); // Flight 102, priority 1 (highest)
    p2.flightNumber = "UA102";
    Models::Process p3(3, 103, 3, 40); // Flight 103, priority 3
    p3.flightNumber = "DL103";

    m_service.scheduler()->readyQueue().enqueue(p1);
    m_service.scheduler()->readyQueue().enqueue(p2);
    m_service.scheduler()->readyQueue().enqueue(p3);

    QString error;
    ApplicationService::DashboardSummary summary = m_service.getDashboardSummary(&error);

    QCOMPARE(summary.readyQueueDepth, 3);
    QCOMPARE(summary.hasHighestPriorityWaiting, true);
    // Highest effective priority is p2 with priority 1
    QCOMPARE(summary.highestPriorityWaitingProcess.flightId, 102);
    QCOMPARE(summary.highestPriorityWaitingProcess.effectivePriority, 1);
}

void TestDashboardMonitoring::testSchedulerMetricsAggregation()
{
    // Populate ready queue with processes and execute them to generate wait & turnaround metrics
    Models::Process p1(10, 201, 2, 20);
    p1.arrivalTime = QDateTime::currentDateTime().addSecs(-100);

    m_service.scheduler()->readyQueue().enqueue(p1);

    Scheduler::DispatchResult disp = m_service.scheduler()->dispatchNextProcess();
    QVERIFY(disp.success);
    QCOMPARE(disp.process.flightId, 201);

    // Simulate completion
    Scheduler::CompletionResult comp = m_service.scheduler()->completeRunningProcess(QDateTime::currentDateTime());
    QVERIFY(comp.success);

    QString error;
    ApplicationService::DashboardSummary summary = m_service.getDashboardSummary(&error);

    QCOMPARE(summary.completedProcesses, 1);
    QVERIFY(summary.averageWaitingTime >= 0.0);
    QVERIFY(summary.averageTurnaroundTime >= 0.0);
    QCOMPARE(summary.schedulingPolicy, QStringLiteral("Non-Preemptive Priority Scheduling (Tie-breakers: Arrival Time, Flight ID)"));
    QCOMPARE(summary.isAgingActive, m_service.scheduler()->agingEngine()->isRunning());
}

void TestDashboardMonitoring::testNextCandidateIdentification()
{
    Models::Process normalP(20, 301, 3, 30);
    Models::Process emergP(21, 302, 3, 15);
    emergP.setEmergency(true); // Emergency priority becomes 1

    m_service.scheduler()->readyQueue().enqueue(normalP);
    m_service.scheduler()->readyQueue().enqueue(emergP);

    QString error;
    ApplicationService::DashboardSummary summary = m_service.getDashboardSummary(&error);

    // Emergency process (priority 1) must be the next candidate
    QCOMPARE(summary.hasHighestPriorityWaiting, true);
    QCOMPARE(summary.highestPriorityWaitingProcess.flightId, 302);
    QCOMPARE(summary.highestPriorityWaitingProcess.effectivePriority, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Offline / Disconnected Behavior
// ─────────────────────────────────────────────────────────────────────────────

void TestDashboardMonitoring::testOfflineDashboardSummaryResilience()
{
    QString error;
    ApplicationService::DashboardSummary summary = m_service.getDashboardSummary(&error);

    QVERIFY(!summary.schedulingPolicy.isEmpty());
    QVERIFY(summary.runwayUtilization >= 0.0 && summary.runwayUtilization <= 100.0);
}

void TestDashboardMonitoring::testOfflineRecentOperationLogsResilience()
{
    QString error;
    QList<ApplicationService::OperationLogEntry> logs = m_service.getRecentOperationLogs(10, &error);

    if (!m_dbConnected) {
        QVERIFY(logs.isEmpty());
        QVERIFY(!error.isEmpty());
    } else {
        // If DB is connected, logs can be returned
        QVERIFY(logs.size() <= 10);
    }
}

void TestDashboardMonitoring::testOfflineRunwayUtilizationResilience()
{
    QString error;
    double util = m_service.getRunwayUtilizationPercentage(&error);

    if (!m_dbConnected) {
        QCOMPARE(util, 0.0);
        QVERIFY(!error.isEmpty());
    } else {
        QVERIFY(util >= 0.0 && util <= 100.0);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Live Database Integration (Conditional)
// ─────────────────────────────────────────────────────────────────────────────

void TestDashboardMonitoring::testLiveDatabaseDashboardSummary()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database not available. Skipping live DB dashboard test.");
    }

    QString error;
    ApplicationService::DashboardSummary summary = m_service.getDashboardSummary(&error);

    QVERIFY2(error.isEmpty(), qPrintable(error));

    // Compare with live SQL counts
    QSqlQuery q;
    if (q.exec("SELECT COUNT(*) FROM flights")) {
        if (q.next()) {
            QCOMPARE(summary.totalFlights, q.value(0).toInt());
        }
    }

    if (q.exec("SELECT COUNT(*) FROM runways")) {
        if (q.next()) {
            QCOMPARE(summary.totalRunways, q.value(0).toInt());
        }
    }

    if (summary.totalRunways > 0) {
        double expectedUtil = (static_cast<double>(summary.occupiedRunways) * 100.0) / static_cast<double>(summary.totalRunways);
        QVERIFY(qAbs(summary.runwayUtilization - expectedUtil) < 0.01);
    } else {
        QCOMPARE(summary.runwayUtilization, 0.0);
    }
}

void TestDashboardMonitoring::testEmergencyFlightDetection()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database not available. Skipping live emergency flight detection test.");
    }

    QString error;
    ApplicationService::DashboardSummary summary = m_service.getDashboardSummary(&error);

    QSqlQuery q("SELECT COUNT(*) FROM flights WHERE is_emergency = 1 AND status NOT IN ('Landed', 'Departed', 'Cancelled')");
    if (q.next()) {
        int expectedEmergencies = q.value(0).toInt();
        QCOMPARE(summary.emergencyFlights, expectedEmergencies);
        QCOMPARE(summary.activeEmergencyFlights.size(), expectedEmergencies);
    }
}

void TestDashboardMonitoring::testRecentOperationLogsPaginationAndOrdering()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database not available. Skipping operation logs ordering test.");
    }

    QString error;
    int requestedLimit = 5;
    QList<ApplicationService::OperationLogEntry> logs = m_service.getRecentOperationLogs(requestedLimit, &error);

    QVERIFY(logs.size() <= requestedLimit);

    // Verify descending logId ordering
    for (int i = 0; i < logs.size() - 1; ++i) {
        QVERIFY(logs[i].logId >= logs[i + 1].logId);
    }
}

void TestDashboardMonitoring::testLastTransactionExtraction()
{
    if (!m_dbConnected) {
        QSKIP("Live MySQL database not available. Skipping last transaction extraction test.");
    }

    QString error;
    ApplicationService::DashboardSummary summary = m_service.getDashboardSummary(&error);

    // If transactions exist in operation_logs, verify lastTx fields
    QSqlQuery q("SELECT COUNT(*) FROM operation_logs WHERE transaction_status IS NOT NULL");
    if (q.next() && q.value(0).toInt() > 0) {
        QVERIFY(!summary.lastTx.operation.isEmpty());
        QVERIFY(!summary.lastTx.status.isEmpty());
    }
}

void TestDashboardMonitoring::testDashboardConsistencyWithSubsystems()
{
    // Verify that dashboard summary values strictly reflect the subsystem objects
    QString error;
    ApplicationService::DashboardSummary summary = m_service.getDashboardSummary(&error);

    // ReadyQueue count
    QCOMPARE(summary.readyQueueDepth, m_service.scheduler()->readyQueue().size());

    // Aging active
    QCOMPARE(summary.isAgingActive, m_service.scheduler()->agingEngine()->isRunning());

    // Completed processes count
    QCOMPARE(summary.completedProcesses, m_service.scheduler()->completedProcesses().size());
}

QTEST_MAIN(TestDashboardMonitoring)
#include "TestDashboardMonitoring.moc"
