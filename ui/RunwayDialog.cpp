#include "RunwayDialog.h"

#include "services/ApplicationService.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

RunwayDialog::RunwayDialog(ApplicationService *service, Mode mode, QWidget *parent)
    : QDialog(parent)
    , m_service(service)
    , m_mode(mode)
{
    setWindowTitle(mode == Mode::Add ? QStringLiteral("Add Runway Resource") : QStringLiteral("Edit Runway Resource"));
    setMinimumWidth(460);
    setStyleSheet(R"(
        QDialog {
            background-color: #ffffff;
        }
        QLabel {
            color: #1e293b;
            font-size: 13px;
        }
        QLineEdit, QComboBox {
            background-color: #f8fafc;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 13px;
            color: #0f172a;
        }
        QLineEdit:focus, QComboBox:focus {
            border: 1px solid #2563eb;
            background-color: #ffffff;
        }
        QLineEdit:disabled, QComboBox:disabled {
            background-color: #f1f5f9;
            color: #64748b;
        }
    )");

    setupUi();
    applyRoleRestrictions();
}

void RunwayDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(16);

    // Role notice banner
    m_roleNotice = new QLabel(this);
    m_roleNotice->hide();
    mainLayout->addWidget(m_roleNotice);

    // Form inputs
    QGroupBox *formGroup = new QGroupBox(m_mode == Mode::Add ? QStringLiteral("Runway Parameters") : QStringLiteral("Runway Resource Details"), this);
    formGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #334155; font-size: 14px; border: 1px solid #e2e8f0; border-radius: 8px; padding-top: 14px; margin-top: 6px; }");
    QFormLayout *formLayout = new QFormLayout(formGroup);
    formLayout->setContentsMargins(16, 20, 16, 16);
    formLayout->setSpacing(14);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Runway Number / Designation
    m_numberEdit = new QLineEdit(this);
    m_numberEdit->setPlaceholderText(QStringLiteral("e.g. 09L, 27R, 14"));
    m_numberEdit->setMaxLength(10);
    formLayout->addRow(QStringLiteral("Designation *:"), m_numberEdit);

    // Runway Type
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItems({
        QStringLiteral("Both"),
        QStringLiteral("Landing"),
        QStringLiteral("Takeoff")
    });
    formLayout->addRow(QStringLiteral("Runway Type:"), m_typeCombo);

    // Operational Status
    m_statusCombo = new QComboBox(this);
    m_statusCombo->addItems({
        QStringLiteral("Available"),
        QStringLiteral("Maintenance"),
        QStringLiteral("Occupied")
    });
    formLayout->addRow(QStringLiteral("Current Status:"), m_statusCombo);

    mainLayout->addWidget(formGroup);

    // Error label
    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet("color: #dc2626; font-size: 12px; font-weight: 500;");
    m_errorLabel->setWordWrap(true);
    m_errorLabel->hide();
    mainLayout->addWidget(m_errorLabel);

    // Button Row
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();

    m_cancelButton = new QPushButton(QStringLiteral("Cancel"), this);
    m_cancelButton->setStyleSheet(R"(
        QPushButton {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 8px 18px;
            font-size: 13px;
            font-weight: 500;
            color: #475569;
        }
        QPushButton:hover {
            background-color: #f1f5f9;
        }
    )");
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_cancelButton);

    m_saveButton = new QPushButton(m_mode == Mode::Add ? QStringLiteral("Create Runway") : QStringLiteral("Save Changes"), this);
    m_saveButton->setStyleSheet(R"(
        QPushButton {
            background-color: #2563eb;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 20px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #1d4ed8;
        }
        QPushButton:disabled {
            background-color: #94a3b8;
        }
    )");
    connect(m_saveButton, &QPushButton::clicked, this, &RunwayDialog::onValidateAndAccept);
    buttonLayout->addWidget(m_saveButton);

    mainLayout->addLayout(buttonLayout);
}

void RunwayDialog::applyRoleRestrictions()
{
    if (!m_service || !m_service->isAuthenticated()) {
        m_numberEdit->setEnabled(false);
        m_typeCombo->setEnabled(false);
        m_statusCombo->setEnabled(false);
        m_saveButton->setEnabled(false);
        return;
    }

    const Session &session = m_service->currentSession();

    if (session.isAdmin()) {
        m_numberEdit->setEnabled(true);
        m_typeCombo->setEnabled(true);
        m_statusCombo->setEnabled(true);
        m_roleNotice->setText(QStringLiteral("👑 Administrator Mode: Full runway resource management authorized."));
        m_roleNotice->setStyleSheet("color: #1e40af; background-color: #dbeafe; border: 1px solid #bfdbfe; padding: 8px 12px; border-radius: 6px; font-weight: 500; font-size: 12px;");
        m_roleNotice->show();
    } else if (session.hasPermission(QStringLiteral("action:RunwayMaintenance"))) {
        // Airport Operator can only modify status
        m_numberEdit->setEnabled(false);
        m_typeCombo->setEnabled(false);
        m_statusCombo->setEnabled(true);
        // Airport operators can only set Available or Maintenance
        m_statusCombo->clear();
        m_statusCombo->addItems({
            QStringLiteral("Available"),
            QStringLiteral("Maintenance")
        });
        m_roleNotice->setText(QStringLiteral("🛠 Airport Operator Mode: Authorized to toggle Maintenance and operational availability."));
        m_roleNotice->setStyleSheet("color: #92400e; background-color: #fef3c7; border: 1px solid #fde68a; padding: 8px 12px; border-radius: 6px; font-weight: 500; font-size: 12px;");
        m_roleNotice->show();
    } else {
        // Read only
        m_numberEdit->setEnabled(false);
        m_typeCombo->setEnabled(false);
        m_statusCombo->setEnabled(false);
        m_saveButton->setEnabled(false);
        m_roleNotice->setText(QStringLiteral("👁 Read-Only Mode: You are viewing runway details with monitoring privileges."));
        m_roleNotice->setStyleSheet("color: #475569; background-color: #f1f5f9; border: 1px solid #e2e8f0; padding: 8px 12px; border-radius: 6px; font-weight: 500; font-size: 12px;");
        m_roleNotice->show();
    }
}

void RunwayDialog::setRunway(const Models::Runway &runway)
{
    m_runwayId = runway.id;
    m_numberEdit->setText(runway.runwayNumber);
    
    int typeIdx = m_typeCombo->findText(runway.runwayType, Qt::MatchFixedString);
    if (typeIdx >= 0) {
        m_typeCombo->setCurrentIndex(typeIdx);
    }

    int statusIdx = m_statusCombo->findText(runway.status, Qt::MatchFixedString);
    if (statusIdx >= 0) {
        m_statusCombo->setCurrentIndex(statusIdx);
    } else {
        // If status was Occupied and operator combo didn't have Occupied, add it as disabled
        m_statusCombo->addItem(runway.status);
        m_statusCombo->setCurrentText(runway.status);
    }
}

Models::Runway RunwayDialog::runway() const
{
    Models::Runway rw;
    rw.id = m_runwayId;
    rw.runwayNumber = m_numberEdit->text().trimmed().toUpper();
    rw.name = rw.runwayNumber;
    rw.runwayType = m_typeCombo->currentText().trimmed();
    rw.status = m_statusCombo->currentText().trimmed();
    return rw;
}

void RunwayDialog::onValidateAndAccept()
{
    m_errorLabel->hide();

    Models::Runway rw = runway();
    QString error;
    if (!rw.validate(&error)) {
        m_errorLabel->setText(error);
        m_errorLabel->show();
        return;
    }

    if (m_service && m_service->isAuthenticated()) {
        const Session &session = m_service->currentSession();
        if (session.isOperator() && !session.isAdmin()) {
            if (rw.status.compare(QStringLiteral("Occupied"), Qt::CaseInsensitive) == 0) {
                m_errorLabel->setText(QStringLiteral("Airport Operators cannot set status to Occupied. Occupancy is managed through flight allocation."));
                m_errorLabel->show();
                return;
            }
        }
    }

    accept();
}
