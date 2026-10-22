#include "RunwayAllocationDialog.h"

#include "services/ApplicationService.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QGroupBox>
#include <QMessageBox>

RunwayAllocationDialog::RunwayAllocationDialog(ApplicationService *service, int preselectedRunwayId, QWidget *parent)
    : QDialog(parent)
    , m_service(service)
    , m_initialRunwayId(preselectedRunwayId)
{
    setWindowTitle(QStringLiteral("Allocate Runway Resource"));
    resize(720, 520);
    setStyleSheet(R"(
        QDialog {
            background-color: #ffffff;
        }
        QLabel {
            color: #1e293b;
            font-size: 13px;
        }
        QComboBox {
            background-color: #f8fafc;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 13px;
            color: #0f172a;
        }
        QComboBox:focus {
            border: 1px solid #2563eb;
            background-color: #ffffff;
        }
        QTableWidget {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            gridline-color: #f1f5f9;
            selection-background-color: #dbeafe;
            selection-color: #1e3a8a;
            font-size: 13px;
        }
        QHeaderView::section {
            background-color: #f8fafc;
            color: #475569;
            font-weight: 600;
            padding: 8px;
            border: none;
            border-bottom: 1px solid #cbd5e1;
        }
    )");

    setupUi();
    loadAvailableRunways();
    loadEligibleFlights();
}

void RunwayAllocationDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(14);

    // Header info banner
    QLabel *titleLabel = new QLabel(QStringLiteral("🛬 Runway Resource Allocation"), this);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #0f172a;");
    mainLayout->addWidget(titleLabel);

    QLabel *descLabel = new QLabel(
        QStringLiteral("Assign an available runway to an active simulated flight. Only available runways and eligible flights are shown."),
        this
    );
    descLabel->setStyleSheet("color: #64748b; font-size: 12px; margin-bottom: 4px;");
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);

    // Runway selection group
    QGroupBox *runwayGroup = new QGroupBox(QStringLiteral("1. Target Runway"), this);
    runwayGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #334155; border: 1px solid #e2e8f0; border-radius: 8px; padding-top: 16px; margin-top: 4px; }");
    QVBoxLayout *rwLayout = new QVBoxLayout(runwayGroup);
    rwLayout->setSpacing(8);

    QHBoxLayout *rwComboLayout = new QHBoxLayout();
    QLabel *rwLabel = new QLabel(QStringLiteral("Available Runway:"), runwayGroup);
    rwLabel->setFixedWidth(130);
    rwComboLayout->addWidget(rwLabel);

    m_runwayCombo = new QComboBox(runwayGroup);
    m_runwayCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_runwayCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RunwayAllocationDialog::onRunwayChanged);
    rwComboLayout->addWidget(m_runwayCombo);
    rwLayout->addLayout(rwComboLayout);

    m_runwayDetails = new QLabel(runwayGroup);
    m_runwayDetails->setStyleSheet("color: #475569; font-size: 12px; margin-left: 134px;");
    rwLayout->addWidget(m_runwayDetails);

    mainLayout->addWidget(runwayGroup);

    // Eligible flights group
    QGroupBox *flightsGroup = new QGroupBox(QStringLiteral("2. Select Eligible Flight Process"), this);
    flightsGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #334155; border: 1px solid #e2e8f0; border-radius: 8px; padding-top: 16px; margin-top: 4px; }");
    QVBoxLayout *fLayout = new QVBoxLayout(flightsGroup);
    fLayout->setSpacing(8);

    m_flightsTable = new QTableWidget(flightsGroup);
    m_flightsTable->setColumnCount(6);
    m_flightsTable->setHorizontalHeaderLabels({
        QStringLiteral("Flight #"),
        QStringLiteral("Airline"),
        QStringLiteral("Operation"),
        QStringLiteral("Priority"),
        QStringLiteral("Status"),
        QStringLiteral("Scheduled")
    });
    m_flightsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_flightsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_flightsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_flightsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_flightsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_flightsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_flightsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_flightsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_flightsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_flightsTable, &QTableWidget::itemSelectionChanged,
            this, &RunwayAllocationDialog::onFlightSelectionChanged);
    fLayout->addWidget(m_flightsTable);

    m_emptyNotice = new QLabel(QStringLiteral("No eligible flights available for this runway configuration."), flightsGroup);
    m_emptyNotice->setStyleSheet("color: #94a3b8; font-style: italic; padding: 12px; font-size: 13px;");
    m_emptyNotice->setAlignment(Qt::AlignCenter);
    m_emptyNotice->hide();
    fLayout->addWidget(m_emptyNotice);

    mainLayout->addWidget(flightsGroup);

    // Error feedback
    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet("color: #dc2626; font-size: 12px; font-weight: 500;");
    m_errorLabel->setWordWrap(true);
    m_errorLabel->hide();
    mainLayout->addWidget(m_errorLabel);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_cancelButton = new QPushButton(QStringLiteral("Cancel"), this);
    m_cancelButton->setStyleSheet(R"(
        QPushButton {
            background-color: #f1f5f9;
            color: #475569;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 8px 18px;
            font-size: 13px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #e2e8f0;
        }
    )");
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_cancelButton);

    m_allocateButton = new QPushButton(QStringLiteral("🚀 Confirm Allocation"), this);
    m_allocateButton->setEnabled(false);
    m_allocateButton->setStyleSheet(R"(
        QPushButton {
            background-color: #16a34a;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 20px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #15803d;
        }
        QPushButton:disabled {
            background-color: #cbd5e1;
            color: #94a3b8;
        }
    )");
    connect(m_allocateButton, &QPushButton::clicked, this, &RunwayAllocationDialog::onAllocateClicked);
    buttonLayout->addWidget(m_allocateButton);

    mainLayout->addLayout(buttonLayout);
}

void RunwayAllocationDialog::loadAvailableRunways()
{
    m_runwayCombo->clear();
    m_availableRunways.clear();

    if (!m_service) return;

    QString error;
    m_availableRunways = m_service->getAvailableRunwaysForAllocation(&error);

    if (m_availableRunways.isEmpty()) {
        m_runwayCombo->addItem(QStringLiteral("No Available Runways"));
        m_runwayCombo->setEnabled(false);
        m_runwayDetails->setText(QStringLiteral("All runways are currently Occupied or in Maintenance."));
        return;
    }

    int selectIndex = 0;
    for (int i = 0; i < m_availableRunways.size(); ++i) {
        const Models::Runway &rw = m_availableRunways[i];
        QString label = QStringLiteral("%1 (Permitted: %2)")
                            .arg(rw.runwayNumber, rw.runwayType);
        m_runwayCombo->addItem(label, rw.id);
        if (m_initialRunwayId > 0 && rw.id == m_initialRunwayId) {
            selectIndex = i;
        }
    }

    m_runwayCombo->setCurrentIndex(selectIndex);
    onRunwayChanged(selectIndex);
}

void RunwayAllocationDialog::loadEligibleFlights()
{
    m_flightsTable->setRowCount(0);
    m_eligibleFlights.clear();

    int rwId = selectedRunwayId();
    if (rwId <= 0 || !m_service) {
        m_emptyNotice->show();
        m_flightsTable->hide();
        m_allocateButton->setEnabled(false);
        return;
    }

    QString error;
    m_eligibleFlights = m_service->getEligibleFlightsForRunwayAllocation(rwId, &error);

    if (m_eligibleFlights.isEmpty()) {
        m_emptyNotice->show();
        m_flightsTable->hide();
        m_allocateButton->setEnabled(false);
        return;
    }

    m_emptyNotice->hide();
    m_flightsTable->show();
    m_flightsTable->setRowCount(m_eligibleFlights.size());

    for (int row = 0; row < m_eligibleFlights.size(); ++row) {
        const Models::Flight &f = m_eligibleFlights[row];

        // 0: Flight Number
        QString fnText = f.flightNumber;
        if (f.isEmergency) fnText += QStringLiteral(" 🚨");
        QTableWidgetItem *fnItem = new QTableWidgetItem(fnText);
        QFont fnFont = fnItem->font();
        fnFont.setBold(true);
        fnItem->setFont(fnFont);
        if (f.isEmergency) {
            fnItem->setForeground(QColor(QStringLiteral("#dc2626")));
        }
        m_flightsTable->setItem(row, 0, fnItem);

        // 1: Airline
        m_flightsTable->setItem(row, 1, new QTableWidgetItem(f.airline));

        // 2: Operation (Arrival / Departure)
        QTableWidgetItem *typeItem = new QTableWidgetItem(f.flightType);
        typeItem->setTextAlignment(Qt::AlignCenter);
        m_flightsTable->setItem(row, 2, typeItem);

        // 3: Priority
        QTableWidgetItem *prioItem = new QTableWidgetItem(QString::number(f.priority));
        prioItem->setTextAlignment(Qt::AlignCenter);
        if (f.priority == 1 || f.isEmergency) {
            prioItem->setForeground(QColor(QStringLiteral("#dc2626")));
        }
        m_flightsTable->setItem(row, 3, prioItem);

        // 4: Status
        QTableWidgetItem *statusItem = new QTableWidgetItem(f.operationalStatus);
        statusItem->setTextAlignment(Qt::AlignCenter);
        m_flightsTable->setItem(row, 4, statusItem);

        // 5: Scheduled Time
        QString timeStr = f.scheduledTime.isValid() ? f.scheduledTime.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QStringLiteral("—");
        m_flightsTable->setItem(row, 5, new QTableWidgetItem(timeStr));
    }

    m_flightsTable->selectRow(0);
}

void RunwayAllocationDialog::onRunwayChanged(int index)
{
    if (index < 0 || index >= m_availableRunways.size()) {
        m_runwayDetails->clear();
        return;
    }

    const Models::Runway &rw = m_availableRunways[index];
    m_runwayDetails->setText(QStringLiteral("Runway %1 is Available. Compatible with %2 flight operations.")
                                 .arg(rw.runwayNumber, rw.runwayType));
    loadEligibleFlights();
}

void RunwayAllocationDialog::onFlightSelectionChanged()
{
    m_allocateButton->setEnabled(selectedFlightId() > 0 && selectedRunwayId() > 0);
}

int RunwayAllocationDialog::selectedRunwayId() const
{
    if (m_runwayCombo && m_runwayCombo->currentIndex() >= 0) {
        return m_runwayCombo->currentData().toInt();
    }
    return 0;
}

int RunwayAllocationDialog::selectedFlightId() const
{
    int row = m_flightsTable->currentRow();
    if (row >= 0 && row < m_eligibleFlights.size()) {
        return m_eligibleFlights[row].id;
    }
    return 0;
}

void RunwayAllocationDialog::onAllocateClicked()
{
    int rwId = selectedRunwayId();
    int fId  = selectedFlightId();

    if (rwId <= 0 || fId <= 0) {
        m_errorLabel->setText(QStringLiteral("Please select both an available runway and an eligible flight."));
        m_errorLabel->show();
        return;
    }

    m_errorLabel->hide();

    QString error;
    Models::AllocationResult res = m_service->allocateRunway(rwId, fId, &error);

    if (res == Models::AllocationResult::Success) {
        accept();
    } else {
        QString msg = error.isEmpty() ? Models::allocationResultToString(res) : error;
        m_errorLabel->setText(QStringLiteral("Allocation failed: %1").arg(msg));
        m_errorLabel->show();
    }
}
