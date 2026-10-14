#include "FlightsView.h"

#include "services/ApplicationService.h"
#include "ui/FlightDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDateTime>

FlightsView::FlightsView(ApplicationService *service, QWidget *parent)
    : QWidget(parent)
    , m_service(service)
{
    setupUi();

    if (m_service) {
        connect(m_service, &ApplicationService::flightsChanged, this, &FlightsView::refreshFlights);
    }
}

void FlightsView::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(16);

    // ── 1. Header Row ────────────────────────────────────────────────────────
    QHBoxLayout *headerLayout = new QHBoxLayout;
    QVBoxLayout *titleTextLayout = new QVBoxLayout;

    QLabel *titleLabel = new QLabel(QStringLiteral("Flight Operations Management"), this);
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #0f172a;");

    QLabel *subLabel = new QLabel(QStringLiteral("Manage flight arrivals, departures, operational statuses, and schedules in MySQL"), this);
    subLabel->setStyleSheet("font-size: 13px; color: #64748b;");

    titleTextLayout->addWidget(titleLabel);
    titleTextLayout->addWidget(subLabel);
    headerLayout->addLayout(titleTextLayout);
    headerLayout->addStretch();

    m_countBadge = new QLabel(QStringLiteral("0 Flights"), this);
    m_countBadge->setStyleSheet("background-color: #e2e8f0; color: #334155; font-size: 12px; font-weight: 600; "
                               "padding: 6px 14px; border-radius: 12px; border: 1px solid #cbd5e1;");
    headerLayout->addWidget(m_countBadge);

    m_permNotice = new QLabel(this);
    m_permNotice->setStyleSheet("background-color: #fef3c7; color: #92400e; font-size: 11px; font-weight: 600; "
                               "padding: 6px 12px; border-radius: 12px; border: 1px solid #fde68a;");
    m_permNotice->setText(QStringLiteral("👁 Read-Only Mode"));
    m_permNotice->hide();
    headerLayout->addWidget(m_permNotice);

    mainLayout->addLayout(headerLayout);

    // ── 2. Toolbar Row ───────────────────────────────────────────────────────
    QHBoxLayout *toolbarLayout = new QHBoxLayout;
    toolbarLayout->setSpacing(10);

    // Search input
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("🔍 Search by Flight #, Airline, Route, or Aircraft..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumWidth(320);
    m_searchEdit->setStyleSheet(R"(
        QLineEdit {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 13px;
            color: #0f172a;
        }
        QLineEdit:focus {
            border: 1px solid #3b82f6;
            background-color: #f8fafc;
        }
    )");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &FlightsView::onSearchChanged);
    toolbarLayout->addWidget(m_searchEdit);

    // Status filter
    m_statusFilter = new QComboBox(this);
    m_statusFilter->addItem(QStringLiteral("All Statuses"));
    m_statusFilter->addItems({
        QStringLiteral("Scheduled"),
        QStringLiteral("Taxiing"),
        QStringLiteral("Cleared"),
        QStringLiteral("Landed"),
        QStringLiteral("Departed"),
        QStringLiteral("Cancelled")
    });
    m_statusFilter->setStyleSheet(R"(
        QComboBox {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 13px;
            color: #0f172a;
            min-width: 140px;
        }
    )");
    connect(m_statusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FlightsView::onStatusFilterChanged);
    toolbarLayout->addWidget(m_statusFilter);

    // Refresh button
    m_refreshButton = new QPushButton(QStringLiteral("🔄 Refresh"), this);
    m_refreshButton->setStyleSheet(R"(
        QPushButton {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 8px 14px;
            font-size: 13px;
            font-weight: 500;
            color: #334155;
        }
        QPushButton:hover {
            background-color: #f1f5f9;
            border-color: #94a3b8;
        }
    )");
    connect(m_refreshButton, &QPushButton::clicked, this, &FlightsView::refreshFlights);
    toolbarLayout->addWidget(m_refreshButton);

    toolbarLayout->addStretch();

    // Add button
    m_addButton = new QPushButton(QStringLiteral("➕ Add Flight"), this);
    m_addButton->setStyleSheet(R"(
        QPushButton {
            background-color: #16a34a;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #15803d;
        }
        QPushButton:disabled {
            background-color: #94a3b8;
        }
    )");
    connect(m_addButton, &QPushButton::clicked, this, &FlightsView::onAddFlightClicked);
    toolbarLayout->addWidget(m_addButton);

    // Edit button
    m_editButton = new QPushButton(QStringLiteral("✏ Edit"), this);
    m_editButton->setEnabled(false);
    m_editButton->setStyleSheet(R"(
        QPushButton {
            background-color: #2563eb;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #1d4ed8;
        }
        QPushButton:disabled {
            background-color: #cbd5e1;
            color: #94a3b8;
        }
    )");
    connect(m_editButton, &QPushButton::clicked, this, &FlightsView::onEditFlightClicked);
    toolbarLayout->addWidget(m_editButton);

    // Delete button
    m_deleteButton = new QPushButton(QStringLiteral("🗑 Delete"), this);
    m_deleteButton->setEnabled(false);
    m_deleteButton->setStyleSheet(R"(
        QPushButton {
            background-color: #dc2626;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #b91c1c;
        }
        QPushButton:disabled {
            background-color: #cbd5e1;
            color: #94a3b8;
        }
    )");
    connect(m_deleteButton, &QPushButton::clicked, this, &FlightsView::onDeleteFlightClicked);
    toolbarLayout->addWidget(m_deleteButton);

    mainLayout->addLayout(toolbarLayout);

    // ── 3. Content Area (Stacked: 0=Table, 1=Empty/Error State) ──────────────
    m_contentStack = new QStackedWidget(this);

    // Table view
    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(10);
    m_tableWidget->setHorizontalHeaderLabels({
        QStringLiteral("ID"),
        QStringLiteral("Flight #"),
        QStringLiteral("Airline"),
        QStringLiteral("Type"),
        QStringLiteral("Route"),
        QStringLiteral("Scheduled Time"),
        QStringLiteral("Priority"),
        QStringLiteral("Emergency"),
        QStringLiteral("Operational Status"),
        QStringLiteral("Assigned Aircraft")
    });

    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->setShowGrid(false);
    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(8, QHeaderView::ResizeToContents);

    m_tableWidget->setStyleSheet(R"(
        QTableWidget {
            background-color: #ffffff;
            border: 1px solid #e2e8f0;
            border-radius: 8px;
            gridline-color: #f1f5f9;
            font-size: 13px;
        }
        QTableWidget::item {
            padding: 8px 12px;
            color: #1e293b;
            border-bottom: 1px solid #f1f5f9;
        }
        QTableWidget::item:selected {
            background-color: #e0e7ff;
            color: #1e1b4b;
        }
        QHeaderView::section {
            background-color: #f8fafc;
            color: #475569;
            font-weight: 600;
            font-size: 12px;
            padding: 10px 12px;
            border: none;
            border-bottom: 2px solid #cbd5e1;
        }
    )");

    connect(m_tableWidget, &QTableWidget::itemSelectionChanged, this, &FlightsView::onSelectionChanged);
    connect(m_tableWidget, &QTableWidget::cellDoubleClicked, this, &FlightsView::onTableDoubleClicked);

    m_contentStack->addWidget(m_tableWidget);

    // Empty / Error State Widget
    m_stateWidget = new QWidget(this);
    QVBoxLayout *stateLayout = new QVBoxLayout(m_stateWidget);
    stateLayout->setAlignment(Qt::AlignCenter);

    m_stateIcon = new QLabel(QStringLiteral("✈"), this);
    m_stateIcon->setStyleSheet("font-size: 48px; color: #94a3b8;");
    m_stateIcon->setAlignment(Qt::AlignCenter);

    m_stateTitle = new QLabel(QStringLiteral("No Flights Found"), this);
    m_stateTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #334155; margin-top: 8px;");
    m_stateTitle->setAlignment(Qt::AlignCenter);

    m_stateMessage = new QLabel(QStringLiteral("No flights match your search criteria or the database is empty."), this);
    m_stateMessage->setStyleSheet("font-size: 13px; color: #64748b; margin-top: 4px;");
    m_stateMessage->setAlignment(Qt::AlignCenter);

    m_stateButton = new QPushButton(QStringLiteral("Add Flight"), this);
    m_stateButton->setStyleSheet("background-color: #2563eb; color: #ffffff; border-radius: 6px; padding: 8px 16px; margin-top: 14px; font-weight: 600;");
    connect(m_stateButton, &QPushButton::clicked, this, &FlightsView::onAddFlightClicked);

    stateLayout->addWidget(m_stateIcon);
    stateLayout->addWidget(m_stateTitle);
    stateLayout->addWidget(m_stateMessage);
    stateLayout->addWidget(m_stateButton);

    m_contentStack->addWidget(m_stateWidget);
    mainLayout->addWidget(m_contentStack);
}

void FlightsView::updatePermissions()
{
    if (!m_service) {
        return;
    }

    const Session &session = m_service->currentSession();
    bool canManage = session.hasPermission(QStringLiteral("action:ManageFlights"));

    m_addButton->setVisible(canManage);
    m_editButton->setVisible(canManage);
    m_deleteButton->setVisible(canManage);
    m_stateButton->setVisible(canManage);

    if (!canManage) {
        m_permNotice->show();
    } else {
        m_permNotice->hide();
    }

    onSelectionChanged();
}

void FlightsView::refreshFlights()
{
    if (!m_service) {
        return;
    }

    updatePermissions();

    QString search = m_searchEdit->text();
    QString status = m_statusFilter->currentText();
    QString err;

    m_currentFlights = m_service->getAllFlights(search, status, &err);

    if (!err.isEmpty() && m_currentFlights.isEmpty()) {
        m_stateIcon->setText(QStringLiteral("⚠"));
        m_stateTitle->setText(QStringLiteral("Database Error"));
        m_stateMessage->setText(err);
        m_stateButton->setText(QStringLiteral("Retry Connection"));
        m_contentStack->setCurrentWidget(m_stateWidget);
        m_countBadge->setText(QStringLiteral("Error"));
        m_countBadge->setStyleSheet("background-color: #fee2e2; color: #991b1b; font-size: 12px; font-weight: 600; padding: 6px 14px; border-radius: 12px;");
        return;
    }

    if (m_currentFlights.isEmpty()) {
        m_stateIcon->setText(QStringLiteral("✈"));
        m_stateTitle->setText(QStringLiteral("No Flights Found"));
        m_stateMessage->setText(search.isEmpty() ?
            QStringLiteral("No flights are currently recorded in MySQL. Click 'Add Flight' to create one.") :
            QStringLiteral("No flights matched your filter '%1'.").arg(search));
        m_stateButton->setText(QStringLiteral("➕ Add Flight"));
        m_contentStack->setCurrentWidget(m_stateWidget);
        m_countBadge->setText(QStringLiteral("0 Flights"));
        m_countBadge->setStyleSheet("background-color: #e2e8f0; color: #334155; font-size: 12px; font-weight: 600; padding: 6px 14px; border-radius: 12px;");
        return;
    }

    renderFlightsTable(m_currentFlights);
    m_contentStack->setCurrentWidget(m_tableWidget);
    m_countBadge->setText(QStringLiteral("%1 Flights").arg(m_currentFlights.size()));
    m_countBadge->setStyleSheet("background-color: #e0f2fe; color: #0369a1; font-size: 12px; font-weight: 600; padding: 6px 14px; border-radius: 12px; border: 1px solid #bae6fd;");
}

void FlightsView::renderFlightsTable(const QList<Models::Flight> &flights)
{
    m_tableWidget->setRowCount(0);
    m_tableWidget->setRowCount(flights.size());

    for (int row = 0; row < flights.size(); ++row) {
        const Models::Flight &f = flights[row];

        // 0: ID
        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(f.id));
        idItem->setTextAlignment(Qt::AlignCenter);

        // 1: Flight Number
        QTableWidgetItem *fnItem = new QTableWidgetItem(f.flightNumber);
        QFont boldFont = fnItem->font();
        boldFont.setBold(true);
        fnItem->setFont(boldFont);
        fnItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        // 2: Airline
        QTableWidgetItem *airItem = new QTableWidgetItem(f.airline);
        airItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        // 3: Flight Type
        QTableWidgetItem *typeItem = new QTableWidgetItem(f.flightType);
        typeItem->setTextAlignment(Qt::AlignCenter);

        // 4: Route
        QString routeStr = QStringLiteral("%1 → %2").arg(f.effectiveSource(), f.destination);
        QTableWidgetItem *routeItem = new QTableWidgetItem(routeStr);
        routeItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        // 5: Scheduled Time
        QString timeStr = f.scheduledTime.isValid() ? f.scheduledTime.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QStringLiteral("—");
        QTableWidgetItem *timeItem = new QTableWidgetItem(timeStr);
        timeItem->setTextAlignment(Qt::AlignCenter);

        // 6: Priority
        QString prioStr;
        switch (f.priority) {
            case 1: prioStr = QStringLiteral("P1 (Highest)"); break;
            case 2: prioStr = QStringLiteral("P2 (High)"); break;
            case 3: prioStr = QStringLiteral("P3 (Standard)"); break;
            case 4: prioStr = QStringLiteral("P4 (Low)"); break;
            case 5: prioStr = QStringLiteral("P5 (Lowest)"); break;
            default: prioStr = QStringLiteral("P%1").arg(f.priority); break;
        }
        QTableWidgetItem *prioItem = new QTableWidgetItem(prioStr);
        prioItem->setTextAlignment(Qt::AlignCenter);

        // 7: Emergency
        QTableWidgetItem *emergItem;
        if (f.isEmergency) {
            emergItem = new QTableWidgetItem(QStringLiteral("🚨 Emergency"));
            emergItem->setForeground(QColor(185, 28, 28)); // Red
            QFont f_emerg = emergItem->font();
            f_emerg.setBold(true);
            emergItem->setFont(f_emerg);
        } else {
            emergItem = new QTableWidgetItem(QStringLiteral("Normal"));
            emergItem->setForeground(QColor(100, 116, 139));
        }
        emergItem->setTextAlignment(Qt::AlignCenter);

        // 8: Operational Status
        QTableWidgetItem *statusItem = new QTableWidgetItem(f.operationalStatus);
        statusItem->setTextAlignment(Qt::AlignCenter);
        if (f.operationalStatus == QStringLiteral("Cleared") || f.operationalStatus == QStringLiteral("Landed")) {
            statusItem->setForeground(QColor(22, 101, 52)); // Green
        } else if (f.operationalStatus == QStringLiteral("Taxiing")) {
            statusItem->setForeground(QColor(180, 83, 9)); // Amber
        } else if (f.operationalStatus == QStringLiteral("Cancelled")) {
            statusItem->setForeground(QColor(185, 28, 28)); // Red
        } else {
            statusItem->setForeground(QColor(30, 41, 59));
        }

        // 9: Assigned Aircraft
        QTableWidgetItem *acItem = new QTableWidgetItem(f.aircraftRegistration.isEmpty() ? QStringLiteral("Unassigned") : f.aircraftRegistration);
        acItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        m_tableWidget->setItem(row, 0, idItem);
        m_tableWidget->setItem(row, 1, fnItem);
        m_tableWidget->setItem(row, 2, airItem);
        m_tableWidget->setItem(row, 3, typeItem);
        m_tableWidget->setItem(row, 4, routeItem);
        m_tableWidget->setItem(row, 5, timeItem);
        m_tableWidget->setItem(row, 6, prioItem);
        m_tableWidget->setItem(row, 7, emergItem);
        m_tableWidget->setItem(row, 8, statusItem);
        m_tableWidget->setItem(row, 9, acItem);
    }
}

void FlightsView::onSearchChanged(const QString &text)
{
    Q_UNUSED(text)
    refreshFlights();
}

void FlightsView::onStatusFilterChanged(int index)
{
    Q_UNUSED(index)
    refreshFlights();
}

void FlightsView::onSelectionChanged()
{
    bool hasSelection = (m_tableWidget->currentRow() >= 0);
    bool canManage = m_service && m_service->currentSession().hasPermission(QStringLiteral("action:ManageFlights"));

    m_editButton->setEnabled(hasSelection && canManage);
    m_deleteButton->setEnabled(hasSelection && canManage);
}

Models::Flight* FlightsView::getSelectedFlight()
{
    int row = m_tableWidget->currentRow();
    if (row < 0 || row >= m_currentFlights.size()) {
        return nullptr;
    }
    return &m_currentFlights[row];
}

void FlightsView::onAddFlightClicked()
{
    if (!m_service || !m_service->currentSession().hasPermission(QStringLiteral("action:ManageFlights"))) {
        QMessageBox::warning(this, QStringLiteral("Access Denied"),
                             QStringLiteral("You do not have permission to create flights."));
        return;
    }

    FlightDialog dialog(m_service, FlightDialog::Mode::Add, this);
    if (dialog.exec() == QDialog::Accepted) {
        Models::Flight newFlight = dialog.flight();
        QString error;
        if (m_service->createFlight(newFlight, &error)) {
            QMessageBox::information(this, QStringLiteral("Flight Created"),
                                     QStringLiteral("Flight %1 successfully created and persisted to database.").arg(newFlight.flightNumber));
            refreshFlights();
        } else {
            QMessageBox::critical(this, QStringLiteral("Error Creating Flight"),
                                  QStringLiteral("Could not create flight:\n%1").arg(error));
        }
    }
}

void FlightsView::onEditFlightClicked()
{
    Models::Flight *selected = getSelectedFlight();
    if (!selected) {
        return;
    }

    if (!m_service || !m_service->currentSession().hasPermission(QStringLiteral("action:ManageFlights"))) {
        QMessageBox::warning(this, QStringLiteral("Access Denied"),
                             QStringLiteral("You do not have permission to edit flights."));
        return;
    }

    FlightDialog dialog(m_service, FlightDialog::Mode::Edit, this);
    dialog.setFlight(*selected);

    if (dialog.exec() == QDialog::Accepted) {
        Models::Flight updatedFlight = dialog.flight();
        QString error;
        if (m_service->updateFlight(updatedFlight, &error)) {
            QMessageBox::information(this, QStringLiteral("Flight Updated"),
                                     QStringLiteral("Flight %1 successfully updated.").arg(updatedFlight.flightNumber));
            refreshFlights();
        } else {
            QMessageBox::critical(this, QStringLiteral("Error Updating Flight"),
                                  QStringLiteral("Could not update flight:\n%1").arg(error));
        }
    }
}

void FlightsView::onDeleteFlightClicked()
{
    Models::Flight *selected = getSelectedFlight();
    if (!selected) {
        return;
    }

    if (!m_service || !m_service->currentSession().hasPermission(QStringLiteral("action:ManageFlights"))) {
        QMessageBox::warning(this, QStringLiteral("Access Denied"),
                             QStringLiteral("You do not have permission to delete flights."));
        return;
    }

    // Confirmation dialog
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        QStringLiteral("Confirm Flight Deletion"),
        QStringLiteral("Are you sure you want to permanently delete flight '%1' (ID %2)?\n\n"
                       "This will remove the record from the database if no constraints prevent it.")
            .arg(selected->flightNumber, QString::number(selected->id)),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    QString error;
    bool success = m_service->deleteFlight(selected->id, &error);

    if (success) {
        QMessageBox::information(this, QStringLiteral("Flight Deleted"),
                                 QStringLiteral("Flight '%1' was successfully removed.").arg(selected->flightNumber));
        refreshFlights();
    } else {
        // Check if deletion was prevented by referential integrity
        if (error.contains(QStringLiteral("runway assignment"), Qt::CaseInsensitive) ||
            error.contains(QStringLiteral("referential integrity"), Qt::CaseInsensitive)) {
            QMessageBox::StandardButton cancelOpt = QMessageBox::question(
                this,
                QStringLiteral("Deletion Blocked by Database Constraints"),
                QStringLiteral("%1\n\nWould you like to cancel the flight instead by changing its status to 'Cancelled'?")
                    .arg(error),
                QMessageBox::Yes | QMessageBox::No
            );

            if (cancelOpt == QMessageBox::Yes) {
                Models::Flight cancelledFlight = *selected;
                cancelledFlight.operationalStatus = QStringLiteral("Cancelled");
                QString updateErr;
                if (m_service->updateFlight(cancelledFlight, &updateErr)) {
                    QMessageBox::information(this, QStringLiteral("Status Updated"),
                                             QStringLiteral("Flight '%1' operational status updated to 'Cancelled'.").arg(selected->flightNumber));
                    refreshFlights();
                } else {
                    QMessageBox::critical(this, QStringLiteral("Error"),
                                          QStringLiteral("Failed to update status: %1").arg(updateErr));
                }
            }
        } else {
            QMessageBox::critical(this, QStringLiteral("Delete Failed"),
                                  QStringLiteral("Could not delete flight:\n%1").arg(error));
        }
    }
}

void FlightsView::onTableDoubleClicked(int row, int column)
{
    Q_UNUSED(row)
    Q_UNUSED(column)

    if (m_service && m_service->currentSession().hasPermission(QStringLiteral("action:ManageFlights"))) {
        onEditFlightClicked();
    } else {
        Models::Flight *selected = getSelectedFlight();
        if (selected) {
            QMessageBox::information(this, QStringLiteral("Flight Details"),
                QStringLiteral("Flight Number: %1\nAirline: %2\nType: %3\nRoute: %4 → %5\nScheduled Time: %6\nPriority: %7\nEmergency: %8\nStatus: %9\nAircraft: %10")
                    .arg(selected->flightNumber,
                         selected->airline,
                         selected->flightType,
                         selected->effectiveSource(),
                         selected->destination,
                         selected->scheduledTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                         QString::number(selected->priority),
                         selected->isEmergency ? QStringLiteral("Yes") : QStringLiteral("No"),
                         selected->operationalStatus,
                         selected->aircraftRegistration)
            );
        }
    }
}
