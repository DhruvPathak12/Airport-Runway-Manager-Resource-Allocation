#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>

#include "models/Flight.h"

class ApplicationService;

/**
 * @brief GUI view for viewing, filtering, creating, editing, and deleting flights.
 */
class FlightsView : public QWidget
{
    Q_OBJECT

public:
    explicit FlightsView(ApplicationService *service, QWidget *parent = nullptr);
    ~FlightsView() override = default;

    /**
     * @brief Updates UI controls and action availability based on current session permissions.
     */
    void updatePermissions();

public slots:
    /**
     * @brief Refreshes the flight list from the MySQL database.
     */
    void refreshFlights();

private slots:
    void onSearchChanged(const QString &text);
    void onStatusFilterChanged(int index);
    void onSelectionChanged();
    void onAddFlightClicked();
    void onEditFlightClicked();
    void onDeleteFlightClicked();
    void onTableDoubleClicked(int row, int column);

private:
    void setupUi();
    void renderFlightsTable(const QList<Models::Flight> &flights);
    Models::Flight* getSelectedFlight();

    ApplicationService *m_service = nullptr;

    // UI controls
    QLineEdit      *m_searchEdit     = nullptr;
    QComboBox      *m_statusFilter   = nullptr;
    QPushButton    *m_refreshButton  = nullptr;
    QPushButton    *m_addButton      = nullptr;
    QPushButton    *m_editButton     = nullptr;
    QPushButton    *m_deleteButton   = nullptr;
    QLabel         *m_countBadge     = nullptr;
    QLabel         *m_permNotice     = nullptr;

    // View stack: 0 = Table, 1 = Empty / Offline / Error message
    QStackedWidget *m_contentStack   = nullptr;
    QTableWidget   *m_tableWidget    = nullptr;
    QWidget        *m_stateWidget    = nullptr;
    QLabel         *m_stateIcon      = nullptr;
    QLabel         *m_stateTitle     = nullptr;
    QLabel         *m_stateMessage   = nullptr;
    QPushButton    *m_stateButton    = nullptr;

    QList<Models::Flight> m_currentFlights;
};
