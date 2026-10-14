#include "models/Process.h"

namespace Models {

Process::Process()
    : arrivalTime(QDateTime::currentDateTime())
{
}

Process::Process(int qid, int fid, int priority, int burstSec, ProcessState st)
    : id(qid)
    , queueId(qid)
    , flightId(fid)
    , arrivalTime(QDateTime::currentDateTime())
    , basePriority(priority)
    , effectivePriority(priority)
    , serviceTime(burstSec)
    , state(st)
{
}

bool Process::validate(QString *outError) const
{
    if (flightId <= 0) {
        if (outError) *outError = QStringLiteral("Associated flight ID is required and must be greater than zero.");
        return false;
    }

    if (basePriority < 1 || basePriority > 5) {
        if (outError) *outError = QStringLiteral("Base priority must be between 1 (highest) and 5 (lowest).");
        return false;
    }

    if (effectivePriority < 1 || effectivePriority > 5) {
        if (outError) *outError = QStringLiteral("Effective priority must be between 1 (highest) and 5 (lowest).");
        return false;
    }

    if (serviceTime <= 0) {
        if (outError) *outError = QStringLiteral("Service time (burst time) must be greater than zero seconds.");
        return false;
    }

    if (serviceTime > 65535) {
        if (outError) *outError = QStringLiteral("Service time cannot exceed 65535 seconds.");
        return false;
    }

    if (!arrivalTime.isValid()) {
        if (outError) *outError = QStringLiteral("Arrival time must be a valid date and time.");
        return false;
    }

    if (waitingTime != -1 && (waitingTime < 0 || waitingTime > 65535)) {
        if (outError) *outError = QStringLiteral("Waiting time must be between 0 and 65535 seconds.");
        return false;
    }

    if (turnaroundTime != -1 && (turnaroundTime < 0 || turnaroundTime > 65535)) {
        if (outError) *outError = QStringLiteral("Turnaround time must be between 0 and 65535 seconds.");
        return false;
    }

    return true;
}

bool Process::calculateMetrics(const QDateTime &completedAt, QString *outError)
{
    if (!completedAt.isValid()) {
        if (outError) *outError = QStringLiteral("Completion time must be a valid date and time.");
        return false;
    }

    if (!arrivalTime.isValid()) {
        if (outError) *outError = QStringLiteral("Arrival time is invalid; cannot compute metrics.");
        return false;
    }

    if (completedAt < arrivalTime) {
        if (outError) {
            *outError = QStringLiteral("Completion time (%1) cannot precede arrival time (%2).")
                            .arg(completedAt.toString(Qt::ISODate))
                            .arg(arrivalTime.toString(Qt::ISODate));
        }
        return false;
    }

    qint64 tatSec = arrivalTime.secsTo(completedAt);
    if (tatSec < serviceTime) {
        if (outError) {
            *outError = QStringLiteral("Turnaround time (%1s) cannot be less than service burst duration (%2s).")
                            .arg(tatSec)
                            .arg(serviceTime);
        }
        return false;
    }

    if (tatSec > 65535) {
        if (outError) *outError = QStringLiteral("Calculated turnaround time exceeds 65535 seconds limit.");
        return false;
    }

    qint64 wtSec = tatSec - serviceTime;
    if (wtSec < 0 || wtSec > 65535) {
        if (outError) *outError = QStringLiteral("Calculated waiting time is out of valid range (0-65535).");
        return false;
    }

    completionTime = completedAt;
    turnaroundTime = static_cast<int>(tatSec);
    waitingTime = static_cast<int>(wtSec);
    return true;
}

bool Process::calculateMetricsFromStart(const QDateTime &startedAt, QString *outError)
{
    if (!startedAt.isValid()) {
        if (outError) *outError = QStringLiteral("Start time must be a valid date and time.");
        return false;
    }

    if (!arrivalTime.isValid()) {
        if (outError) *outError = QStringLiteral("Arrival time is invalid; cannot compute metrics.");
        return false;
    }

    if (startedAt < arrivalTime) {
        if (outError) {
            *outError = QStringLiteral("Execution start time (%1) cannot precede arrival time (%2).")
                            .arg(startedAt.toString(Qt::ISODate))
                            .arg(arrivalTime.toString(Qt::ISODate));
        }
        return false;
    }

    QDateTime compAt = startedAt.addSecs(serviceTime);
    return calculateMetrics(compAt, outError);
}


bool Process::canTransitionTo(ProcessState targetState, QString *outReason) const
{
    if (state == targetState) {
        if (outReason) {
            *outReason = QStringLiteral("Cannot transition to the same state ('%1').")
                             .arg(stateToString(state));
        }
        return false;
    }

    switch (state) {
    case ProcessState::New:
        if (targetState == ProcessState::Ready) {
            return true;
        }
        break;

    case ProcessState::Ready:
        if (targetState == ProcessState::Running) {
            return true;
        }
        break;

    case ProcessState::Running:
        if (targetState == ProcessState::Completed || targetState == ProcessState::Blocked) {
            return true;
        }
        break;

    case ProcessState::Blocked:
        if (targetState == ProcessState::Ready) {
            return true;
        }
        break;

    case ProcessState::Completed:
        // Terminal state: no transitions permitted
        break;
    }

    if (outReason) {
        *outReason = QStringLiteral("Invalid state transition from '%1' to '%2'.")
                         .arg(stateToString(state))
                         .arg(stateToString(targetState));
    }
    return false;
}

bool Process::transitionTo(ProcessState targetState, QString *outError)
{
    if (!canTransitionTo(targetState, outError)) {
        return false;
    }

    state = targetState;
    return true;
}

QString Process::stateToString(ProcessState st)
{
    switch (st) {
    case ProcessState::New:       return QStringLiteral("New");
    case ProcessState::Ready:     return QStringLiteral("Ready");
    case ProcessState::Running:   return QStringLiteral("Running");
    case ProcessState::Blocked:   return QStringLiteral("Blocked");
    case ProcessState::Completed: return QStringLiteral("Completed");
    }
    return QStringLiteral("Ready");
}

QString Process::stateToDbString(ProcessState st)
{
    switch (st) {
    case ProcessState::New:       return QStringLiteral("Ready");
    case ProcessState::Ready:     return QStringLiteral("Ready");
    case ProcessState::Running:   return QStringLiteral("Running");
    case ProcessState::Blocked:   return QStringLiteral("Blocked");
    case ProcessState::Completed: return QStringLiteral("Completed");
    }
    return QStringLiteral("Ready");
}

ProcessState Process::stateFromString(const QString &str, bool *ok)
{
    QString trimmed = str.trimmed();
    if (ok) *ok = true;

    if (trimmed.compare(QStringLiteral("New"), Qt::CaseInsensitive) == 0) {
        return ProcessState::New;
    }
    if (trimmed.compare(QStringLiteral("Ready"), Qt::CaseInsensitive) == 0) {
        return ProcessState::Ready;
    }
    if (trimmed.compare(QStringLiteral("Running"), Qt::CaseInsensitive) == 0) {
        return ProcessState::Running;
    }
    if (trimmed.compare(QStringLiteral("Blocked"), Qt::CaseInsensitive) == 0) {
        return ProcessState::Blocked;
    }
    if (trimmed.compare(QStringLiteral("Completed"), Qt::CaseInsensitive) == 0) {
        return ProcessState::Completed;
    }

    if (ok) *ok = false;
    return ProcessState::Ready;
}

bool Process::isFlightEligible(const Flight &flight, QString *outReason)
{
    if (flight.id <= 0) {
        if (outReason) *outReason = QStringLiteral("Flight has no persistent database ID.");
        return false;
    }

    if (flight.priority < 1 || flight.priority > 5) {
        if (outReason) *outReason = QStringLiteral("Flight priority (%1) is outside valid range (1-5).").arg(flight.priority);
        return false;
    }

    QString status = flight.operationalStatus.trimmed();
    if (status.compare(QStringLiteral("Landed"), Qt::CaseInsensitive) == 0) {
        if (outReason) *outReason = QStringLiteral("Flight %1 has already landed (terminal status) and cannot enter the scheduling queue.")
                                         .arg(flight.flightNumber.isEmpty() ? QString::number(flight.id) : flight.flightNumber);
        return false;
    }
    if (status.compare(QStringLiteral("Departed"), Qt::CaseInsensitive) == 0) {
        if (outReason) *outReason = QStringLiteral("Flight %1 has already departed (terminal status) and cannot enter the scheduling queue.")
                                         .arg(flight.flightNumber.isEmpty() ? QString::number(flight.id) : flight.flightNumber);
        return false;
    }
    if (status.compare(QStringLiteral("Cancelled"), Qt::CaseInsensitive) == 0) {
        if (outReason) *outReason = QStringLiteral("Flight %1 is cancelled and cannot enter the scheduling queue.")
                                         .arg(flight.flightNumber.isEmpty() ? QString::number(flight.id) : flight.flightNumber);
        return false;
    }

    static const QStringList eligibleStatuses = {
        QStringLiteral("Scheduled"),
        QStringLiteral("Taxiing"),
        QStringLiteral("Cleared")
    };

    for (const QString &valid : eligibleStatuses) {
        if (status.compare(valid, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    if (outReason) {
        *outReason = QStringLiteral("Flight status '%1' is not eligible for scheduling queue entry.").arg(flight.operationalStatus);
    }
    return false;
}

Process Process::fromFlight(const Flight &flight, int serviceTimeSeconds)
{
    Process proc;
    proc.flightId = flight.id;
    proc.basePriority = flight.priority;
    proc.effectivePriority = flight.priority; // equal initially; dynamically adjusted by aging in Phase 2
    proc.serviceTime = (serviceTimeSeconds > 0) ? serviceTimeSeconds : 60;
    proc.arrivalTime = QDateTime::currentDateTime();
    proc.state = ProcessState::Ready;

    // Presentation metadata
    proc.flightNumber = flight.flightNumber;
    proc.airline = flight.airline;
    proc.operationalStatus = flight.operationalStatus;

    return proc;
}

} // namespace Models
