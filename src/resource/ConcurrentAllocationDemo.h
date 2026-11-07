#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QMetaType>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <vector>

class ResourceManager;

/**
 * @brief Coordinates concurrent C++ worker threads competing for shared runway resources.
 *
 * ── OS Concept Mapping (Phase 2 / Day 20) ────────────────────────────────────
 *   Multithreading             → Concurrent C++ threads simulating simultaneous flight requests
 *   Shared Resource            → Runway managed by ResourceManager
 *   Critical Section           → Protected allocation path in ResourceManager
 *   Mutual Exclusion           → Central std::mutex ensures at most one flight acquires ownership
 *   Condition Variable         → Start barrier ensuring true simultaneous release of all threads
 *   Thread Lifecycle           → Explicit creation, synchronization, joining, and exception handling
 *   Race Condition Prevention  → Only one thread succeeds; contenders are rejected safely
 */
class ConcurrentAllocationDemo : public QObject
{
    Q_OBJECT

public:
    struct Request {
        int requestId = 0;
        QString threadLabel;       // e.g. "Thread 1 (T1)"
        int flightId = 0;
        QString flightNumber;      // e.g. "AI-101"
        int runwayId = 0;
        QString runwayNumber;      // e.g. "09R"
    };

    struct ThreadResult {
        int requestId = 0;
        QString threadLabel;
        QString systemThreadId;    // std::this_thread::get_id() as string
        int flightId = 0;
        QString flightNumber;
        int runwayId = 0;
        QString status;            // "Waiting to Start", "Requesting Resource", "Critical Section Entered", "Allocation Successful", "Allocation Rejected", "Thread Completed"
        bool success = false;
        QString message;           // Contention error or success message
        qint64 durationMs = 0;
    };

    struct DemoReport {
        int runwayId = 0;
        QString runwayNumber;
        int totalRequests = 0;
        int successfulAllocations = 0;
        int rejectedAllocations = 0;
        int finalOwningFlightId = 0;
        QString finalOwningFlightNumber;
        QList<ThreadResult> results;
        bool hasDataRace = false;
        int maxConcurrentObserved = 0;
        QString summaryMessage;
    };

    explicit ConcurrentAllocationDemo(ResourceManager *resourceManager, QObject *parent = nullptr);
    ~ConcurrentAllocationDemo() override;

    // Non-copyable
    ConcurrentAllocationDemo(const ConcurrentAllocationDemo &) = delete;
    ConcurrentAllocationDemo &operator=(const ConcurrentAllocationDemo &) = delete;

    /**
     * @brief Executes a synchronous demonstration of concurrent runway allocation.
     * Launches worker threads, coordinates their start via a condition variable barrier,
     * joins all threads before returning, and compiles a DemoReport.
     */
    DemoReport execute(int runwayId, const QList<Request> &requests);

    /**
     * @brief Convenience overload executing a default contention scenario with the specified thread count.
     */
    DemoReport executeDefaultScenario(int runwayId, int threadCount = 3);

    /**
     * @brief Launches the demonstration asynchronously without blocking the caller (e.g. GUI thread).
     * Delivers progressive status and final DemoReport via queued signals.
     */
    void startAsync(int runwayId, const QList<Request> &requests);
    void startAsyncDefault(int runwayId, int threadCount = 3);

    /**
     * @brief Safely cancels any ongoing asynchronous execution and joins all threads.
     */
    void cancelAndJoin();

    /**
     * @brief Returns true if an asynchronous demonstration is currently running.
     */
    bool isRunning() const;

    /**
     * @brief Static helper generating default benchmark requests for a runway.
     */
    static QList<Request> createDefaultRequests(int runwayId, int count = 3);

signals:
    /**
     * @brief Emitted when a demonstration run commences.
     */
    void demoStarted(int runwayId, int totalThreads);

    /**
     * @brief Emitted when an individual worker thread's state updates.
     */
    void threadStateChanged(const ConcurrentAllocationDemo::ThreadResult &result);

    /**
     * @brief Emitted when all worker threads have completed and joined.
     */
    void demoFinished(const ConcurrentAllocationDemo::DemoReport &report);

private:
    void workerRoutine(const Request &req);

    ResourceManager *m_resourceMgr = nullptr;

    // Condition variable start barrier
    std::mutex              m_startMutex;
    std::condition_variable m_startCv;
    std::atomic<int>        m_readyCount{0};
    std::atomic<bool>       m_startFlag{false};
    std::atomic<bool>       m_abortFlag{false};

    // Shared results collection protected by results mutex
    std::mutex              m_resultsMutex;
    QList<ThreadResult>     m_results;

    // Worker threads
    std::vector<std::thread> m_workerThreads;

    // Coordinator thread for non-blocking asynchronous execution
    std::thread             m_coordinatorThread;
    std::atomic<bool>       m_isRunning{false};
};

Q_DECLARE_METATYPE(ConcurrentAllocationDemo::Request)
Q_DECLARE_METATYPE(ConcurrentAllocationDemo::ThreadResult)
Q_DECLARE_METATYPE(ConcurrentAllocationDemo::DemoReport)

