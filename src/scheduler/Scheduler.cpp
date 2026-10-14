#include "Scheduler.h"

#include <QDebug>

Scheduler::Scheduler(QObject *parent)
    : QObject(parent)
{
}

void Scheduler::runScheduler()
{
    // TODO (Day 11): Implement Non-Preemptive Priority Scheduling.
    // Steps:
    //   1. Query DatabaseManager for all scheduling_queue rows with status='Waiting'.
    //   2. Sort by (priority ASC, arrival_time ASC).
    //   3. Process in order: update status, record completion_time, compute TAT, WT.
    //   4. Emit schedulingComplete() when done.
    qDebug() << "[Scheduler] runScheduler() — stub (implemented Day 11)";
    emit schedulingComplete();
}
