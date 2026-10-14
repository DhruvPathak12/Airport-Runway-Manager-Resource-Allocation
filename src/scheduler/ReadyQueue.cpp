#include "scheduler/ReadyQueue.h"

namespace SchedulerComponents {

bool ReadyQueue::enqueue(const Models::Process &process, QString *outError)
{
    // 1. Validate process data
    QString valErr;
    if (!process.validate(&valErr)) {
        if (outError) *outError = valErr;
        return false;
    }

    // 2. Validate process state (only Ready processes can enter ready queue)
    if (process.state != Models::ProcessState::Ready) {
        if (outError) {
            *outError = QStringLiteral("Cannot enqueue process for flight %1: process state is '%2', but must be 'Ready' to enter the ready queue.")
                            .arg(process.flightId)
                            .arg(Models::Process::stateToString(process.state));
        }
        return false;
    }

    // 3. Prevent duplicates by simulated PID (flightId)
    if (containsFlight(process.flightId)) {
        if (outError) {
            *outError = QStringLiteral("Cannot enqueue duplicate process: flight %1 (PID: %1) is already in the ready queue.")
                            .arg(process.flightId);
        }
        return false;
    }

    // 4. Prevent duplicates by persistent queueId if set
    if (process.queueId > 0 && containsQueueId(process.queueId)) {
        if (outError) {
            *outError = QStringLiteral("Cannot enqueue duplicate process: queue ID %1 is already in the ready queue.")
                            .arg(process.queueId);
        }
        return false;
    }

    // 5. Append to back in strict FIFO order
    m_queue.append(process);
    return true;
}

bool ReadyQueue::dequeue(Models::Process *outProcess, QString *outError)
{
    if (m_queue.isEmpty()) {
        if (outError) *outError = QStringLiteral("Ready queue is empty.");
        return false;
    }

    Models::Process front = m_queue.takeFirst();
    if (outProcess) {
        *outProcess = front;
    }
    return true;
}

bool ReadyQueue::removeByFlightId(int flightId, Models::Process *outRemoved, QString *outError)
{
    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue[i].flightId == flightId) {
            Models::Process removed = m_queue.takeAt(i);
            if (outRemoved) {
                *outRemoved = removed;
            }
            return true;
        }
    }
    if (outError) {
        *outError = QStringLiteral("Process for flight ID %1 not found in ready queue.").arg(flightId);
    }
    return false;
}

bool ReadyQueue::removeByQueueId(int queueId, Models::Process *outRemoved, QString *outError)
{
    if (queueId <= 0) {
        if (outError) *outError = QStringLiteral("Invalid queue ID: must be greater than zero.");
        return false;
    }

    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue[i].queueId == queueId || m_queue[i].id == queueId) {
            Models::Process removed = m_queue.takeAt(i);
            if (outRemoved) {
                *outRemoved = removed;
            }
            return true;
        }
    }
    if (outError) {
        *outError = QStringLiteral("Process with queue ID %1 not found in ready queue.").arg(queueId);
    }
    return false;
}

bool ReadyQueue::containsFlight(int flightId) const
{
    return findByFlightId(flightId) != nullptr;
}

bool ReadyQueue::containsQueueId(int queueId) const
{
    return findByQueueId(queueId) != nullptr;
}

const Models::Process* ReadyQueue::findByFlightId(int flightId) const
{
    for (const auto &proc : m_queue) {
        if (proc.flightId == flightId) {
            return &proc;
        }
    }
    return nullptr;
}

Models::Process* ReadyQueue::findByFlightId(int flightId)
{
    for (auto &proc : m_queue) {
        if (proc.flightId == flightId) {
            return &proc;
        }
    }
    return nullptr;
}

const Models::Process* ReadyQueue::findByQueueId(int queueId) const
{
    if (queueId <= 0) {
        return nullptr;
    }

    for (const auto &proc : m_queue) {
        if (proc.queueId == queueId || proc.id == queueId) {
            return &proc;
        }
    }
    return nullptr;
}

Models::Process* ReadyQueue::findByQueueId(int queueId)
{
    if (queueId <= 0) {
        return nullptr;
    }

    for (auto &proc : m_queue) {
        if (proc.queueId == queueId || proc.id == queueId) {
            return &proc;
        }
    }
    return nullptr;
}

const Models::Process* ReadyQueue::peek() const
{
    if (m_queue.isEmpty()) {
        return nullptr;
    }
    return &m_queue.first();
}

bool ReadyQueue::transitionProcessState(int flightId,
                                        Models::ProcessState newState,
                                        Models::Process *outProcess,
                                        QString *outError)
{
    int index = -1;
    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue[i].flightId == flightId) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        if (outError) {
            *outError = QStringLiteral("Process for flight %1 not found in ready queue.").arg(flightId);
        }
        return false;
    }

    if (!m_queue[index].canTransitionTo(newState, outError)) {
        return false;
    }

    m_queue[index].transitionTo(newState);

    if (newState != Models::ProcessState::Ready) {
        // Process is no longer ready; remove from queue to maintain state consistency
        Models::Process removed = m_queue.takeAt(index);
        if (outProcess) {
            *outProcess = removed;
        }
    } else {
        if (outProcess) {
            *outProcess = m_queue[index];
        }
    }

    return true;
}

} // namespace SchedulerComponents
