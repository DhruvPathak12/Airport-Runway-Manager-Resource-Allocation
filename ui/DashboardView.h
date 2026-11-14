#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QProgressBar>
#include <QTimer>

#include "services/ApplicationService.h"

/**
 * @brief System Dashboard and Operational Monitoring View (Day 24).
 *
 * Provides a comprehensive, real-time, data-driven operational overview of:
 *   - Flight activity & emergency monitoring
 *   - OS process scheduling & ready queue depth
 *   - Runway resource utilization & allocation status
 *   - MySQL transactions & immutable operation audit logs
 *   - High-level KPIs and performance metrics
 */
class DashboardView : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardView(ApplicationService *service, QWidget *parent = nullptr);
    ~DashboardView() override;

public slots:
    void refreshDashboard();

signals:
    void navigateToFlights();
    void navigateToRunways();
    void navigateToScheduling();

private slots:
    void onClockTick();

private:
    void setupUi();
    QWidget* createHeaderSection();
    QWidget* createKpiSection();
    QWidget* createEmergencySection();
    QWidget* createMiddleSection();
    QWidget* createBottomSection();

    QWidget* createKpiCard(const QString &icon, const QString &title, QLabel *&valueLabel,
                           const QString &accentColor, const QString &subtitle = QString());

    ApplicationService *m_service = nullptr;
    QTimer             *m_refreshTimer = nullptr;

    // Header Widgets
    QLabel      *m_clockLabel = nullptr;
    QLabel      *m_dbBadge = nullptr;
    QLabel      *m_userBadge = nullptr;
    QPushButton *m_refreshButton = nullptr;

    // First Row KPI Labels
    QLabel *m_lblTotalFlights = nullptr;
    QLabel *m_lblScheduledFlights = nullptr;
    QLabel *m_lblActiveFlights = nullptr;
    QLabel *m_lblEmergencyFlights = nullptr;
    QLabel *m_lblTotalRunways = nullptr;

    // Second Row KPI Labels
    QLabel *m_lblAvailableRunways = nullptr;
    QLabel *m_lblOccupiedRunways = nullptr;
    QLabel *m_lblMaintenanceRunways = nullptr;
    QLabel *m_lblRunwayUtilization = nullptr;
    QLabel *m_lblReadyQueue = nullptr;
    QLabel *m_lblRunningProcesses = nullptr;
    QLabel *m_lblCompletedProcesses = nullptr;

    // Emergency Section
    QWidget      *m_emergencyCard = nullptr;
    QLabel       *m_emergencyBanner = nullptr;
    QTableWidget *m_emergencyTable = nullptr;

    // Runway Section
    QProgressBar *m_runwayUtilProgress = nullptr;
    QLabel       *m_runwayUtilLabel = nullptr;
    QTableWidget *m_runwayTable = nullptr;
    QPushButton  *m_btnNavRunways = nullptr;

    // Scheduling Section
    QLabel       *m_lblSchedPolicy = nullptr;
    QLabel       *m_lblAgingStatus = nullptr;
    QLabel       *m_lblAvgWaitTime = nullptr;
    QLabel       *m_lblAvgTurnaroundTime = nullptr;
    QLabel       *m_lblNextCandidate = nullptr;
    QPushButton  *m_btnNavScheduling = nullptr;

    // Flights Activity Table
    QTableWidget *m_flightsTable = nullptr;
    QPushButton  *m_btnNavFlights = nullptr;

    // Transactions & Logs
    QLabel       *m_lastTxOperation = nullptr;
    QLabel       *m_lastTxStatus = nullptr;
    QLabel       *m_lastTxDetails = nullptr;
    QLabel       *m_lastTxTime = nullptr;
    QTableWidget *m_logsTable = nullptr;
};

