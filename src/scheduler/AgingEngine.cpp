#include "scheduler/AgingEngine.h"
#include <QDebug>

namespace SchedulerComponents {

AgingEngine::AgingEngine(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(m_intervalSeconds * 1000);
    connect(m_timer, &QTimer::timeout, this, &AgingEngine::onTimerFired);
}

AgingEngine::~AgingEngine()
{
    stop();
}

bool AgingEngine::setIntervalSeconds(int seconds, QString *outError)
{
    if (seconds < MIN_INTERVAL_SECONDS || seconds > MAX_INTERVAL_SECONDS) {
        if (outError) {
            *outError = QStringLiteral("Aging interval must be between %1 and %2 seconds.")
                            .arg(MIN_INTERVAL_SECONDS)
                            .arg(MAX_INTERVAL_SECONDS);
        }
        return false;
    }

    if (m_intervalSeconds != seconds) {
        m_intervalSeconds = seconds;
        if (m_timer) {
            m_timer->setInterval(m_intervalSeconds * 1000);
        }
        emit intervalChanged(m_intervalSeconds);
    }
    return true;
}

bool AgingEngine::start(QString *outError)
{
    Q_UNUSED(outError);
    if (m_isRunning) {
        return true;
    }

    if (!m_timer) {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &AgingEngine::onTimerFired);
    }

    m_timer->start(m_intervalSeconds * 1000);
    m_isRunning = true;
    emit agingStarted();
    return true;
}

void AgingEngine::stop()
{
    if (!m_isRunning) {
        return;
    }

    if (m_timer) {
        m_timer->stop();
    }
    m_isRunning = false;
    emit agingStopped();
}

int AgingEngine::evaluateAging(ReadyQueue &queue,
                               const QDateTime &evaluationTime,
                               int *outUpdatedCount)
{
    int updated = 0;
    int evaluated = queue.evaluateAging(m_intervalSeconds, evaluationTime, &updated);

    if (outUpdatedCount) {
        *outUpdatedCount = updated;
    }

    emit agingEvaluated(evaluated, updated);
    return evaluated;
}

void AgingEngine::onTimerFired()
{
    emit timeoutTriggered();
}

} // namespace SchedulerComponents

