#include "AircraftDialog.h"

#include "services/ApplicationService.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

AircraftDialog::AircraftDialog(ApplicationService *service, Mode mode, QWidget *parent)
    : QDialog(parent)
    , m_service(service)
    , m_mode(mode)
{
    setWindowTitle(mode == Mode::Add ? QStringLiteral("Register New Aircraft") : QStringLiteral("Edit Aircraft Record"));
    setModal(true);
    setMinimumWidth(480);
    setStyleSheet(R"(
        QDialog {
            background-color: #f8fafc;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            font-size: 13px;
        }
        QGroupBox {
            font-weight: bold;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            margin-top: 12px;
            padding-top: 14px;
            background-color: #ffffff;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 14px;
            padding: 0 6px;
            color: #1e293b;
        }
        QLabel {
            color: #334155;
            font-weight: 500;
        }
        QLineEdit, QComboBox, QSpinBox {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 4px;
            padding: 6px 10px;
            color: #0f172a;
            selection-background-color: #3b82f6;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus {
            border: 1px solid #2563eb;
            background-color: #f0fdfa;
        }
        QLineEdit:disabled, QSpinBox:disabled {
            background-color: #f1f5f9;
            color: #64748b;
        }
        QPushButton#saveButton {
            background-color: #2563eb;
            color: #ffffff;
            font-weight: 600;
            border: none;
            border-radius: 5px;
            padding: 8px 18px;
        }
        QPushButton#saveButton:hover {
            background-color: #1d4ed8;
        }
        QPushButton#cancelButton {
            background-color: #e2e8f0;
            color: #334155;
            font-weight: 600;
            border: none;
            border-radius: 5px;
            padding: 8px 18px;
        }
        QPushButton#cancelButton:hover {
            background-color: #cbd5e1;
        }
    )");

    setupUi();
    applyRoleRestrictions();
}

void AircraftDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Header label
    QLabel *headerLabel = new QLabel(m_mode == Mode::Add ?
        QStringLiteral("🛩 Register Aircraft into Fleet") :
        QStringLiteral("🛩 Edit Aircraft Specifications"));
    headerLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #1e3a8a;");
    mainLayout->addWidget(headerLabel);

    // Role notice if applicable
    m_roleNotice = new QLabel(this);
    m_roleNotice->setStyleSheet("background-color: #fef3c7; color: #92400e; font-size: 11px; font-weight: 600; "
                                "padding: 6px 12px; border-radius: 6px; border: 1px solid #fde68a;");
    m_roleNotice->hide();
    mainLayout->addWidget(m_roleNotice);

    // Form inside GroupBox
    QGroupBox *infoGroup = new QGroupBox(QStringLiteral("Aircraft Fleet Details"), this);
    QFormLayout *formLayout = new QFormLayout(infoGroup);
    formLayout->setSpacing(12);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setContentsMargins(16, 20, 16, 16);

    // 1. Registration Number
    m_regEdit = new QLineEdit(this);
    m_regEdit->setPlaceholderText(QStringLiteral("e.g. VT-AT1 (max 20 chars, unique)"));
    m_regEdit->setMaxLength(20);
    formLayout->addRow(QStringLiteral("Registration Number:"), m_regEdit);

    // 2. Aircraft Type
    m_typeEdit = new QLineEdit(this);
    m_typeEdit->setPlaceholderText(QStringLiteral("e.g. Boeing 737-800 / Airbus A320neo"));
    m_typeEdit->setMaxLength(50);
    formLayout->addRow(QStringLiteral("Aircraft Type:"), m_typeEdit);

    // 3. Passenger Capacity
    m_capacitySpin = new QSpinBox(this);
    m_capacitySpin->setRange(1, 65535);
    m_capacitySpin->setValue(180);
    m_capacitySpin->setSuffix(QStringLiteral(" seats"));
    formLayout->addRow(QStringLiteral("Passenger Capacity:"), m_capacitySpin);

    // 4. Status
    m_statusCombo = new QComboBox(this);
    m_statusCombo->addItems({
        QStringLiteral("Active"),
        QStringLiteral("Maintenance"),
        QStringLiteral("Retired")
    });
    formLayout->addRow(QStringLiteral("Operational Status:"), m_statusCombo);

    mainLayout->addWidget(infoGroup);

    // Error label
    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet("color: #dc2626; font-size: 12px; font-weight: bold; padding: 4px;");
    m_errorLabel->setWordWrap(true);
    m_errorLabel->hide();
    mainLayout->addWidget(m_errorLabel);

    // Button Row
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();

    m_cancelButton = new QPushButton(QStringLiteral("Cancel"), this);
    m_cancelButton->setObjectName(QStringLiteral("cancelButton"));
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_cancelButton);

    m_saveButton = new QPushButton(m_mode == Mode::Add ? QStringLiteral("Register Aircraft") : QStringLiteral("Save Changes"), this);
    m_saveButton->setObjectName(QStringLiteral("saveButton"));
    connect(m_saveButton, &QPushButton::clicked, this, &AircraftDialog::onValidateAndAccept);
    buttonLayout->addWidget(m_saveButton);

    mainLayout->addLayout(buttonLayout);
}

void AircraftDialog::applyRoleRestrictions()
{
    if (!m_service) {
        return;
    }

    const Session &session = m_service->currentSession();

    // If Operator is editing: can only update operational status
    if (session.isOperator() && m_mode == Mode::Edit) {
        m_regEdit->setEnabled(false);
        m_typeEdit->setEnabled(false);
        m_capacitySpin->setEnabled(false);
        m_statusCombo->setEnabled(true);

        m_roleNotice->setText(QStringLiteral("ℹ Airport Operator mode: Operational status updates permitted. Registration and specifications are read-only."));
        m_roleNotice->show();
    }
}

void AircraftDialog::setAircraft(const Models::Aircraft &aircraft)
{
    m_aircraftId = aircraft.id;
    m_regEdit->setText(aircraft.effectiveRegistration());
    m_typeEdit->setText(aircraft.effectiveType());
    m_capacitySpin->setValue(aircraft.capacity);

    int idx = m_statusCombo->findText(aircraft.status);
    if (idx != -1) {
        m_statusCombo->setCurrentIndex(idx);
    }
}

Models::Aircraft AircraftDialog::aircraft() const
{
    Models::Aircraft ac;
    ac.id = m_aircraftId;
    ac.registrationNumber = m_regEdit->text().trimmed().toUpper();
    ac.registrationNo = ac.registrationNumber;
    ac.aircraftType = m_typeEdit->text().trimmed();
    ac.model = ac.aircraftType;
    ac.capacity = m_capacitySpin->value();
    ac.status = m_statusCombo->currentText();
    return ac;
}

void AircraftDialog::onValidateAndAccept()
{
    Models::Aircraft ac = aircraft();
    QString error;

    if (!ac.validate(&error)) {
        m_errorLabel->setText(QStringLiteral("⚠ %1").arg(error));
        m_errorLabel->show();
        return;
    }

    m_errorLabel->hide();
    accept();
}
