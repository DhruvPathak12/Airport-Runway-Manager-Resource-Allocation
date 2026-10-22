#pragma once

#include <QDialog>
#include <QComboBox>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>

#include "models/Runway.h"
#include "models/Flight.h"

class ApplicationService;

/**
 * @brief Modal dialog for allocating an available runway to an eligible flight.
 *
 * Implements Day 15 Runway Resource Allocation GUI workflow:
 * - Selects available runway (or pre-selects context runway).
 * - Queries and lists eligible non-terminal flights via ApplicationService.
 * - Enforces operational compatibility (runway type vs. flight type).
 * - Executes parameterized allocation transaction via ApplicationService.
 */
class RunwayAllocationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RunwayAllocationDialog(ApplicationService *service, int preselectedRunwayId = 0, QWidget *parent = nullptr);
    ~RunwayAllocationDialog() override = default;

    int selectedRunwayId() const;
    int selectedFlightId() const;

private slots:
    void onRunwayChanged(int index);
    void onFlightSelectionChanged();
    void onAllocateClicked();

private:
    void setupUi();
    void loadAvailableRunways();
    void loadEligibleFlights();

    ApplicationService *m_service = nullptr;
    int                 m_initialRunwayId = 0;

    // Controls
    QComboBox      *m_runwayCombo       = nullptr;
    QLabel         *m_runwayDetails     = nullptr;
    QTableWidget   *m_flightsTable      = nullptr;
    QLabel         *m_emptyNotice       = nullptr;
    QLabel         *m_errorLabel        = nullptr;
    QPushButton    *m_allocateButton    = nullptr;
    QPushButton    *m_cancelButton      = nullptr;

    QList<Models::Runway> m_availableRunways;
    QList<Models::Flight> m_eligibleFlights;
};
