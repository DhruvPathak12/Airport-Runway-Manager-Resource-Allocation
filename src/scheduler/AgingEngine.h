#pragma once

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QString>

#include "scheduler/ReadyQueue.h"

namespace SchedulerComponents {

/**
 * @brief AgingEngine provides real-time wall-clock aging timer and evaluation.
 *
 * ── OS Concept Mapping ────────────────────────────────────────────────────────
 *   In Priority Scheduling, low priority processes can suffer starvation if higher
 *   priority processes continuously arrive. Priority Aging prevents starvation by
 *   gradually improving the effective priority of processes that have been waiting
 *   in the READY queue for configurable intervals.
 *
 * ── Real-Time Wall Clock & Timer Design ───────────────────────────────────────
 *   - Centralized QTimer triggers periodic aging evaluation.
 *   - Idempotent evaluation: Aging is calculated strictly from elapsed continuous
 *     waiting time (readySince.secsTo(now)), avoiding drift or duplicate aging
 *     if timer ticks late or restarts.
 *   - Configurable interval with default of 30 seconds.
 *   - Safe start/stop lifecycle management without dangling callbacks.
 */
class AgingEngine : public QObject
{
    Q_OBJECT

public:
    static constexpr int DEFAULT_INTERVAL_SECONDS = 30;
    static constexpr int MIN_INTERVAL_SECONDS = 1;
    static constexpr int MAX_INTERVAL_SECONDS = 86400; // 24 hours

    explicit AgingEngine(QObject *parent = nullptr);
    ~AgingEngine() override;

    int intervalSeconds() const { return m_intervalSeconds; }
    bool setIntervalSeconds(int seconds, QString *outError = nullptr);

    bool isRunning() const { return m_isRunning; }
    bool start(QString *outError = nullptr);
    void stop();

    /**
     * @brief Evaluates aging across all eligible READY processes in the given queue.
     * Updates agingLevel, effectivePriority, and reorders the queue if any priority changes.
     *
     * @param queue Reference to in-memory ReadyQueue.
     * @param evaluationTime Injected timestamp for deterministic testing, or invalid for wall clock.
     * @param outUpdatedCount Optional count of processes whose effective priority changed.
     * @return Number of processes evaluated in READY state.
     */
    int evaluateAging(ReadyQueue &queue,
                      const QDateTime &evaluationTime = QDateTime(),
                      int *outUpdatedCount = nullptr);

signals:
    void agingStarted();
    void agingStopped();
    void intervalChanged(int newIntervalSeconds);
    void agingEvaluated(int eligibleCount, int updatedCount);
    void timeoutTriggered();

private slots:
    void onTimerFired();

private:
    int      m_intervalSeconds = DEFAULT_INTERVAL_SECONDS;
    bool     m_isRunning       = false;
    QTimer  *m_timer           = nullptr;
};

} // namespace SchedulerComponents

