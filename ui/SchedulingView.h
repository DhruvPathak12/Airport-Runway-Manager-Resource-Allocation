#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QStackedWidget>

#include "models/Process.h"

class ApplicationService;

/**
 * @brief GUI view for monitoring the OS Priority Scheduler, Ready Queue, and Priority Aging.
 * Displays real-time process waiting times, base/effective priorities, aging levels,
 * emergency statuses, and provides controls for configuring and running Priority Aging.
 */
class SchedulingView : public QWidget
{
    Q_OBJECT

public:
    explicit SchedulingView(ApplicationService *service, QWidget *parent = nullptr);
    ~SchedulingView() override;

    /**
     * @brief Updates control availability and action permissions based on the active session role.
     */
    void updatePermissions();

public slots:
    /**
     * @brief Refreshes the ready queue and scheduling processes view.
     */
    void refreshView();

private slots:
    void onStartAgingClicked();
    void onStopAgingClicked();
    void onIntervalChanged(int seconds);
    void onEvaluateAgingClicked();
    void onDispatchClicked();
    void onClockTick();

private:
    void setupUi();
    void renderProcessesTable(const QList<Models::Process> &processes);
    void updateStatusBanner();

    ApplicationService *m_service = nullptr;

    // UI controls
    QLabel         *m_statusBadge         = nullptr;
    QSpinBox       *m_intervalSpin        = nullptr;
    QPushButton    *m_startAgingButton    = nullptr;
    QPushButton    *m_stopAgingButton     = nullptr;
    QPushButton    *m_evalAgingButton     = nullptr;
    QPushButton    *m_dispatchButton      = nullptr;
    QPushButton    *m_refreshButton       = nullptr;
    QLabel         *m_runningBanner       = nullptr;
    QLabel         *m_permNotice          = nullptr;

    // Table
    QTableWidget   *m_tableWidget         = nullptr;
    QTimer         *m_clockTimer          = nullptr;

    QList<Models::Process> m_cachedProcesses;
};

