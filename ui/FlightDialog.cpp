#include "FlightDialog.h"

#include "services/ApplicationService.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>

FlightDialog::FlightDialog(ApplicationService *service, Mode mode, QWidget *parent)
    : QDialog(parent)
    , m_service(service)
    , m_mode(mode)
{
    setWindowTitle(mode == Mode::Add ? QStringLiteral("Create New Flight") : QStringLiteral("Edit Flight Details"));
    setModal(true);
    setMinimumWidth(520);
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
        QLineEdit, QComboBox, QDateTimeEdit, QSpinBox {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 4px;
            padding: 6px 10px;
            color: #0f172a;
            selection-background-color: #3b82f6;
        }
        QLineEdit:focus, QComboBox:focus, QDateTimeEdit:focus, QSpinBox:focus {
            border: 1px solid #2563eb;
            background-color: #f0fdfa;
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
    populateAircraftList();
}

void FlightDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Header label
    QLabel *headerLabel = new QLabel(m_mode == Mode::Add ?
        QStringLiteral("✈ Create New Flight Operation") :
        QStringLiteral("✈ Edit Existing Flight Operation"));
    headerLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #1e3a8a;");
    mainLayout->addWidget(headerLabel);

    // Form inside GroupBox
    QGroupBox *infoGroup = new QGroupBox(QStringLiteral("Flight Parameters & Schedule"), this);
    QFormLayout *formLayout = new QFormLayout(infoGroup);
    formLayout->setSpacing(12);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setContentsMargins(16, 20, 16, 16);

    // 1. Flight Number
    m_flightNumEdit = new QLineEdit(this);
    m_flightNumEdit->setPlaceholderText(QStringLiteral("e.g. AI-202 (max 10 chars)"));
    m_flightNumEdit->setMaxLength(10);
    formLayout->addRow(QStringLiteral("Flight Number:"), m_flightNumEdit);

    // 2. Airline
    m_airlineEdit = new QLineEdit(this);
    m_airlineEdit->setPlaceholderText(QStringLiteral("e.g. Air India"));
    m_airlineEdit->setMaxLength(100);
    formLayout->addRow(QStringLiteral("Airline:"), m_airlineEdit);

    // 3. Flight Type
    m_flightTypeCombo = new QComboBox(this);
    m_flightTypeCombo->addItem(QStringLiteral("Arrival"));
    m_flightTypeCombo->addItem(QStringLiteral("Departure"));
    formLayout->addRow(QStringLiteral("Flight Type:"), m_flightTypeCombo);

    // 4. Source & Destination
    QHBoxLayout *routeLayout = new QHBoxLayout;
    m_sourceEdit = new QLineEdit(this);
    m_sourceEdit->setPlaceholderText(QStringLiteral("Source (e.g. DEL)"));
    m_sourceEdit->setMaxLength(100);

    QLabel *arrowLabel = new QLabel(QStringLiteral("→"), this);
    arrowLabel->setStyleSheet("font-weight: bold; color: #64748b; font-size: 14px;");

    m_destinationEdit = new QLineEdit(this);
    m_destinationEdit->setPlaceholderText(QStringLiteral("Destination (e.g. BOM)"));
    m_destinationEdit->setMaxLength(100);

    routeLayout->addWidget(m_sourceEdit);
    routeLayout->addWidget(arrowLabel);
    routeLayout->addWidget(m_destinationEdit);
    formLayout->addRow(QStringLiteral("Route:"), routeLayout);

    // 5. Aircraft Assignment
    m_aircraftCombo = new QComboBox(this);
    formLayout->addRow(QStringLiteral("Assigned Aircraft:"), m_aircraftCombo);

    // 6. Scheduled Time
    m_scheduledTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime().addSecs(3600), this);
    m_scheduledTimeEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_scheduledTimeEdit->setCalendarPopup(true);
    formLayout->addRow(QStringLiteral("Scheduled Time:"), m_scheduledTimeEdit);

    // 7. Priority (1 to 5)
    m_prioritySpin = new QSpinBox(this);
    m_prioritySpin->setRange(1, 5);
    m_prioritySpin->setValue(3);
    m_prioritySpin->setToolTip(QStringLiteral("Priority scale: 1 (highest) to 5 (lowest)"));
    formLayout->addRow(QStringLiteral("Priority (1-5):"), m_prioritySpin);

    // 8. Emergency Flag
    m_emergencyCheck = new QCheckBox(QStringLiteral("Declare Emergency (immediate priority escalation)"), this);
    m_emergencyCheck->setStyleSheet("color: #b91c1c; font-weight: 600;");
    formLayout->addRow(QStringLiteral("Emergency:"), m_emergencyCheck);

    // 9. Operational Status
    m_statusCombo = new QComboBox(this);
    m_statusCombo->addItems({
        QStringLiteral("Scheduled"),
        QStringLiteral("Taxiing"),
        QStringLiteral("Cleared"),
        QStringLiteral("Landed"),
        QStringLiteral("Departed"),
        QStringLiteral("Cancelled")
    });
    formLayout->addRow(QStringLiteral("Operational Status:"), m_statusCombo);

    mainLayout->addWidget(infoGroup);

    // Error message label
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

    m_saveButton = new QPushButton(m_mode == Mode::Add ? QStringLiteral("Create Flight") : QStringLiteral("Save Changes"), this);
    m_saveButton->setObjectName(QStringLiteral("saveButton"));
    connect(m_saveButton, &QPushButton::clicked, this, &FlightDialog::onValidateAndAccept);
    buttonLayout->addWidget(m_saveButton);

    mainLayout->addLayout(buttonLayout);
}

void FlightDialog::populateAircraftList()
{
    m_aircraftCombo->clear();
    if (!m_service) {
        m_aircraftCombo->addItem(QStringLiteral("— None (Unassigned) —"), 0);
        return;
    }

    QString err;
    QList<QPair<int, QString>> aircraftList = m_service->getAvailableAircraft(&err);
    if (aircraftList.isEmpty()) {
        m_aircraftCombo->addItem(QStringLiteral("— None (Unassigned) —"), 0);
    } else {
        for (const auto &pair : aircraftList) {
            m_aircraftCombo->addItem(pair.second, pair.first);
        }
    }
}

void FlightDialog::setFlight(const Models::Flight &flight)
{
    m_flightId = flight.id;
    m_createdAt = flight.createdAt;

    m_flightNumEdit->setText(flight.flightNumber);
    m_airlineEdit->setText(flight.airline);
    m_sourceEdit->setText(flight.effectiveSource());
    m_destinationEdit->setText(flight.destination);

    int typeIdx = m_flightTypeCombo->findText(flight.flightType);
    if (typeIdx != -1) {
        m_flightTypeCombo->setCurrentIndex(typeIdx);
    }

    if (flight.scheduledTime.isValid()) {
        m_scheduledTimeEdit->setDateTime(flight.scheduledTime);
    }

    m_prioritySpin->setValue(flight.priority);
    m_emergencyCheck->setChecked(flight.isEmergency);

    int statusIdx = m_statusCombo->findText(flight.operationalStatus);
    if (statusIdx != -1) {
        m_statusCombo->setCurrentIndex(statusIdx);
    }

    int acIdx = m_aircraftCombo->findData(flight.aircraftId);
    if (acIdx != -1) {
        m_aircraftCombo->setCurrentIndex(acIdx);
    }
}

Models::Flight FlightDialog::flight() const
{
    Models::Flight f;
    f.id = m_flightId;
    f.flightNumber = m_flightNumEdit->text().trimmed();
    f.airline = m_airlineEdit->text().trimmed();
    f.source = m_sourceEdit->text().trimmed();
    f.origin = f.source;
    f.destination = m_destinationEdit->text().trimmed();
    f.flightType = m_flightTypeCombo->currentText();
    f.scheduledTime = m_scheduledTimeEdit->dateTime();
    f.priority = m_prioritySpin->value();
    f.isEmergency = m_emergencyCheck->isChecked();
    f.operationalStatus = m_statusCombo->currentText();
    f.aircraftId = m_aircraftCombo->currentData().toInt();
    f.createdAt = m_createdAt;
    return f;
}

void FlightDialog::onValidateAndAccept()
{
    Models::Flight f = flight();
    QString error;

    if (!f.validate(&error)) {
        m_errorLabel->setText(QStringLiteral("⚠ %1").arg(error));
        m_errorLabel->show();
        return;
    }

    m_errorLabel->hide();
    accept();
}
