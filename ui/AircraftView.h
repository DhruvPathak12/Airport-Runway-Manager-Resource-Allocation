#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>

#include "models/Aircraft.h"

class ApplicationService;

/**
 * @brief GUI view for viewing, filtering, creating, editing, and managing aircraft records.
 */
class AircraftView : public QWidget
{
    Q_OBJECT

public:
    explicit AircraftView(ApplicationService *service, QWidget *parent = nullptr);
    ~AircraftView() override = default;

    /**
     * @brief Updates UI controls and action availability based on session permissions.
     */
    void updatePermissions();

public slots:
    /**
     * @brief Refreshes the aircraft list from MySQL.
     */
    void refreshAircraft();

private slots:
    void onSearchChanged(const QString &text);
    void onStatusFilterChanged(int index);
    void onSelectionChanged();
    void onAddAircraftClicked();
    void onEditAircraftClicked();
    void onDeleteAircraftClicked();
    void onTableDoubleClicked(int row, int column);
    void onQuickStatusChanged(int index);

private:
    void setupUi();
    void renderAircraftTable(const QList<Models::Aircraft> &aircraftList);
    Models::Aircraft* getSelectedAircraft();

    ApplicationService *m_service = nullptr;

    // Controls
    QLineEdit      *m_searchEdit      = nullptr;
    QComboBox      *m_statusFilter    = nullptr;
    QPushButton    *m_refreshButton   = nullptr;
    QPushButton    *m_addButton       = nullptr;
    QPushButton    *m_editButton      = nullptr;
    QPushButton    *m_deleteButton    = nullptr;
    QComboBox      *m_quickStatusCombo = nullptr;
    QLabel         *m_countBadge      = nullptr;
    QLabel         *m_permNotice      = nullptr;

    // Content stack (0=Table, 1=Empty / Error)
    QStackedWidget *m_contentStack    = nullptr;
    QTableWidget   *m_tableWidget     = nullptr;
    QWidget        *m_stateWidget     = nullptr;
    QLabel         *m_stateIcon       = nullptr;
    QLabel         *m_stateTitle      = nullptr;
    QLabel         *m_stateMessage    = nullptr;
    QPushButton    *m_stateButton     = nullptr;

    QList<Models::Aircraft> m_currentAircraft;
};
