#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>

#include "models/Runway.h"

class ApplicationService;

/**
 * @brief GUI view for viewing, filtering, and managing runway shared resources and maintenance.
 */
class RunwaysView : public QWidget
{
    Q_OBJECT

public:
    explicit RunwaysView(ApplicationService *service, QWidget *parent = nullptr);
    ~RunwaysView() override = default;

    /**
     * @brief Updates UI controls and action availability based on session permissions.
     */
    void updatePermissions();

public slots:
    /**
     * @brief Refreshes the runway list from MySQL.
     */
    void refreshRunways();

private slots:
    void onSearchChanged(const QString &text);
    void onStatusFilterChanged(int index);
    void onSelectionChanged();
    void onToggleMaintenanceClicked();
    void onAddRunwayClicked();
    void onEditRunwayClicked();
    void onTableDoubleClicked(int row, int column);

private:
    void setupUi();
    void renderRunwayTable(const QList<Models::Runway> &runwayList);
    Models::Runway* getSelectedRunway();

    ApplicationService *m_service = nullptr;

    // Controls
    QLineEdit      *m_searchEdit           = nullptr;
    QComboBox      *m_statusFilter         = nullptr;
    QPushButton    *m_refreshButton        = nullptr;
    QPushButton    *m_toggleMaintenanceBtn = nullptr;
    QPushButton    *m_addButton            = nullptr;
    QPushButton    *m_editButton           = nullptr;
    QLabel         *m_countBadge           = nullptr;
    QLabel         *m_permNotice           = nullptr;

    // Content stack (0=Table, 1=Empty / Error)
    QStackedWidget *m_contentStack         = nullptr;
    QTableWidget   *m_tableWidget          = nullptr;
    QWidget        *m_stateWidget          = nullptr;
    QLabel         *m_stateIcon            = nullptr;
    QLabel         *m_stateTitle           = nullptr;
    QLabel         *m_stateMessage         = nullptr;
    QPushButton    *m_stateButton          = nullptr;

    QList<Models::Runway> m_currentRunways;
};
