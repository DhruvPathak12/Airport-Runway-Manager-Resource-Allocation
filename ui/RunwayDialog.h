#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>

#include "models/Runway.h"

class ApplicationService;

/**
 * @brief Modal dialog for adding or editing runway resources with role-aware field permissions.
 */
class RunwayDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode {
        Add,
        Edit
    };

    explicit RunwayDialog(ApplicationService *service, Mode mode = Mode::Edit, QWidget *parent = nullptr);
    ~RunwayDialog() override = default;

    void setRunway(const Models::Runway &runway);
    Models::Runway runway() const;

private slots:
    void onValidateAndAccept();

private:
    void setupUi();
    void applyRoleRestrictions();

    ApplicationService *m_service   = nullptr;
    Mode                m_mode      = Mode::Edit;
    int                 m_runwayId  = 0;

    // Controls
    QLabel      *m_roleNotice   = nullptr;
    QLineEdit   *m_numberEdit   = nullptr;
    QComboBox   *m_typeCombo    = nullptr;
    QComboBox   *m_statusCombo  = nullptr;
    QLabel      *m_errorLabel   = nullptr;
    QPushButton *m_saveButton   = nullptr;
    QPushButton *m_cancelButton = nullptr;
};
