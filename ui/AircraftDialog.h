#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>

#include "models/Aircraft.h"

class ApplicationService;

/**
 * @brief Modal dialog for adding and editing aircraft records with role-aware field permissions.
 */
class AircraftDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode {
        Add,
        Edit
    };

    explicit AircraftDialog(ApplicationService *service, Mode mode = Mode::Add, QWidget *parent = nullptr);
    ~AircraftDialog() override = default;

    void setAircraft(const Models::Aircraft &aircraft);
    Models::Aircraft aircraft() const;

private slots:
    void onValidateAndAccept();

private:
    void setupUi();
    void applyRoleRestrictions();

    ApplicationService *m_service    = nullptr;
    Mode                m_mode       = Mode::Add;
    int                 m_aircraftId = 0;

    // Controls
    QLabel      *m_roleNotice     = nullptr;
    QLineEdit   *m_regEdit        = nullptr;
    QLineEdit   *m_typeEdit       = nullptr;
    QSpinBox    *m_capacitySpin   = nullptr;
    QComboBox   *m_statusCombo    = nullptr;
    QLabel      *m_errorLabel     = nullptr;
    QPushButton *m_saveButton     = nullptr;
    QPushButton *m_cancelButton   = nullptr;
};
