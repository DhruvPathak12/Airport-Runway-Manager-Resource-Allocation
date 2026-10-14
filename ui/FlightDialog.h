#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>

#include "models/Flight.h"

class ApplicationService;

/**
 * @brief Modal dialog for creating and editing flight records with client-side validation.
 */
class FlightDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode {
        Add,
        Edit
    };

    explicit FlightDialog(ApplicationService *service, Mode mode = Mode::Add, QWidget *parent = nullptr);
    ~FlightDialog() override = default;

    void setFlight(const Models::Flight &flight);
    Models::Flight flight() const;

private slots:
    void onValidateAndAccept();

private:
    void setupUi();
    void populateAircraftList();

    ApplicationService *m_service = nullptr;
    Mode                m_mode    = Mode::Add;
    int                 m_flightId = 0;
    QDateTime           m_createdAt;

    // Form inputs
    QLineEdit      *m_flightNumEdit    = nullptr;
    QLineEdit      *m_airlineEdit      = nullptr;
    QComboBox      *m_aircraftCombo    = nullptr;
    QLineEdit      *m_sourceEdit       = nullptr;
    QLineEdit      *m_destinationEdit  = nullptr;
    QComboBox      *m_flightTypeCombo  = nullptr;
    QDateTimeEdit  *m_scheduledTimeEdit = nullptr;
    QSpinBox       *m_prioritySpin     = nullptr;
    QCheckBox      *m_emergencyCheck   = nullptr;
    QComboBox      *m_statusCombo      = nullptr;
    QLabel         *m_errorLabel       = nullptr;
    QPushButton    *m_saveButton       = nullptr;
    QPushButton    *m_cancelButton     = nullptr;
};
