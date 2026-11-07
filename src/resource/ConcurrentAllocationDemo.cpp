#include "ConcurrentAllocationDemo.h"
#include "ResourceManager.h"

#include <sstream>
#include <QDateTime>

ConcurrentAllocationDemo::ConcurrentAllocationDemo(ResourceManager *resourceManager, QObject *parent)
    : QObject(parent)
    , m_resourceMgr(resourceManager)
{
    qRegisterMetaType<ConcurrentAllocationDemo::Request>("ConcurrentAllocationDemo::Request");
    qRegisterMetaType<ConcurrentAllocationDemo::ThreadResult>("ConcurrentAllocationDemo::ThreadResult");
    qRegisterMetaType<ConcurrentAllocationDemo::DemoReport>("ConcurrentAllocationDemo::DemoReport");
}

ConcurrentAllocationDemo::~ConcurrentAllocationDemo()
{
    cancelAndJoin();
}

QList<ConcurrentAllocationDemo::Request> ConcurrentAllocationDemo::createDefaultRequests(int runwayId, int count)
{
    QList<Request> reqs;
    if (count <= 0) {
        return reqs;
    }

    const QStringList flightNumbers = {
        QStringLiteral("AI-101"),
        QStringLiteral("6E-202"),
        QStringLiteral("AI-303"),
        QStringLiteral("SG-404"),
        QStringLiteral("UK-505"),
        QStringLiteral("QP-606"),
        QStringLiteral("IX-707"),
        QStringLiteral("G8-808"),
        QStringLiteral("I5-909"),
        QStringLiteral("6E-999")
    };

    const QList<int> flightIds = {101, 202, 303, 404, 505, 606, 707, 808, 909, 999};

    for (int i = 0; i < count; ++i) {
        Request r;
        r.requestId = i + 1;
        r.threadLabel = QStringLiteral("Thread %1 (T%1)").arg(i + 1);
        r.flightId = (i < flightIds.size()) ? flightIds[i] : (1000 + i);
        r.flightNumber = (i < flightNumbers.size()) ? flightNumbers[i] : QStringLiteral("FL-%1").arg(1000 + i);
        r.runwayId = runwayId;
        r.runwayNumber = QStringLiteral("RW-%1").arg(runwayId);
        reqs.append(r);
    }
    return reqs;
}

ConcurrentAllocationDemo::DemoReport ConcurrentAllocationDemo::executeDefaultScenario(int runwayId, int threadCount)
{
    return execute(runwayId, createDefaultRequests(runwayId, threadCount));
}

ConcurrentAllocationDemo::DemoReport ConcurrentAllocationDemo::execute(int runwayId, const QList<Request> &requests)
{
    DemoReport report;
    report.runwayId = runwayId;
    report.runwayNumber = QStringLiteral("RW-%1").arg(runwayId);
    report.totalRequests = requests.size();

    if (!m_resourceMgr) {
        report.summaryMessage = QStringLiteral("Error: ResourceManager is null.");
        return report;
    }

    if (requests.isEmpty()) {
        report.summaryMessage = QStringLiteral("Warning: No requests provided for concurrency demonstration.");
        return report;
    }

    // Reset synchronization and lifecycle state
    m_abortFlag.store(false);
    m_startFlag.store(false);
    m_readyCount.store(0);

    {
        std::lock_guard<std::mutex> rLock(m_resultsMutex);
        m_results.clear();
    }

    // Clean up any remaining worker threads
    for (auto &t : m_workerThreads) {
        if (t.joinable()) {
            t.join();
        }
    }
    m_workerThreads.clear();

    emit demoStarted(runwayId, requests.size());

    // 1. Thread Creation & Lifecycle Management
    int totalWorkers = requests.size();
    m_workerThreads.reserve(totalWorkers);

    try {
        for (int i = 0; i < totalWorkers; ++i) {
            m_workerThreads.emplace_back(&ConcurrentAllocationDemo::workerRoutine, this, requests[i]);
        }
    } catch (const std::exception &ex) {
        // Safe thread creation failure handling: abort and join already-spawned threads
        m_abortFlag.store(true);
        m_startCv.notify_all();

        for (auto &t : m_workerThreads) {
            if (t.joinable()) {
                t.join();
            }
        }
        m_workerThreads.clear();

        report.summaryMessage = QStringLiteral("Thread creation failure: %1").arg(ex.what());
        return report;
    }

    // 2. Start Barrier Synchronization using Condition Variable
    {
        std::unique_lock<std::mutex> lk(m_startMutex);
        m_startCv.wait(lk, [this, totalWorkers]() {
            return m_readyCount.load() >= totalWorkers || m_abortFlag.load();
        });

        // Release all worker threads simultaneously to create genuine resource contention
        m_startFlag.store(true);
        m_startCv.notify_all();
    }

    // 3. Thread Joining & Cleanup (No detached threads; all workers guaranteed joined)
    for (auto &t : m_workerThreads) {
        if (t.joinable()) {
            t.join();
        }
    }
    m_workerThreads.clear();

    // 4. Results Aggregation and Invariant Verification
    {
        std::lock_guard<std::mutex> rLock(m_resultsMutex);
        report.results = m_results;
    }

    report.successfulAllocations = 0;
    report.rejectedAllocations = 0;
    int winningFlightId = 0;
    QString winningFlightNum;

    for (const auto &res : report.results) {
        if (res.success) {
            report.successfulAllocations++;
            winningFlightId = res.flightId;
            winningFlightNum = res.flightNumber;
        } else {
            report.rejectedAllocations++;
        }
    }

    report.finalOwningFlightId = m_resourceMgr->activeFlightForRunway(runwayId);
    report.maxConcurrentObserved = m_resourceMgr->maxConcurrentObserved();

    if (report.finalOwningFlightId == winningFlightId && !winningFlightNum.isEmpty()) {
        report.finalOwningFlightNumber = winningFlightNum;
    } else if (report.finalOwningFlightId > 0) {
        report.finalOwningFlightNumber = QStringLiteral("Flight #%1").arg(report.finalOwningFlightId);
    } else {
        report.finalOwningFlightNumber = QStringLiteral("None (Available)");
    }

    // 5. Data Race & Invariant Evaluation
    if (report.successfulAllocations > 1) {
        report.hasDataRace = true;
        report.summaryMessage = QStringLiteral("CRITICAL RACE CONDITION: %1 concurrent allocations succeeded simultaneously!")
                                    .arg(report.successfulAllocations);
    } else if (report.successfulAllocations == 1) {
        if (report.finalOwningFlightId != winningFlightId) {
            report.hasDataRace = true;
            report.summaryMessage = QStringLiteral("INCONSISTENCY: Winning thread allocated flight %1 but ResourceManager reports flight %2.")
                                        .arg(winningFlightId).arg(report.finalOwningFlightId);
        } else {
            report.hasDataRace = false;
            report.summaryMessage = QStringLiteral("Mutual Exclusion Verified: Exactly 1 thread won reservation (%1), %2 threads safely rejected with contention errors.")
                                        .arg(winningFlightNum.isEmpty() ? QStringLiteral("Flight #%1").arg(winningFlightId) : winningFlightNum)
                                        .arg(report.rejectedAllocations);
        }
    } else {
        report.hasDataRace = false;
        report.summaryMessage = QStringLiteral("All %1 requests rejected safely (Runway was unavailable, occupied, or in maintenance).")
                                    .arg(report.totalRequests);
    }

    return report;
}

void ConcurrentAllocationDemo::workerRoutine(const Request &req)
{
    ThreadResult res;
    res.requestId = req.requestId;
    res.threadLabel = req.threadLabel;
    res.flightId = req.flightId;
    res.flightNumber = req.flightNumber;
    res.runwayId = req.runwayId;
    res.status = QStringLiteral("Waiting to Start");

    // Capture system thread ID
    std::stringstream ss;
    ss << std::this_thread::get_id();
    res.systemThreadId = QString::fromStdString(ss.str());

    // 1. Signal readiness to Start Barrier and wait for simultaneous release
    {
        std::unique_lock<std::mutex> lk(m_startMutex);
        m_readyCount++;
        m_startCv.notify_all();
        m_startCv.wait(lk, [this]() {
            return m_startFlag.load() || m_abortFlag.load();
        });
    }

    if (m_abortFlag.load()) {
        res.status = QStringLiteral("Thread Aborted");
        res.message = QStringLiteral("Execution aborted before resource request.");
        res.success = false;

        std::lock_guard<std::mutex> rLock(m_resultsMutex);
        m_results.append(res);

        QMetaObject::invokeMethod(this, [this, res]() {
            emit threadStateChanged(res);
        }, Qt::QueuedConnection);
        return;
    }

    // 2. Request Resource under Mutex Protection
    res.status = QStringLiteral("Requesting Resource");
    QMetaObject::invokeMethod(this, [this, res]() {
        emit threadStateChanged(res);
    }, Qt::QueuedConnection);

    qint64 t0 = QDateTime::currentMSecsSinceEpoch();

    try {
        QString errorMsg;
        // The core critical section: ResourceManager::allocateRunway is protected by std::mutex
        bool success = m_resourceMgr->allocateRunway(req.runwayId, req.flightId, &errorMsg);
        qint64 t1 = QDateTime::currentMSecsSinceEpoch();
        res.durationMs = t1 - t0;
        res.success = success;

        if (success) {
            res.status = QStringLiteral("Allocation Successful");
            res.message = QStringLiteral("Runway %1 reserved successfully for %2.")
                              .arg(req.runwayId).arg(req.flightNumber);
        } else {
            res.status = QStringLiteral("Allocation Rejected");
            res.message = errorMsg.isEmpty() ? QStringLiteral("Runway allocation rejected.") : errorMsg;
        }
    } catch (const std::exception &ex) {
        res.success = false;
        res.status = QStringLiteral("Thread Failed");
        res.message = QStringLiteral("Exception: %1").arg(ex.what());
    } catch (...) {
        res.success = false;
        res.status = QStringLiteral("Thread Failed");
        res.message = QStringLiteral("Unknown exception encountered during allocation.");
    }

    // 3. Record result in thread-safe shared collection
    {
        std::lock_guard<std::mutex> rLock(m_resultsMutex);
        m_results.append(res);
    }

    // 4. Safely dispatch result to GUI thread via Qt queued event delivery
    QMetaObject::invokeMethod(this, [this, res]() {
        emit threadStateChanged(res);
    }, Qt::QueuedConnection);
}

void ConcurrentAllocationDemo::startAsyncDefault(int runwayId, int threadCount)
{
    startAsync(runwayId, createDefaultRequests(runwayId, threadCount));
}

void ConcurrentAllocationDemo::startAsync(int runwayId, const QList<Request> &requests)
{
    if (m_isRunning.load()) {
        cancelAndJoin();
    }

    m_isRunning.store(true);

    if (m_coordinatorThread.joinable()) {
        m_coordinatorThread.join();
    }

    m_coordinatorThread = std::thread([this, runwayId, requests]() {
        DemoReport report = execute(runwayId, requests);
        m_isRunning.store(false);

        QMetaObject::invokeMethod(this, [this, report]() {
            emit demoFinished(report);
        }, Qt::QueuedConnection);
    });
}

void ConcurrentAllocationDemo::cancelAndJoin()
{
    m_abortFlag.store(true);
    m_startCv.notify_all();

    if (m_coordinatorThread.joinable()) {
        m_coordinatorThread.join();
    }

    for (auto &t : m_workerThreads) {
        if (t.joinable()) {
            t.join();
        }
    }
    m_workerThreads.clear();
    m_isRunning.store(false);
}

bool ConcurrentAllocationDemo::isRunning() const
{
    return m_isRunning.load();
}

